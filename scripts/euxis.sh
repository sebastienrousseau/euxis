#!/usr/bin/env bash
#
# Euxis - Multi-Provider AI Agent Framework
# Usage: euxis <agent> <task> [provider]
#        euxis delegate <agent> <task> [provider]
#
# Agents are discovered dynamically from ~/.euxis/prompts/*.txt
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
# Utility Functions
# ============================================================================

list_agents() {
    local name
    for f in "${PROMPTS_DIR}"/*.txt; do
        name=$(basename "${f}" .txt)
        # Skip partials (files starting with _)
        [[ "${name}" == _* ]] && continue
        echo "    ${name}"
    done
}

usage() {
    cat <<EOF
Euxis - Multi-Provider AI Agent Framework

Usage: euxis <agent> <task> [provider]
       euxis delegate <agent> <task> [provider]

Arguments:
    agent       Agent to invoke (see list below)
    task        Task description or file path
    provider    AI provider: claude (default), gemini, openai

Subcommands:
    delegate    Invoke a sub-agent (used by orchestrator for chaining)

Available Agents:
$(list_agents)

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

# ============================================================================
# Context Functions
# ============================================================================

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

    mkdir -p "${agent_dir}/output"

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

# ============================================================================
# Prompt Assembly
# ============================================================================

prepare_prompt() {
    local agent="$1"
    local task="$2"
    local audit_path="$3"
    local memory_path="$4"
    local session_id="$5"
    local model_name="$6"

    local prompt_file="${PROMPTS_DIR}/${agent}.txt"
    local protocol_file="${PROMPTS_DIR}/_protocol.txt"

    if [[ ! -f "${prompt_file}" ]]; then
        log_error "Agent prompt not found: ${prompt_file}"
        exit 1
    fi

    # Load base prompt and append protocol partial
    local prompt
    prompt=$(cat "${prompt_file}")
    if [[ -f "${protocol_file}" ]]; then
        prompt="${prompt}"$'\n\n'"$(cat "${protocol_file}")"
    fi

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

# ============================================================================
# Provider Execution
# ============================================================================

run_claude() {
    local full_prompt="$1"
    echo "${full_prompt}" | claude -p
}

run_gemini() {
    local full_prompt="$1"
    if command -v gemini &>/dev/null; then
        echo "${full_prompt}" | gemini prompt
    else
        log_error "Gemini CLI not found. Install it or use a different provider."
        exit 1
    fi
}

run_openai() {
    local full_prompt="$1"
    if command -v sgpt &>/dev/null; then
        echo "${full_prompt}" | sgpt
    else
        log_error "sgpt (shell-gpt) not found. Install it or use a different provider."
        exit 1
    fi
}

execute_provider() {
    local provider="$1"
    local full_prompt="$2"

    case "${provider}" in
        claude)  run_claude "${full_prompt}" ;;
        gemini)  run_gemini "${full_prompt}" ;;
        openai)  run_openai "${full_prompt}" ;;
    esac
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
    PROVIDER="${3:-${DEFAULT_PROVIDER}}"

    # Sanitize agent name: reject characters outside [a-zA-Z0-9_-]
    if [[ "${AGENT}" =~ [^a-zA-Z0-9_-] ]]; then
        log_error "Invalid agent name: ${AGENT} (only alphanumeric, hyphens, and underscores allowed)"
        exit 1
    fi

    # Validate agent (dynamic discovery from prompts directory)
    local prompt_file="${PROMPTS_DIR}/${AGENT}.txt"
    if [[ ! -f "${prompt_file}" || "${AGENT}" == _* ]]; then
        log_error "Unknown agent: ${AGENT}"
        echo "Available agents:" >&2
        list_agents >&2
        exit 1
    fi

    # Validate provider
    case "${PROVIDER}" in
        claude|gemini|openai) ;;
        *)
            log_error "Unknown provider: ${PROVIDER}"
            echo "Valid providers: claude, gemini, openai" >&2
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

    # Determine model name based on provider
    case "${PROVIDER}" in
        claude)  MODEL_NAME="Claude" ;;
        gemini)  MODEL_NAME="Gemini" ;;
        openai)  MODEL_NAME="GPT" ;;
    esac
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
    # Phase 1: Parse and validate arguments
    parse_args "$@"

    # Phase 2: Setup session paths and directories
    setup_session

    # Phase 3: Log session metadata
    log_info "Agent: ${AGENT}"
    log_info "Project: ${PROJECT}"
    log_info "Provider: ${PROVIDER}"
    log_info "Session: ${SESSION_ID}"
    log_info "Output: ${OUTPUT_PATH}"

    # Phase 4: Assemble prompt with context
    local full_prompt
    full_prompt=$(prepare_prompt "${AGENT}" "${TASK}" "${AUDIT_PATH}" "${MEMORY_PATH}" "${SESSION_ID}" "${MODEL_NAME}")

    # Phase 5: Execute with selected provider
    local output
    output=$(execute_provider "${PROVIDER}" "${full_prompt}")

    # Phase 6: Capture output to session file
    capture_output "${AGENT}" "${PROJECT}" "${PROVIDER}" "${TASK}" "${SESSION_ID}" "${OUTPUT_PATH}" "${output}"

    # Phase 7: Print output to stdout
    echo "${output}"
}

# ============================================================================
# Delegate: invoke a sub-agent from the orchestrator (or any agent)
# Usage: euxis delegate <agent> <task> [provider]
# ============================================================================

delegate() {
    if [[ $# -lt 2 ]]; then
        log_error "delegate requires: <agent> <task> [provider]"
        exit 1
    fi

    local sub_agent="$1"
    local sub_task="$2"
    local provider="${3:-${DEFAULT_PROVIDER}}"

    log_info "Delegating to ${sub_agent}..."

    # Re-invoke euxis for the sub-agent (inherits EUXIS_PROJECT)
    EUXIS_PROJECT="${EUXIS_PROJECT:-$(get_project_name)}" \
        "$0" "${sub_agent}" "${sub_task}" "${provider}"
}

# ============================================================================
# Entry Point
# ============================================================================

case "${1:-}" in
    delegate) shift; delegate "$@" ;;
    *)        main "$@" ;;
esac
