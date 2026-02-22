#!/usr/bin/env bash
#
# Euxis Dispatch Module - Command routing and main execution flow
# Part of the Euxis Enterprise Unified eXecution Intelligence System
#
[[ -n "${_EUXIS_LIB_DISPATCH:-}" ]] && return; _EUXIS_LIB_DISPATCH=1

set -euo pipefail

# ============================================================================
# Main Execution Function
# ============================================================================

main() {
    local _t_main _t_prompt _t_provider _ms_prompt _ms_provider _ms_total
    _t_main=$(_perf_start)

    parse_args "$@"
    setup_session

    # Lifecycle: mark agent active
    agent_lifecycle_transition "${AGENT}" "active" "${SESSION_ID}"

    log_info "Agent: ${AGENT}"
    log_info "Project: ${PROJECT}"
    log_info "Provider: ${PROVIDER}"
    log_info "Session: ${SESSION_ID}"
    log_info "Output: ${OUTPUT_PATH}"

    # Sanitize task input before prompt assembly (goal hijacking prevention)
    TASK=$(sanitize_task_input "${TASK}")

    _t_prompt=$(_perf_start)
    local full_prompt
    full_prompt=$(prepare_prompt "${AGENT}" "${TASK}" "${AUDIT_PATH}" "${MEMORY_PATH}" "${SESSION_ID}" "${MODEL_NAME}")
    _ms_prompt=$(_perf_elapsed_ms "${_t_prompt}")
    _perf_record "prompt_assembly" "${_ms_prompt}" "${AGENT}"
    _perf_check_budget "${_ms_prompt}" "${EUXIS_PROMPT_BUDGET_MS:-500}" "prompt_assembly(${AGENT})" || true

    _t_provider=$(_perf_start)
    start_spinner "${AGENT} (${PROVIDER})..."
    trap 'stop_spinner' EXIT
    local output
    if ! output=$(run_with_fallback "${PROVIDER}" "${full_prompt}" "${AGENT}" "${TASK}"); then
        stop_spinner
        trap - EXIT
        agent_lifecycle_transition "${AGENT}" "failed" "${SESSION_ID}"
        log_error "Agent execution failed for ${AGENT} (provider: ${PROVIDER})"
        exit 1
    fi
    stop_spinner
    trap - EXIT
    _ms_provider=$(_perf_elapsed_ms "${_t_provider}")
    _perf_record "provider_execution" "${_ms_provider}" "${AGENT}" "${PROVIDER}"

    _ms_total=$(_perf_elapsed_ms "${_t_main}")
    _perf_record "total_agent_run" "${_ms_total}" "${AGENT}"

    # Lifecycle: mark agent completed
    agent_lifecycle_transition "${AGENT}" "completed" "${SESSION_ID}"

    # Log coordination overhead (active agent count at time of execution)
    local active_count
    active_count=$(count_active_agents)
    if (( active_count > 1 )); then
        _perf_record "coordination_overhead" "${_ms_total}" "${AGENT}" "concurrent_agents=${active_count}"
    fi

    capture_output "${AGENT}" "${PROJECT}" "${PROVIDER}" "${TASK}" "${SESSION_ID}" "${OUTPUT_PATH}" "${output}"

    if [[ ! -t 1 ]]; then
        local extracted=""

        if [[ "${output}" == *'```json'* ]]; then
            extracted=$(echo "${output}" | awk '/^```json$/{found=""; capture=1; next} /^```$/ && capture{capture=0} capture{found=found (found?"\n":"") $0} END{print found}')
        fi

        if [[ -z "$extracted" ]] && [[ "${output}" == *'EUXIS_HANDOFF'* ]]; then
            local handoff
            handoff=$(echo "${output}" | sed -n '/<!-- EUXIS_HANDOFF/,/-->/p' | sed '1d;$d')
            if echo "$handoff" | jq -e 'has("dispatches")' &>/dev/null; then
                extracted="$handoff"
            fi
        fi

        if [[ -z "$extracted" ]] && command -v python3 &>/dev/null; then
            extracted=$(python3 -c "
import json, re, sys
text = sys.stdin.read()
for m in re.finditer(r'\{[^{}]*\"dispatches\"[^{}]*\[.*?\].*?\}', text, re.DOTALL):
    try:
        obj = json.loads(m.group())
        if 'dispatches' in obj:
            print(json.dumps(obj, indent=2))
            sys.exit(0)
    except json.JSONDecodeError:
        pass
" <<< "${output}" 2>/dev/null || true)
        fi

        if [[ -n "$extracted" ]] && echo "$extracted" | jq empty 2>/dev/null; then
            echo "$extracted"
        else
            echo "${output}"
        fi
    else
        echo "${output}"
    fi
}

# ============================================================================
# Delegate: invoke a sub-agent from the orchestrator (or any agent)
# ============================================================================

delegate() {
    local _t_delegate
    _t_delegate=$(_perf_start)

    if [[ $# -lt 2 ]]; then
        log_error "delegate requires: <agent> <task> [provider]"
        exit 1
    fi

    local sub_agent="$1"
    local sub_task="$2"
    local provider="${3:-$(resolve_tiered_provider "${sub_agent}")}"

    # Security validation: validate delegate arguments
    if [[ -z "$sub_agent" ]]; then
        _perf_record "delegate_error" "$(_perf_elapsed_ms "${_t_delegate}")" "system" "empty_agent"
        log_error "delegate requires non-empty agent name"
        exit 1
    fi

    if [[ -z "$sub_task" ]]; then
        _perf_record "delegate_error" "$(_perf_elapsed_ms "${_t_delegate}")" "system" "empty_task"
        log_error "delegate requires non-empty task"
        exit 1
    fi

    # Sanitize delegate task input (goal hijacking prevention)
    sub_task=$(sanitize_task_input "${sub_task}")

    _perf_record "delegate_start" "$(_perf_elapsed_ms "${_t_delegate}")" "${sub_agent}" "delegated"
    log_info "Delegating to ${sub_agent}..."

    EUXIS_PROJECT="${EUXIS_PROJECT:-$(get_project_name)}" \
        "$0" "${sub_agent}" "${sub_task}" "${provider}"
}

# ============================================================================
# Command Dispatcher - Route commands to appropriate modules
# ============================================================================

# Run a Python script through the project venv interpreter (for scripts needing
# venv-installed packages like chromadb, textual, etc.)
_exec_python() {
    local script="$1"; shift
    local venv_python="${EUXIS_HOME}/.venv/bin/python3"
    if [[ -x "${venv_python}" ]]; then
        exec "${venv_python}" "${script}" "$@"
    else
        log_error "Python venv not found at ${venv_python##*/} — required packages (chromadb, textual) may be missing"
        log_error "Run: python3 -m venv ${EUXIS_HOME}/.venv && ${EUXIS_HOME}/.venv/bin/pip install -r ${EUXIS_HOME}/requirements.txt"
        # Fall back to system python3 but only if it exists
        if command -v python3 &>/dev/null; then
            log_warn "Falling back to system python3 — some features may not work"
            exec python3 "${script}" "$@"
        else
            log_error "No python3 found. Install Python 3.12+ and create the venv."
            exit 1
        fi
    fi
}

dispatch_command() {
    local command="${1:-}"
    shift || true

    case "${command}" in
        # Built-in
        delegate)   delegate "$@" ;;
        # Slash commands (fast-path)
        slash)      exec "${EUXIS_BIN}/euxis-slash" "$@" ;;
        /*)         exec "${EUXIS_BIN}/euxis-slash" "${command#/}" "$@" ;;
        # Orchestration
        dispatch)   exec "${EUXIS_BIN}/euxis-dispatch" "$@" ;;
        agent)      exec "${EUXIS_BIN}/euxis-agent" "$@" ;;
        loop)       exec "${EUXIS_BIN}/euxis-loop" "$@" ;;
        council)    exec "${EUXIS_BIN}/euxis-council" "$@" ;;
        replay)     exec "${EUXIS_BIN}/euxis-replay" "$@" ;;
        bus)        exec "${EUXIS_BIN}/euxis-bus" "$@" ;;
        graph)      exec "${EUXIS_BIN}/euxis-graph" "$@" ;;
        squad)      exec "${EUXIS_BIN}/euxis-squad" "$@" ;;
        playbook)   exec "${EUXIS_BIN}/euxis-playbook" "$@" ;;
        combo)      exec "${EUXIS_BIN}/euxis-combo" "$@" ;;
        synthesize) _exec_python "${EUXIS_BIN}/euxis-synthesize" "$@" ;;
        codex)      exec "${EUXIS_BIN}/euxis-codex" "$@" ;;
        hooks)      exec "${EUXIS_BIN}/euxis-hooks" "$@" ;;
        # Quality
        verify)     exec "${EUXIS_BIN}/euxis-verify" "$@" ;;
        health)     exec "${EUXIS_BIN}/euxis-health" "$@" ;;
        lint)       exec "${EUXIS_BIN}/euxis-lint" "$@" ;;
        certify)    exec "${EUXIS_BIN}/euxis-certify" "$@" ;;
        test)       exec "${EUXIS_BIN}/euxis-test-infra" "$@" ;;
        audit)      exec "${EUXIS_BIN}/euxis-audit-run" "$@" ;;
        bench)      exec "${EUXIS_BIN}/euxis-bench" "$@" ;;
        # Memory
        cortex)     _exec_python "${EUXIS_BIN}/euxis-cortex" "$@" ;;
        context-worker) exec "${EUXIS_BIN}/euxis-context-worker" "$@" ;;
        # Maintenance
        kaizen)     exec "${EUXIS_BIN}/euxis-kaizen" "$@" ;;
        daemon)     exec "${EUXIS_BIN}/euxis-daemon" "$@" ;;
        optimize)   exec "${EUXIS_BIN}/euxis-optimize" "$@" ;;
        sync-docs)  exec "${EUXIS_BIN}/euxis-sync-docs" "$@" ;;
        deploy)     exec "${EUXIS_BIN}/euxis-deploy" "$@" ;;
        # Interface
        voice)      exec "${EUXIS_BIN}/euxis-voice" "$@" ;;
        gym)        exec "${EUXIS_BIN}/euxis-gym" "$@" ;;
        ui)         exec "${EUXIS_BIN}/euxis-ui" "$@" ;;
        wasm)       _exec_python "${EUXIS_BIN}/euxis-wasm" "$@" ;;
        # Default: agent mode
        *)          main "${command}" "$@" ;;
    esac
}
