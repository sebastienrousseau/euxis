#!/usr/bin/env bash
#
# Euxis CLI Module - Command-line interface and user interaction
# Part of the Euxis Multi-Provider AI Agent Framework
#
[[ -n "${_EUXIS_LIB_CLI:-}" ]] && return; _EUXIS_LIB_CLI=1

set -euo pipefail

# Source validation library for security checks
source "${EUXIS_HOME}/bin/lib/validation.sh"

# ============================================================================
# Usage Information
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
    local _t_parse
    _t_parse=$(_perf_start)

    if [[ $# -lt 2 ]]; then
        usage
    fi

    AGENT="$1"
    TASK="$2"
    PROVIDER="${3:-}"

    # Security validation: validate agent name and task input
    if ! validate_agent_name "${AGENT}"; then
        _perf_record "cli_error" "$(_perf_elapsed_ms "${_t_parse}")" "cli" "invalid_agent_name"
        exit 1
    fi

    if ! validate_task_input "${TASK}"; then
        _perf_record "cli_error" "$(_perf_elapsed_ms "${_t_parse}")" "cli" "invalid_task_input"
        exit 1
    fi

    local prompt_file
    prompt_file=$(resolve_agent_path "${AGENT}") || true
    if [[ -z "${prompt_file}" || "${AGENT}" == _* ]]; then
        _perf_record "cli_error" "$(_perf_elapsed_ms "${_t_parse}")" "cli" "unknown_agent:${AGENT}"
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