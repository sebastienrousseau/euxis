#!/usr/bin/env bash
#
# Euxis - Multi-Provider AI Agent Framework
# Usage: euxis <agent> <task> [provider]
#        euxis delegate <agent> <task> [provider]
#
# Agents are discovered dynamically from ~/.euxis/prompts/{core,fleet}/*.txt
# Providers: claude (default), gemini, openai, ollama, opencode, qwen, crush, kilo, amazon-q, goose
#

set -euo pipefail

# ============================================================================
# Configuration & Library Loading
# ============================================================================

EUXIS_HOME="${HOME}/.euxis"
PROMPTS_DIR="${EUXIS_HOME}/prompts"
PROJECTS_DIR="${EUXIS_HOME}/data/projects"
source "${EUXIS_HOME}/bin/lib/common.sh"
source "${EUXIS_HOME}/bin/lib/providers.sh"
source "${EUXIS_HOME}/bin/lib/agents.sh"
source "${EUXIS_HOME}/bin/lib/memory.sh"
source "${EUXIS_HOME}/bin/lib/session.sh"
source "${EUXIS_HOME}/bin/lib/template.sh"
source "${EUXIS_HOME}/bin/lib/skill-detector.sh"
source "${EUXIS_HOME}/bin/lib/prompt.sh"

# ============================================================================
# Fast Boot Health Check (hash-based, silent on pass)
# ============================================================================

check_health_fast() {
    # Skip in dispatch mode or when explicitly disabled
    [[ "${EUXIS_HEALTH_CHECK:-}" == "skip" ]] && return 0
    [[ "${EUXIS_DISPATCH:-}" == "true" ]] && return 0

    local hash_file="/tmp/euxis_health_hash"
    local current_hash
    current_hash=$(ls -lR "${PROMPTS_DIR}" 2>/dev/null | md5sum 2>/dev/null | awk '{print $1}')

    if [[ -f "${hash_file}" ]] && [[ "$(< "${hash_file}")" == "${current_hash}" ]]; then
        return 0
    fi

    local health_script="${EUXIS_HOME}/bin/euxis-health"
    if [[ -x "${health_script}" ]]; then
        if ! "${health_script}" --silent; then
            echo "[euxis] WARNING: Fleet integrity check failed. Run 'euxis-health' for details." >&2
        fi
    fi

    echo "${current_hash}" > "${hash_file}"
}

check_health_fast

# ============================================================================
# Context Display (PWD Beacon)
# ============================================================================

show_context() {
    # Skip context display for non-interactive/piped output
    [[ -t 1 ]] || return 0

    local repo_root branch relative_path
    repo_root=$(git rev-parse --show-toplevel 2>/dev/null || echo "$PWD")
    branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "no-git")
    relative_path=".${PWD#"$repo_root"}"

    local CYAN='\033[0;36m'
    local YELLOW='\033[1;33m'
    local NC='\033[0m'

    echo -e "Scope: ${CYAN}${repo_root##*/}${NC}/${relative_path}  Branch: ${YELLOW}${branch}${NC}"
    echo "---------------------------------------------------"
}

show_context

# ============================================================================
# Git Branch Guard (Feature Branch Enforcer)
# ============================================================================

