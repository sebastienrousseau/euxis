#!/usr/bin/env bash
# (c) 2026 Euxis Fleet. All rights reserved.
# Euxis Library: Prompt assembly with conditional protocol loading, caching, and token budgets
[[ -n "${_EUXIS_LIB_PROMPT:-}" ]] && return; _EUXIS_LIB_PROMPT=1

set -euo pipefail


EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"
PROMPTS_DIR="${EUXIS_HOME}/prompts"

source "${EUXIS_HOME}/core/lib/agents.sh"
source "${EUXIS_HOME}/core/lib/memory.sh"
source "${EUXIS_HOME}/core/lib/template.sh"

# Max token budget for protocol loading (prevents context bloat)
EUXIS_PROTOCOL_TOKEN_BUDGET="${EUXIS_PROTOCOL_TOKEN_BUDGET:-12000}"

# Protocol cache TTL in seconds (skip re-reading files within window)
_EUXIS_PROTO_CACHE_TTL="${_EUXIS_PROTO_CACHE_TTL:-60}"

# Session-level caches (persist within same shell invocation)
_EUXIS_PROTO_CACHE=""
_EUXIS_PROTO_CACHE_KEY=""
_EUXIS_REGISTRY_CACHE=""
_EUXIS_REGISTRY_MTIME=""

# ============================================================================
# Keyword fingerprint: extract sorted unique keywords for cache key
# ============================================================================

_proto_fingerprint() {
    local task_lower="${1,,}"
    # Extract words that match any conditional keyword, sort for deterministic key
    local fp=""
    [[ "${task_lower}" =~ security|auth|cred|secret|pii ]] && fp="${fp}sec,"
    [[ "${task_lower}" =~ version|release|bump|semver ]] && fp="${fp}ver,"
    [[ "${task_lower}" =~ escalat|incident|sev|outage ]] && fp="${fp}esc,"
    [[ "${task_lower}" =~ evidence|citation|source|fact ]] && fp="${fp}evi,"
    [[ "${task_lower}" =~ handoff|artifact|schema|json ]] && fp="${fp}art,"
    [[ "${task_lower}" =~ brand|signature|logo ]] && fp="${fp}bra,"
    [[ "${task_lower}" =~ synth|aggregate|merge ]] && fp="${fp}syn,"
    [[ "${task_lower}" =~ bus|pubsub|message|topic ]] && fp="${fp}bus,"
    [[ "${task_lower}" =~ graph|entity|edge|knowledge ]] && fp="${fp}gra,"
    printf '%s' "${fp}"
}

# ============================================================================
# Conditional Protocol Loading with Token Budget + Caching
# Scans task text for keywords and selectively loads protocol files.
# _protocol.txt and _common.txt always load (mandatory).
# Stops loading optional protocols when token budget is exceeded.
# Results cached by keyword fingerprint within TTL window.
# ============================================================================

