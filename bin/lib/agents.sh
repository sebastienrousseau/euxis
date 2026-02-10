#!/usr/bin/env bash
# (c) 2026 Euxis Fleet. All rights reserved.
# Euxis Library: Agent discovery and resolution
[[ -n "${_EUXIS_LIB_AGENTS:-}" ]] && return; _EUXIS_LIB_AGENTS=1

EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"
PROMPTS_DIR="${EUXIS_HOME}/prompts"

resolve_agent_path() {
    local agent="$1"
    for dir in "${PROMPTS_DIR}/core" "${PROMPTS_DIR}/fleet"; do
        if [[ -f "${dir}/${agent}.txt" ]]; then
            echo "${dir}/${agent}.txt"
            return 0
        fi
    done
    echo ""
    return 1
}

list_agents() {
    local name
    for dir in "${PROMPTS_DIR}/core" "${PROMPTS_DIR}/fleet"; do
        for f in "${dir}"/*.txt; do
            [[ -f "${f}" ]] || continue
            name="${f##*/}"; name="${name%.txt}"
            [[ "${name}" == _* ]] && continue
            echo "    ${name}"
        done
    done
}

# ============================================================================
# Agent Lifecycle Management
# Tracks agent state transitions: idle -> active -> completed/failed
# Enables coordination overhead measurement and dynamic health monitoring
# ============================================================================

EUXIS_LIFECYCLE_DIR="${EUXIS_HOME}/data/lifecycle"

# Initialize lifecycle tracking directory
_lifecycle_init() {
    [[ -d "${EUXIS_LIFECYCLE_DIR}" ]] || mkdir -p "${EUXIS_LIFECYCLE_DIR}"
}

# Record agent state transition
# Usage: agent_lifecycle_transition "architect" "active" "session-123"
agent_lifecycle_transition() {
    local agent="$1"
    local state="$2"  # idle | active | completed | failed | timeout
    local session="${3:-$(date +%Y%m%d-%H%M%S)}"

    _lifecycle_init

    local state_file="${EUXIS_LIFECYCLE_DIR}/${agent}.state"
    local log_file="${EUXIS_LIFECYCLE_DIR}/transitions.jsonl"

    # Write current state atomically
    printf '%s' "${state}" > "${state_file}"

    # Append transition log
    printf '{"ts":"%s","agent":"%s","state":"%s","session":"%s"}\n' \
        "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
        "${agent}" \
        "${state}" \
        "${session}" \
        >> "${log_file}"
}

# Get current agent state (returns "idle" if no state file)
agent_get_state() {
    local agent="$1"
    local state_file="${EUXIS_LIFECYCLE_DIR}/${agent}.state"

    if [[ -f "${state_file}" ]]; then
        cat "${state_file}"
    else
        echo "idle"
    fi
}

# List all active agents
list_active_agents() {
    _lifecycle_init
    local agent state
    for state_file in "${EUXIS_LIFECYCLE_DIR}"/*.state; do
        [[ -f "${state_file}" ]] || continue
        state=$(< "${state_file}")
        if [[ "${state}" == "active" ]]; then
            agent="${state_file##*/}"; agent="${agent%.state}"
            echo "${agent}"
        fi
    done
}

# Count active agents (for coordination overhead measurement)
count_active_agents() {
    _lifecycle_init
    local count=0
    local state_file
    for state_file in "${EUXIS_LIFECYCLE_DIR}"/*.state; do
        [[ -f "${state_file}" ]] || continue
        [[ "$(< "${state_file}")" == "active" ]] && (( count++ ))
    done
    echo "${count}"
}

# Clean stale agent states (agents stuck in "active" beyond timeout)
# Default timeout: 30 minutes
cleanup_stale_agents() {
    local timeout_seconds="${1:-1800}"
    _lifecycle_init
    local now state_file agent mtime age
    now=$(date +%s)

    for state_file in "${EUXIS_LIFECYCLE_DIR}"/*.state; do
        [[ -f "${state_file}" ]] || continue
        [[ "$(< "${state_file}")" != "active" ]] && continue

        mtime=$(stat -c '%Y' "${state_file}" 2>/dev/null || stat -f '%m' "${state_file}" 2>/dev/null)
        age=$(( now - mtime ))

        if (( age > timeout_seconds )); then
            agent="${state_file##*/}"; agent="${agent%.state}"
            agent_lifecycle_transition "${agent}" "timeout"
            log_warn "Agent ${agent} timed out after ${age}s (threshold: ${timeout_seconds}s)"
        fi
    done
}

# ============================================================================
# Agent Registration & Plugin API
# Third-party agents can register via a simple manifest file
# ============================================================================

EUXIS_PLUGINS_DIR="${EUXIS_HOME}/config/plugins"

