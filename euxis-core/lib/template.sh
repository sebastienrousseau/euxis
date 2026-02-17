#!/usr/bin/env bash
# Euxis Library: Template variable substitution (pure bash — zero forks)
[[ -n "${_EUXIS_LIB_TEMPLATE:-}" ]] && return; _EUXIS_LIB_TEMPLATE=1

set -euo pipefail


EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"

# Pure-bash template expansion for {{VAR}} patterns.
# Uses parameter expansion instead of sed — no subprocess forks.
# Usage: template_substitute "$text" "$audit" "$memory" "$session" "$model"
template_substitute() {
    local text="$1"
    local audit_path="${2:-}"
    local memory_path="${3:-}"
    local session_id="${4:-}"
    local model_name="${5:-}"

    # Pure bash parameter expansion (no forks)
    text="${text//\{\{AUDIT_FILE_PATH\}\}/${audit_path}}"
    text="${text//\{\{MEMORY_FILE_PATH\}\}/${memory_path}}"
    text="${text//\{\{SESSION_ID\}\}/${session_id}}"
    text="${text//\{\{MODEL_NAME\}\}/${model_name}}"
    text="${text//\{\{EUXIS_HOME\}\}/${EUXIS_HOME}}"
    text="${text//\{\{PROMPTS_DIR\}\}/${EUXIS_HOME}/euxis-core/agents/prompts}"
    text="${text//\{\{PROJECTS_DIR\}\}/${EUXIS_HOME}/euxis-runtime/data/projects}"

    printf '%s' "${text}"
}

# Count approximate tokens in a string (1 token ≈ 4 chars)
estimate_tokens() {
    local text="$1"
    echo $(( ${#text} / 4 ))
}
