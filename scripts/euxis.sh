#!/usr/bin/env bash
#
# Euxis - Multi-Provider AI Agent Framework
# Usage: euxis <agent> <task> [provider]
#
# Agents: architect, product-manager, bug-fixer, orchestrator,
#         edge-hunter, legacy-maintainer, release-manager
#
# Providers: claude (default), gemini, openai
#

set -euo pipefail

# ============================================================================
# Configuration
# ============================================================================

EUXIS_HOME="${HOME}/.euxis"
PROMPTS_DIR="${EUXIS_HOME}/prompts"
PROJECTS_DIR="${EUXIS_HOME}/projects"

# Default provider
DEFAULT_PROVIDER="claude"

# ============================================================================
# Functions
# ============================================================================

usage() {
    cat <<EOF
Euxis - Multi-Provider AI Agent Framework

Usage: euxis <agent> <task> [provider]

Arguments:
    agent       Agent to invoke (see list below)
    task        Task description or file path
    provider    AI provider: claude (default), gemini, openai

Available Agents:
    architect           Senior Architect - structure, patterns, refactoring
    product-manager     Technical PM - user stories, acceptance criteria
    bug-fixer           Debugging Specialist - root cause analysis
    orchestrator        Chief Planner - task breakdown, delegation
    edge-hunter         Red Team - security analysis, boundary testing
    legacy-maintainer   Legacy Specialist - documentation, upgrades
    release-manager     Release Lead - changelogs, versioning

Examples:
    euxis architect "Review the authentication module"
    euxis bug-fixer "Fix the null pointer exception in user.py" gemini
    euxis orchestrator "Plan the migration to microservices"

Environment:
    EUXIS_PROJECT       Project name (default: derived from current directory)
    EUXIS_SESSION_ID    Session identifier (default: timestamp)

EOF
    exit 1
}

log_info() {
    echo "[euxis] $*" >&2
}

log_error() {
    echo "[euxis] ERROR: $*" >&2
}

get_project_name() {
    if [[ -n "${EUXIS_PROJECT:-}" ]]; then
        echo "${EUXIS_PROJECT}"
    else
        basename "$(pwd)"
    fi
}

get_session_id() {
    if [[ -n "${EUXIS_SESSION_ID:-}" ]]; then
        echo "${EUXIS_SESSION_ID}"
    else
        date +"%Y%m%d-%H%M%S"
    fi
}

ensure_project_dirs() {
    local project="$1"
    local agent="$2"
    local agent_dir="${PROJECTS_DIR}/${project}/${agent}"

    mkdir -p "${agent_dir}"

    # Initialize files if they don't exist
    [[ -f "${agent_dir}/audit.md" ]] || echo "# Audit Log: ${agent}" > "${agent_dir}/audit.md"
    [[ -f "${agent_dir}/memory.md" ]] || echo "# Memory: ${agent}" > "${agent_dir}/memory.md"
}

get_memory_context() {
    local memory_file="$1"
    local lines="${2:-50}"

    if [[ -f "${memory_file}" ]]; then
        tail -n "${lines}" "${memory_file}" 2>/dev/null || true
    fi
}

prepare_prompt() {
    local agent="$1"
    local task="$2"
    local audit_path="$3"
    local memory_path="$4"
    local session_id="$5"
    local model_name="$6"

    local prompt_file="${PROMPTS_DIR}/${agent}.txt"

    if [[ ! -f "${prompt_file}" ]]; then
        log_error "Agent prompt not found: ${prompt_file}"
        exit 1
    fi

    # Load base prompt
    local prompt
    prompt=$(cat "${prompt_file}")

    # Replace variables
    prompt="${prompt//\{\{AUDIT_FILE_PATH\}\}/${audit_path}}"
    prompt="${prompt//\{\{MEMORY_FILE_PATH\}\}/${memory_path}}"
    prompt="${prompt//\{\{SESSION_ID\}\}/${session_id}}"
    prompt="${prompt//\{\{MODEL_NAME\}\}/${model_name}}"

    # Get memory context
    local memory_context
    memory_context=$(get_memory_context "${memory_path}" 50)

    # Build final prompt
    cat <<EOF
${prompt}

---
## MEMORY CONTEXT (Last 50 lines)
${memory_context}

---
## CURRENT TASK
${task}
EOF
}

run_claude() {
    local full_prompt="$1"
    echo "${full_prompt}" | claude -p
}

run_gemini() {
    local full_prompt="$1"
    # Assumes 'gemini' CLI is installed
    if command -v gemini &>/dev/null; then
        echo "${full_prompt}" | gemini prompt
    else
        log_error "Gemini CLI not found. Install it or use a different provider."
        exit 1
    fi
}

run_openai() {
    local full_prompt="$1"
    # Assumes 'sgpt' (shell-gpt) is installed
    if command -v sgpt &>/dev/null; then
        sgpt "${full_prompt}"
    else
        log_error "sgpt (shell-gpt) not found. Install it or use a different provider."
        exit 1
    fi
}

# ============================================================================
# Main
# ============================================================================

main() {
    # Parse arguments
    if [[ $# -lt 2 ]]; then
        usage
    fi

    local agent="$1"
    local task="$2"
    local provider="${3:-${DEFAULT_PROVIDER}}"

    # Validate agent
    local valid_agents=(
        "architect"
        "product-manager"
        "bug-fixer"
        "orchestrator"
        "edge-hunter"
        "legacy-maintainer"
        "release-manager"
    )

    local agent_valid=false
    for valid in "${valid_agents[@]}"; do
        if [[ "${agent}" == "${valid}" ]]; then
            agent_valid=true
            break
        fi
    done

    if [[ "${agent_valid}" != "true" ]]; then
        log_error "Unknown agent: ${agent}"
        echo "Valid agents: ${valid_agents[*]}" >&2
        exit 1
    fi

    # Setup paths
    local project
    project=$(get_project_name)
    local session_id
    session_id=$(get_session_id)

    ensure_project_dirs "${project}" "${agent}"

    local audit_path="${PROJECTS_DIR}/${project}/${agent}/audit.md"
    local memory_path="${PROJECTS_DIR}/${project}/${agent}/memory.md"

    # Determine model name based on provider
    local model_name
    case "${provider}" in
        claude)  model_name="Claude" ;;
        gemini)  model_name="Gemini" ;;
        openai)  model_name="GPT" ;;
        *)
            log_error "Unknown provider: ${provider}"
            echo "Valid providers: claude, gemini, openai" >&2
            exit 1
            ;;
    esac

    log_info "Agent: ${agent}"
    log_info "Project: ${project}"
    log_info "Provider: ${provider}"
    log_info "Session: ${session_id}"

    # Prepare full prompt with context
    local full_prompt
    full_prompt=$(prepare_prompt "${agent}" "${task}" "${audit_path}" "${memory_path}" "${session_id}" "${model_name}")

    # Execute with selected provider
    case "${provider}" in
        claude)  run_claude "${full_prompt}" ;;
        gemini)  run_gemini "${full_prompt}" ;;
        openai)  run_openai "${full_prompt}" ;;
    esac
}

main "$@"