# Register a third-party agent from a plugin manifest
# Manifest format: JSON with {agent_id, role, prompt_file, tier, tags}
register_agent_plugin() {
    local manifest="$1"

    if [[ ! -f "${manifest}" ]]; then
        log_error "Plugin manifest not found: ${manifest}"
        return 1
    fi

    if ! command -v jq &>/dev/null; then
        log_error "jq is required for plugin registration"
        return 1
    fi

    local agent_id role prompt_file tier
    agent_id=$(jq -r '.agent_id' "${manifest}")
    role=$(jq -r '.role' "${manifest}")
    prompt_file=$(jq -r '.prompt_file' "${manifest}")
    tier=$(jq -r '.tier // "standard"' "${manifest}")

    # Validate required fields
    if [[ -z "${agent_id}" || "${agent_id}" == "null" ]]; then
        log_error "Plugin manifest missing 'agent_id'"
        return 1
    fi
    if [[ -z "${prompt_file}" || "${prompt_file}" == "null" || ! -f "${prompt_file}" ]]; then
        log_error "Plugin manifest 'prompt_file' not found: ${prompt_file}"
        return 1
    fi

    # Install: symlink prompt into fleet directory
    [[ -d "${EUXIS_PLUGINS_DIR}" ]] || mkdir -p "${EUXIS_PLUGINS_DIR}"
    ln -sf "${prompt_file}" "${PROMPTS_DIR}/fleet/${agent_id}.txt"

    # Record plugin metadata
    cp "${manifest}" "${EUXIS_PLUGINS_DIR}/${agent_id}.json"

    log_info "Registered plugin agent: ${agent_id} (tier: ${tier})"
}

# Unregister a plugin agent
unregister_agent_plugin() {
    local agent_id="$1"

    rm -f "${PROMPTS_DIR}/fleet/${agent_id}.txt"
    rm -f "${EUXIS_PLUGINS_DIR}/${agent_id}.json"

    log_info "Unregistered plugin agent: ${agent_id}"
}

# List registered plugins
list_plugins() {
    [[ -d "${EUXIS_PLUGINS_DIR}" ]] || return 0
    local manifest agent_id
    for manifest in "${EUXIS_PLUGINS_DIR}"/*.json; do
        [[ -f "${manifest}" ]] || continue
        agent_id="${manifest##*/}"; agent_id="${agent_id%.json}"
        echo "    ${agent_id} (plugin)"
    done
}

# ============================================================================
# Agent Health Probes
# Lightweight liveness/readiness checks for individual agents
# ============================================================================

# Liveness probe: checks if agent prompt exists and is readable
agent_probe_liveness() {
    local agent="$1"
    local path
    path=$(resolve_agent_path "${agent}" 2>/dev/null) || true
    if [[ -n "${path}" && -f "${path}" && -r "${path}" ]]; then
        echo "live"
        return 0
    else
        echo "dead"
        return 1
    fi
}

# Readiness probe: checks if agent has required frontmatter and sections
agent_probe_readiness() {
    local agent="$1"
    local path
    path=$(resolve_agent_path "${agent}" 2>/dev/null) || true

    if [[ -z "${path}" || ! -f "${path}" ]]; then
        echo "not_ready:missing_prompt"
        return 1
    fi

    # Check frontmatter
    local first_line
    first_line=$(head -n 1 "${path}")
    if [[ "${first_line}" != "---" ]]; then
        echo "not_ready:no_frontmatter"
        return 1
    fi

    # Check required sections
    if ! grep -qi 'mandate' "${path}"; then
        echo "not_ready:missing_mandate"
        return 1
    fi
    if ! grep -qi 'output format' "${path}"; then
        echo "not_ready:missing_output_format"
        return 1
    fi

    # Check provider is available
    source "${EUXIS_HOME}/bin/lib/providers.sh" 2>/dev/null || true
    local provider
    provider=$(resolve_tiered_provider "${agent}" 2>/dev/null)
    case "${provider}" in
        claude)   command -v claude &>/dev/null || { echo "not_ready:provider_missing(claude)"; return 1; } ;;
        gemini)   command -v gemini &>/dev/null || { echo "not_ready:provider_missing(gemini)"; return 1; } ;;
        ollama)   command -v ollama &>/dev/null || { echo "not_ready:provider_missing(ollama)"; return 1; } ;;
        goose)    command -v goose &>/dev/null || { echo "not_ready:provider_missing(goose)"; return 1; } ;;
    esac

    echo "ready"
    return 0
}

# Full health report for all agents
agent_health_report() {
    local total=0 live=0 ready=0
    local agent name

    printf '%-30s %-8s %-30s\n' "AGENT" "LIVE" "READY"
    printf '%-30s %-8s %-30s\n' "-----" "----" "-----"

    for dir in "${PROMPTS_DIR}/core" "${PROMPTS_DIR}/fleet"; do
        for f in "${dir}"/*.txt; do
            [[ -f "${f}" ]] || continue
            name="${f##*/}"; name="${name%.txt}"
            [[ "${name}" == _* ]] && continue
            (( total++ ))

            local liveness readiness
            liveness=$(agent_probe_liveness "${name}")
            readiness=$(agent_probe_readiness "${name}")

            [[ "${liveness}" == "live" ]] && (( live++ ))
            [[ "${readiness}" == "ready" ]] && (( ready++ ))

            printf '%-30s %-8s %-30s\n' "${name}" "${liveness}" "${readiness}"
        done
    done

    echo ""
    echo "Total: ${total} | Live: ${live} | Ready: ${ready}"
}
