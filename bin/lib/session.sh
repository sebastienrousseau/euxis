#!/usr/bin/env bash
# (c) 2026 Euxis Fleet. All rights reserved.
# Euxis Library: Session and project management
[[ -n "${_EUXIS_LIB_SESSION:-}" ]] && return; _EUXIS_LIB_SESSION=1

set -euo pipefail


EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"
PROJECTS_DIR="${EUXIS_PROJECTS_DIR:-${EUXIS_HOME}/data/projects}"

get_project_name() {
    if [[ -n "${EUXIS_PROJECT:-}" ]]; then
        echo "${EUXIS_PROJECT}"
    else
        printf '%s' "${PWD##*/}"
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
    local base_dir="${PROJECTS_DIR}"

    if [[ ! -d "${base_dir}" ]]; then
        mkdir -p "${base_dir}" 2>/dev/null || true
    fi
    if [[ ! -w "${base_dir}" ]]; then
        base_dir="${EUXIS_PROJECTS_DIR:-/tmp/euxis/projects}"
        mkdir -p "${base_dir}" 2>/dev/null || true
    fi

    local agent_dir="${base_dir}/${project}/${agent}"

    if ! mkdir -p "${agent_dir}/output" 2>/dev/null; then
        base_dir="${EUXIS_PROJECTS_DIR:-/tmp/euxis/projects}"
        mkdir -p "${base_dir}" 2>/dev/null || true
        agent_dir="${base_dir}/${project}/${agent}"
        mkdir -p "${agent_dir}/output"
    fi

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
