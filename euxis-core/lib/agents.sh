#!/usr/bin/env bash
# Euxis Library: Agent discovery and resolution
[[ -n "${_EUXIS_LIB_AGENTS:-}" ]] && return; _EUXIS_LIB_AGENTS=1

set -euo pipefail


EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"
PROMPTS_DIR="${EUXIS_HOME}/euxis-core/agents/prompts"

# Source SQL registry functions if available
if [[ -f "${EUXIS_HOME}/euxis-core/lib/registry_sql.sh" ]]; then
    source "${EUXIS_HOME}/euxis-core/lib/registry_sql.sh"
    EUXIS_SQL_AVAILABLE=1
else
    EUXIS_SQL_AVAILABLE=0
fi

# ============================================================================
# Agent Discovery Functions (with SQL optimization)
# ============================================================================

resolve_agent_path() {
    local agent="$1"

    # Try SQL first if available (with connection pooling)
    if [[ "${EUXIS_SQL_AVAILABLE}" == "1" ]] && [[ "${EUXIS_FORCE_FILESYSTEM:-}" != "1" ]]; then
        local result
        result=$(resolve_agent_path_sql "${agent}" 2>/dev/null) && {
            echo "${result}"
            return 0
        }
    fi

    # Fallback to filesystem search
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
    # Try SQL first if available (much faster with large registries)
    if [[ "${EUXIS_SQL_AVAILABLE}" == "1" ]] && [[ "${EUXIS_FORCE_FILESYSTEM:-}" != "1" ]]; then
        local agents
        agents=$(list_agents_sql 2>/dev/null) && {
            echo "${agents}" | sed 's/^/    /'
            return 0
        }
    fi

    # Fallback to filesystem scan
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

# New optimized functions (only available with SQL backend)
find_agents_by_tag() {
    local tag="$1"
    [[ -z "${tag}" ]] && { log_error "find_agents_by_tag requires tag name"; return 1; }

    if [[ "${EUXIS_SQL_AVAILABLE}" == "1" ]]; then
        find_agents_by_tag_sql "${tag}"
    else
        log_error "find_agents_by_tag requires SQLite backend. Run: python3 migrate-registry-to-sqlite.py"
        return 1
    fi
}

find_agents_by_tier() {
    local tier="$1"
    [[ -z "${tier}" ]] && { log_error "find_agents_by_tier requires tier name"; return 1; }

    if [[ "${EUXIS_SQL_AVAILABLE}" == "1" ]]; then
        find_agents_by_tier_sql "${tier}"
    else
        log_error "find_agents_by_tier requires SQLite backend. Run: python3 migrate-registry-to-sqlite.py"
        return 1
    fi
}

find_agents_by_capability() {
    local capability="$1"
    [[ -z "${capability}" ]] && { log_error "find_agents_by_capability requires capability name"; return 1; }

    if [[ "${EUXIS_SQL_AVAILABLE}" == "1" ]]; then
        find_agents_by_capability_sql "${capability}"
    else
        log_error "find_agents_by_capability requires SQLite backend. Run: python3 migrate-registry-to-sqlite.py"
        return 1
    fi
}

# Performance-optimized agent search
search_agents() {
    local query_type="$1"
    shift

    case "${query_type}" in
        "all")
            list_agents ;;
        "tier")
            find_agents_by_tier "$1" ;;
        "tag")
            find_agents_by_tag "$1" ;;
        "capability")
            find_agents_by_capability "$1" ;;
        *)
            log_error "Unknown query type: ${query_type}"
            log_error "Valid types: all, tier, tag, capability"
            return 1
            ;;
    esac
}

# ============================================================================
# Agent Lifecycle Management
# Tracks agent state transitions: idle -> active -> completed/failed
# Enables coordination overhead measurement and dynamic health monitoring
# ============================================================================

EUXIS_LIFECYCLE_DIR="${EUXIS_HOME}/euxis-runtime/data/lifecycle"

# Initialize lifecycle tracking directory
_lifecycle_init() {
    if [[ ! -d "${EUXIS_LIFECYCLE_DIR}" ]]; then
        mkdir -p "${EUXIS_LIFECYCLE_DIR}" 2>/dev/null || true
    fi
    if ! touch "${EUXIS_LIFECYCLE_DIR}/.write-test" 2>/dev/null; then
        EUXIS_LIFECYCLE_DIR="${TMPDIR:-/tmp}/euxis/lifecycle"
        mkdir -p "${EUXIS_LIFECYCLE_DIR}" 2>/dev/null || true
    else
        rm -f "${EUXIS_LIFECYCLE_DIR}/.write-test" 2>/dev/null || true
    fi
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

EUXIS_PLUGINS_DIR="${EUXIS_HOME}/euxis-core/config/plugins"

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

    local agent_id prompt_file tier
    agent_id=$(jq -r '.agent_id' "${manifest}")
    prompt_file=$(jq -r '.prompt_file' "${manifest}")
    tier=$(jq -r '.tier // "standard"' "${manifest}")

    # Validate required fields
    if [[ -z "${agent_id}" || "${agent_id}" == "null" ]]; then
        log_error "Plugin manifest missing 'agent_id'"
        return 1
    fi

    # Security: validate agent_id against path traversal (alphanumeric, dash, underscore only)
    if [[ ! "${agent_id}" =~ ^[a-zA-Z0-9_-]+$ ]]; then
        log_error "Invalid agent_id '${agent_id}' — must be alphanumeric, dash, or underscore only"
        return 1
    fi

    if [[ -z "${prompt_file}" || "${prompt_file}" == "null" || ! -f "${prompt_file}" ]]; then
        log_error "Plugin manifest 'prompt_file' not found: ${prompt_file}"
        return 1
    fi

    # Security: resolve prompt_file to real path and verify it doesn't escape EUXIS_HOME
    local real_prompt
    real_prompt=$(realpath "${prompt_file}" 2>/dev/null) || {
        log_error "Cannot resolve prompt_file path: ${prompt_file}"
        return 1
    }
    local real_euxis
    real_euxis=$(realpath "${EUXIS_HOME}" 2>/dev/null) || real_euxis="${EUXIS_HOME}"
    if [[ "${real_prompt}" != "${real_euxis}"/* ]]; then
        log_error "prompt_file must be within EUXIS_HOME: ${prompt_file} resolves outside ${EUXIS_HOME}"
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

    # Security: validate agent_id against path traversal
    if [[ ! "${agent_id}" =~ ^[a-zA-Z0-9_-]+$ ]]; then
        log_error "Invalid agent_id '${agent_id}' — must be alphanumeric, dash, or underscore only"
        return 1
    fi

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
    source "${EUXIS_HOME}/euxis-core/lib/providers.sh" 2>/dev/null || true
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