git_guard() {
    [[ -t 0 ]] || return 0
    # Skip in dispatch mode (dispatched agents should not prompt for branch)
    [[ "${EUXIS_DISPATCH:-}" == "true" ]] && return 0

    local current_branch
    current_branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null) || return 0

    case "${current_branch}" in
        feat/*|fix/*|refactor/*|chore/*) return 0 ;;
    esac

    local protected="main master develop production staging"
    if [[ " ${protected} " == *" ${current_branch} "* ]]; then
        echo "[euxis] SAFETY STOP: You are on a protected branch ('${current_branch}')."

        local latest_tag base_ver major minor patch next_patch next_ver new_branch
        latest_tag=$(git describe --tags --abbrev=0 2>/dev/null || echo "v0.0.0")
        base_ver="${latest_tag#v}"
        IFS='.' read -r major minor patch <<< "${base_ver}"
        major="${major:-0}"; minor="${minor:-0}"; patch="${patch:-0}"
        next_patch=$((patch + 1))
        next_ver="v${major}.${minor}.${next_patch}"
        new_branch="feat/${next_ver}"

        echo "   Euxis must work in a feature branch."
        echo "   Proposed Branch: ${new_branch} (derived from ${latest_tag})"

        read -p "   Create and switch to '${new_branch}'? (y/n) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            git checkout -b "${new_branch}"
            echo "[euxis] Switched to ${new_branch}. Proceeding..."
        else
            echo "[euxis] Aborted. Please switch branches manually before running Euxis."
            exit 1
        fi
    else
        echo "[euxis] Warning: Non-standard branch name '${current_branch}'. Proceeding..."
    fi
}

git_guard

# ============================================================================
# Usage
# ============================================================================

usage() {
    cat <<EOF
Euxis - Multi-Provider AI Agent Framework

Usage: euxis <command> [args...]
       euxis <agent> <task> [provider]

Commands:
    dispatch <manifest>      Deploy fleet from architect manifest
    verify                   Run pre-commit quality gate (5 checks)
    health [--silent]        System health check
    lint                     Static analysis (registry, headers, versions)
    certify                  Full certification pipeline
    test                     Infrastructure unit tests
    squad <cmd> [args]       Squad management and deployment
    playbook <cmd> [args]    Phased squad execution via playbooks
    combo <cmd> [args]       Sequential agent chain execution
    council "<topic>"        Multi-agent adversarial debate
    loop <agent> ...         Autonomous retry loop
    bus <cmd> [args]         Async pub/sub message bus
    graph <cmd> [args]       GraphRAG knowledge graph
    cortex <cmd> [args]      Tri-typed vector memory
    voice                    Offline voice interface
    gym <agent> <test>       Agent evaluation and A/B testing
    audit                    Security audit with probes
    bench                    Performance benchmarking
    kaizen                   Continuous self-improvement cycle
    daemon [interval]        Periodic kaizen with fail-safe
    optimize                 System-wide tune-up
    sync-docs                Documentation sync with approval gate
    deploy                   Docker Compose enterprise deployment
    delegate <agent> ...     Invoke a sub-agent (used for chaining)

Agent Mode:
    euxis <agent> <task> [provider]

    provider    AI provider: claude, gemini, openai, ollama, opencode,
                qwen, crush, kilo, amazon-q, goose
                (auto-selected per agent via intelligence tiering if omitted)

Available Agents:
$(list_agents)

Examples:
    euxis architect "Review the authentication module"
    euxis dispatch plan.json
    euxis verify
    euxis squad list

Environment:
    EUXIS_PROJECT        Project name (default: derived from current directory)
    EUXIS_SESSION_ID     Session identifier (default: timestamp)
    EUXIS_OLLAMA_MODEL   Ollama model name (default: llama3.2)
    EUXIS_OPENCODE_MODEL OpenCode model name (default: codellama)

EOF
    exit 1
}

# ============================================================================
# Argument Parsing & Validation
# ============================================================================

parse_args() {
    if [[ $# -lt 2 ]]; then
        usage
    fi

    AGENT="$1"
    TASK="$2"
    PROVIDER="${3:-}"

    if [[ "${AGENT}" =~ [^a-zA-Z0-9_-] ]]; then
        log_error "Invalid agent name: ${AGENT} (only alphanumeric, hyphens, and underscores allowed)"
        exit 1
    fi

    local prompt_file
    prompt_file=$(resolve_agent_path "${AGENT}") || true
    if [[ -z "${prompt_file}" || "${AGENT}" == _* ]]; then
        log_error "Unknown agent: ${AGENT}"
        echo "Available agents:" >&2
        list_agents >&2
        exit 1
    fi

    if [[ -z "${PROVIDER}" ]]; then
        PROVIDER=$(resolve_tiered_provider "${AGENT}")
    fi

    case "${PROVIDER}" in
        claude|gemini|openai|ollama|opencode|qwen|crush|kilo|amazon-q|goose) ;;
        *)
            log_error "Unknown provider: ${PROVIDER}"
            echo "Valid providers: claude, gemini, openai, ollama, opencode, qwen, crush, kilo, amazon-q, goose" >&2
            exit 1
            ;;
    esac
}

# ============================================================================
# Session Setup
# ============================================================================

setup_session() {
    PROJECT=$(get_project_name)
    SESSION_ID=$(get_session_id)

    ensure_project_dirs "${PROJECT}" "${AGENT}"

    AUDIT_PATH="${PROJECTS_DIR}/${PROJECT}/${AGENT}/audit.md"
    MEMORY_PATH="${PROJECTS_DIR}/${PROJECT}/${AGENT}/memory.md"
    OUTPUT_PATH="${PROJECTS_DIR}/${PROJECT}/${AGENT}/output/${SESSION_ID}.md"

    resolve_provider_config "${PROVIDER}"
    MODEL_NAME="${PROVIDER_MODEL}"
}

# ============================================================================
# Output Capture
# ============================================================================

capture_output() {
    local agent="$1"
    local project="$2"
    local provider="$3"
    local task="$4"
    local session_id="$5"
    local output_path="$6"
    local output="$7"

    cat > "${output_path}" <<OUTEOF
# Output: ${agent} — ${session_id}
**Project:** ${project}
**Provider:** ${provider}
**Task:** ${task}

---

${output}
OUTEOF
}

# ============================================================================
# Main
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

    _t_prompt=$(_perf_start)
    local full_prompt
    full_prompt=$(prepare_prompt "${AGENT}" "${TASK}" "${AUDIT_PATH}" "${MEMORY_PATH}" "${SESSION_ID}" "${MODEL_NAME}")
    _ms_prompt=$(_perf_elapsed_ms "${_t_prompt}")
    _perf_record "prompt_assembly" "${_ms_prompt}" "${AGENT}"
    _perf_check_budget "${_ms_prompt}" "${EUXIS_PROMPT_BUDGET_MS:-500}" "prompt_assembly(${AGENT})" || true

    _t_provider=$(_perf_start)
    start_spinner "${AGENT} (${PROVIDER})..."
    local output
    output=$(execute_provider "${PROVIDER}" "${full_prompt}")
    stop_spinner
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

        if echo "${output}" | grep -q '```json'; then
            extracted=$(echo "${output}" | awk '/^```json$/{found=""; capture=1; next} /^```$/ && capture{capture=0} capture{found=found (found?"\n":"") $0} END{print found}')
        fi

        if [[ -z "$extracted" ]] && echo "${output}" | grep -q 'EUXIS_HANDOFF'; then
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
    if [[ $# -lt 2 ]]; then
        log_error "delegate requires: <agent> <task> [provider]"
        exit 1
    fi

    local sub_agent="$1"
    local sub_task="$2"
    local provider="${3:-$(resolve_tiered_provider "${sub_agent}")}"

    log_info "Delegating to ${sub_agent}..."

    EUXIS_PROJECT="${EUXIS_PROJECT:-$(get_project_name)}" \
        "$0" "${sub_agent}" "${sub_task}" "${provider}"
}

# ============================================================================
# Entry Point
# ============================================================================

EUXIS_BIN="${EUXIS_HOME}/bin"

case "${1:-}" in
    # Built-in
    delegate)   shift; delegate "$@" ;;
    # Slash commands (fast-path)
    slash)      shift; exec "${EUXIS_BIN}/euxis-slash" "$@" ;;
    /*)         exec "${EUXIS_BIN}/euxis-slash" "${1#/}" "${@:2}" ;;
    # Orchestration
    dispatch)   shift; exec "${EUXIS_BIN}/euxis-dispatch" "$@" ;;
    loop)       shift; exec "${EUXIS_BIN}/euxis-loop" "$@" ;;
    council)    shift; exec "${EUXIS_BIN}/euxis-council" "$@" ;;
    bus)        shift; exec "${EUXIS_BIN}/euxis-bus" "$@" ;;
    graph)      shift; exec "${EUXIS_BIN}/euxis-graph" "$@" ;;
    squad)      shift; exec "${EUXIS_BIN}/euxis-squad" "$@" ;;
    playbook)   shift; exec "${EUXIS_BIN}/euxis-playbook" "$@" ;;
    combo)      shift; exec "${EUXIS_BIN}/euxis-combo" "$@" ;;
    synthesize) shift; exec "${EUXIS_BIN}/euxis-synthesize" "$@" ;;
    codex)      shift; exec "${EUXIS_BIN}/euxis-codex" "$@" ;;
    hooks)      shift; exec "${EUXIS_BIN}/euxis-hooks" "$@" ;;
    # Quality
    verify)     shift; exec "${EUXIS_BIN}/euxis-verify" "$@" ;;
    health)     shift; exec "${EUXIS_BIN}/euxis-health" "$@" ;;
    lint)       shift; exec "${EUXIS_BIN}/euxis-lint" "$@" ;;
    certify)    shift; exec "${EUXIS_BIN}/euxis-certify" "$@" ;;
    test)       shift; exec "${EUXIS_BIN}/euxis-test-infra" "$@" ;;
    audit)      shift; exec "${EUXIS_BIN}/euxis-audit-run" "$@" ;;
    bench)      shift; exec "${EUXIS_BIN}/euxis-bench" "$@" ;;
    # Memory
    cortex)     shift; exec "${EUXIS_BIN}/euxis-cortex" "$@" ;;
    # Maintenance
    kaizen)     shift; exec "${EUXIS_BIN}/euxis-kaizen" "$@" ;;
    daemon)     shift; exec "${EUXIS_BIN}/euxis-daemon" "$@" ;;
    optimize)   shift; exec "${EUXIS_BIN}/euxis-optimize" "$@" ;;
    sync-docs)  shift; exec "${EUXIS_BIN}/euxis-sync-docs" "$@" ;;
    deploy)     shift; exec "${EUXIS_BIN}/euxis-deploy" "$@" ;;
    # Interface
    voice)      shift; exec "${EUXIS_BIN}/euxis-voice" "$@" ;;
    gym)        shift; exec "${EUXIS_BIN}/euxis-gym" "$@" ;;
    ui)         shift; exec "${EUXIS_BIN}/euxis-ui" "$@" ;;
    # Default: agent mode
    *)          main "$@" ;;
esac