resolve_protocols() {
    local task="$1"
    local protocols_dir="${PROMPTS_DIR}/protocols"

    # Compute fingerprint for cache lookup
    local fingerprint
    fingerprint=$(_proto_fingerprint "${task}")

    # Return cached result if fingerprint matches
    if [[ -n "${_EUXIS_PROTO_CACHE}" && "${_EUXIS_PROTO_CACHE_KEY}" == "${fingerprint}" ]]; then
        printf '%s' "${_EUXIS_PROTO_CACHE}"
        return
    fi

    local task_lower="${task,,}"
    local result=""
    local token_count=0

    # Mandatory: always load _common.txt and _protocol.txt
    if [[ -f "${protocols_dir}/_common.txt" ]]; then
        local common_content
        common_content=$(< "${protocols_dir}/_common.txt")
        result="${common_content}"$'\n\n'
        token_count=$(( token_count + ${#common_content} / 4 ))
    fi
    if [[ -f "${protocols_dir}/_protocol.txt" ]]; then
        local proto_content
        proto_content=$(< "${protocols_dir}/_protocol.txt")
        result="${result}${proto_content}"$'\n\n'
        token_count=$(( token_count + ${#proto_content} / 4 ))
    fi

    # Conditional protocols: keyword regex -> file mapping
    # Uses bash [[ =~ ]] instead of forking to grep
    local -a kw_patterns=(
        "security|auth|cred|secret|pii"
        "version|release|bump|semver"
        "escalat|incident|sev|outage"
        "evidence|citation|source|fact"
        "handoff|artifact|schema|json"
        "brand|signature|logo"
        "synth|aggregate|merge"
        "bus|pubsub|message|topic"
        "graph|entity|edge|knowledge"
    )
    local -a kw_files=(
        "_security-boundaries.txt"
        "_versioning.txt"
        "_escalation-severity.txt"
        "_evidence-citation.txt"
        "_artifact-contract.txt"
        "_docs-branding.txt"
        "_synthesis.txt"
        "_bus.txt"
        "_graph-memory.txt"
    )

    local i
    for (( i=0; i<${#kw_patterns[@]}; i++ )); do
        if [[ "${task_lower}" =~ ${kw_patterns[$i]} ]]; then
            local filepath="${protocols_dir}/${kw_files[$i]}"
            if [[ -f "${filepath}" ]]; then
                local file_content
                file_content=$(< "${filepath}")
                local file_tokens=$(( ${#file_content} / 4 ))
                # Enforce token budget for optional protocols
                if (( token_count + file_tokens > EUXIS_PROTOCOL_TOKEN_BUDGET )); then
                    continue
                fi
                result="${result}${file_content}"$'\n\n'
                token_count=$(( token_count + file_tokens ))
            fi
        fi
    done

    # Cache result for this fingerprint
    _EUXIS_PROTO_CACHE="${result}"
    _EUXIS_PROTO_CACHE_KEY="${fingerprint}"

    printf '%s' "${result}"
}

# ============================================================================
# Registry memoization (avoids re-parsing JSON on every call)
# ============================================================================

_get_fleet_roster() {
    # Return cached result if available
    if [[ -n "${_EUXIS_REGISTRY_CACHE}" ]]; then
        # Check mtime for cache invalidation
        local registry_file="${EUXIS_HOME}/registry.db"
        [[ -f "${registry_file}" ]] || registry_file="${EUXIS_HOME}/registry.json"
        if [[ -f "${registry_file}" ]]; then
            local current_mtime
            current_mtime=$(stat -c '%Y' "${registry_file}" 2>/dev/null || stat -f '%m' "${registry_file}" 2>/dev/null)
            if [[ "${_EUXIS_REGISTRY_MTIME}" == "${current_mtime}" ]]; then
                printf '%s' "${_EUXIS_REGISTRY_CACHE}"
                return
            fi
        fi
    fi

    # Try SQLite first
    local db_file="${EUXIS_HOME}/registry.db"
    if [[ -f "${db_file}" ]]; then
        local ids
        ids=$(sqlite3 -init /dev/null "${db_file}" "SELECT id FROM agents ORDER BY id" 2>/dev/null)
        if [[ -n "${ids}" ]]; then
            # Join newline-separated IDs with ", "
            _EUXIS_REGISTRY_CACHE=$(echo "${ids}" | paste -sd',' - | sed 's/,/, /g')
            _EUXIS_REGISTRY_MTIME=$(stat -c '%Y' "${db_file}" 2>/dev/null || stat -f '%m' "${db_file}" 2>/dev/null)
            printf '%s' "${_EUXIS_REGISTRY_CACHE}"
            return
        fi
    fi

    # Fall back to JSON
    local registry_file="${EUXIS_HOME}/registry.json"
    if [[ -f "${registry_file}" ]] && command -v jq &>/dev/null; then
        _EUXIS_REGISTRY_CACHE=$(jq -r '[.agents[].id] | join(", ")' "${registry_file}" 2>/dev/null)
        _EUXIS_REGISTRY_MTIME=$(stat -c '%Y' "${registry_file}" 2>/dev/null || stat -f '%m' "${registry_file}" 2>/dev/null)
        printf '%s' "${_EUXIS_REGISTRY_CACHE}"
    fi
}

# ============================================================================
# Prompt Assembly (optimized: bash-native substitution, cached protocols,
# memoized registry, token-aware)
# ============================================================================

prepare_prompt() {
    local agent="$1"
    local task="$2"
    local audit_path="$3"
    local memory_path="$4"
    local session_id="$5"
    local model_name="$6"

    local prompt_file
    prompt_file=$(resolve_agent_path "${agent}")

    if [[ ! -f "${prompt_file}" ]]; then
        echo "[euxis] ERROR: Agent prompt not found: ${prompt_file}" >&2
        exit 1
    fi

    # Load protocols via conditional resolution with token budget + caching
    local protocol_content
    protocol_content=$(resolve_protocols "${task}")

    # Build prompt: protocols first (cache-friendly), then agent
    local prompt
    prompt="${protocol_content}$(< "${prompt_file}")"

    # Replace template variables via pure bash (no sed fork)
    prompt=$(template_substitute "${prompt}" "${audit_path}" "${memory_path}" "${session_id}" "${model_name}")

    # Build tiered memory context
    # Pure bash dirname (no subprocess fork)
    local project_dir="${memory_path%/*}"
    project_dir="${project_dir%/*}"
    if [[ ! -d "${project_dir}" ]]; then
        project_dir=""
    fi

    local memory_context
    memory_context=$(build_tiered_memory "${memory_path}" "${task}" "${project_dir}" "${agent}")

    # Build fleet roster (memoized)
    local fleet_roster
    fleet_roster=$(_get_fleet_roster)

    # Structured JSON intermediates for dispatch context
    local dispatch_directive=""
    if [[ "${EUXIS_DISPATCH:-false}" == "true" ]] || [[ "${task}" =~ \[MESH\ MODE\]|\[FEDERATED\ MODE\] ]]; then
        dispatch_directive='
---
## STRUCTURED INTERMEDIATE OUTPUT (Dispatch Mode)
You are operating in a dispatch pipeline. In addition to your normal output,
you MUST produce a structured JSON intermediate block at the end of your
response, wrapped in a fenced code block tagged `json-intermediate`:

```json-intermediate
{
  "agent": "<your agent_id>",
  "stage": <pipeline_stage_number>,
  "findings": [
    {"severity": "CRITICAL|HIGH|MEDIUM|LOW", "title": "<finding>", "detail": "<explanation>"}
  ],
  "artifacts": [
    {"path": "<file path>", "action": "CREATED|MODIFIED|DELETED"}
  ],
  "status": "SUCCESS|FAILURE"
}
```

Rules:
- The JSON MUST be valid. Do NOT use trailing commas or unquoted keys.
- `stage` should reflect your position in the pipeline (1-indexed).
- `findings` MUST contain at least one entry summarizing work done.
- `artifacts` may be empty `[]` if no files were created or modified.
- Downstream agents will parse this block — do NOT omit it.'
    fi

    cat <<EOF
${prompt}

---
## MEMORY CONTEXT (Tiered Retrieval)
${memory_context}

---
## FLEET ROSTER (Valid Agent IDs)
Use ONLY agents from this list in mission manifests: ${fleet_roster}
${dispatch_directive}

---
## CURRENT TASK
${task}
EOF
}
