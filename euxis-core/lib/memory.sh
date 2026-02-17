#!/usr/bin/env bash
# Euxis Library: Tiered memory retrieval (MemGPT-inspired)
# Optimized: pure-bash keyword extraction, no subprocess forks in hot path
[[ -n "${_EUXIS_LIB_MEMORY:-}" ]] && return; _EUXIS_LIB_MEMORY=1

set -euo pipefail


EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"

# Validate memory file path: reject path traversal and paths outside EUXIS_HOME
_validate_memory_path() {
    local path="$1"
    # Reject path traversal
    if [[ "$path" == *".."* ]]; then
        log_error "Path traversal rejected: ${path}"
        return 1
    fi
    # Ensure path is under EUXIS_HOME
    local resolved
    resolved="$(cd "$(dirname "$path")" 2>/dev/null && pwd)/$(basename "$path")"
    if [[ "$resolved" != "${EUXIS_HOME}"/* ]]; then
        log_error "Path outside EUXIS_HOME rejected: ${path}"
        return 1
    fi
    return 0
}

# Tier 1: Hot memory — most recent entries (always included)
get_hot_memory() {
    local memory_file="$1"
    if [[ -f "${memory_file}" ]]; then
        tail -n 20 "${memory_file}" 2>/dev/null || true
    fi
}

# Pure-bash keyword extraction: words >= 5 chars, deduplicated, max 10
# Replaces: echo | tr | grep -oE | sort -u | head (5 forks → 0 forks)
_extract_keywords() {
    local task_lower="${1,,}"
    local -A seen=()
    local keywords=""
    local count=0
    local word

    # Split on non-alpha characters, collect words >= 5 chars
    for word in ${task_lower//[^a-z]/ }; do
        if (( ${#word} >= 5 )) && [[ -z "${seen[${word}]:-}" ]]; then
            seen["${word}"]=1
            if [[ -n "${keywords}" ]]; then
                keywords="${keywords}|${word}"
            else
                keywords="${word}"
            fi
            (( count++ ))
            (( count >= 10 )) && break
        fi
    done
    printf '%s' "${keywords}"
}

# Tier 2: Relevant memory — keyword search against full memory
get_relevant_memory() {
    local memory_file="$1"
    local task="$2"

    if [[ ! -f "${memory_file}" ]]; then
        return
    fi

    local pattern
    pattern=$(_extract_keywords "${task}")

    if [[ -z "${pattern}" ]]; then
        return
    fi

    # grep is unavoidable here — searching file content (not a hot-path fork issue)
    grep -iE -- "${pattern}" "${memory_file}" 2>/dev/null | \
        sort -u | head -n 10 || true
}

# Tier 3: Cross-agent memory — search sibling agents for relevant context
get_cross_agent_memory() {
    local project_dir="$1"
    local current_agent="$2"
    local task="$3"

    if [[ ! -d "${project_dir}" ]]; then
        return
    fi

    local pattern
    pattern=$(_extract_keywords "${task}")

    if [[ -z "${pattern}" ]]; then
        return
    fi

    # Search all sibling agent memory files (not our own)
    local sibling_mem sibling_agent
    for sibling_mem in "${project_dir}"/*/memory.md; do
        [[ -f "${sibling_mem}" ]] || continue
        # Skip our own memory — pure bash dirname/basename
        sibling_agent="${sibling_mem%/memory.md}"
        sibling_agent="${sibling_agent##*/}"
        [[ "${sibling_agent}" == "${current_agent}" ]] && continue

        local matches
        matches=$(grep -iE -- "${pattern}" "${sibling_mem}" 2>/dev/null | head -n 3 || true)
        if [[ -n "${matches}" ]]; then
            printf '  [%s]:\n' "${sibling_agent}"
            while IFS= read -r line; do
                printf '    %s\n' "${line}"
            done <<< "${matches}"
        fi
    done
}

build_tiered_memory() {
    local memory_file="$1"
    local task="$2"
    local project_dir="$3"
    local agent="$4"

    local hot relevant cross

    hot=$(get_hot_memory "${memory_file}")
    relevant=$(get_relevant_memory "${memory_file}" "${task}")
    cross=$(get_cross_agent_memory "${project_dir}" "${agent}" "${task}")

    cat <<MEMEOF
### Tier 1: Hot Memory (Recent)
${hot:-<empty>}

### Tier 2: Relevant Memory (Keyword Match)
${relevant:-<no matches>}

### Tier 3: Cross-Agent Context (Read-Only)
${cross:-<no sibling matches>}
MEMEOF
}

# ============================================================================
# Memory Pruning Strategy
# Prevents unbounded memory file growth. Called by euxis-kaizen or manually.
# Retains: all REFLECTION/CONTRAINDICATION entries (permanent),
# recent N entries (configurable), and keyword-matched entries.
# ============================================================================

EUXIS_MEMORY_MAX_LINES="${EUXIS_MEMORY_MAX_LINES:-500}"
EUXIS_MEMORY_RECENT_KEEP="${EUXIS_MEMORY_RECENT_KEEP:-100}"

prune_memory() {
    local memory_file="$1"
    local max_lines="${2:-${EUXIS_MEMORY_MAX_LINES}}"
    local recent_keep="${3:-${EUXIS_MEMORY_RECENT_KEEP}}"

    _validate_memory_path "${memory_file}" || return 1
    [[ -f "${memory_file}" ]] || return 0

    local total_lines
    total_lines=$(wc -l < "${memory_file}")

    # No pruning needed if under threshold
    (( total_lines <= max_lines )) && return 0

    local temp_file
    temp_file=$(mktemp "${memory_file}.prune.XXXXXX") || { log_error "Failed to create temp file for pruning"; return 1; }
    trap 'rm -f "${temp_file}"' RETURN

    local header_line

    # Always keep the first line (header: "# Memory: agent-name")
    header_line=$(head -n 1 "${memory_file}")

    {
        printf '%s\n\n' "${header_line}"

        # 1. Extract all permanent entries (REFLECTION, CONTRAINDICATION, PROCEDURAL with CONTRAINDICATION)
        grep -iE '(REFLECTION|CONTRAINDICATION)' -- "${memory_file}" | grep -v '^#' || true

        printf '\n# --- Pruned on %s (retained %s recent + permanent entries) ---\n\n' "$(date +%Y-%m-%d)" "${recent_keep}"

        # 2. Keep the most recent N entries (excluding header and permanent entries already captured)
        tail -n "${recent_keep}" "${memory_file}" | grep -viE '(REFLECTION|CONTRAINDICATION)' || true
    } > "${temp_file}"

    # Atomic replace
    mv "${temp_file}" "${memory_file}"

    log_debug "PRUNE: ${memory_file}: ${total_lines} -> $(wc -l < "${memory_file}") lines"
}

# Prune all memory files in a project
prune_project_memory() {
    local project_dir="$1"
    [[ -d "${project_dir}" ]] || return 0

    local mem_file
    for mem_file in "${project_dir}"/*/memory.md; do
        [[ -f "${mem_file}" ]] || continue
        prune_memory "${mem_file}"
    done
}

# ============================================================================
# Semantic Drift Detection
# Detects when new memories contradict or supersede existing ones.
# Returns contradictions found (or empty if none).
# ============================================================================

detect_semantic_drift() {
    local memory_file="$1"
    local new_fact="$2"

    [[ -f "${memory_file}" ]] || return 0

    local new_lower="${new_fact,,}"
    local contradictions=""

    # Extract key entities from the new fact (words >= 5 chars)
    local entities=""
    local word
    for word in ${new_lower//[^a-z]/ }; do
        (( ${#word} >= 5 )) && entities="${entities:+${entities}|}${word}"
    done

    [[ -z "${entities}" ]] && return 0

    # Search existing memories for matching entities
    local matches
    matches=$(grep -iE -- "${entities}" "${memory_file}" 2>/dev/null | grep -v '^#' | head -20) || true

    [[ -z "${matches}" ]] && return 0

    # Check for negation patterns that indicate contradiction
    # If new fact says X and old fact says "NOT X" or "NEVER X" or vice versa
    local line
    while IFS= read -r line; do
        local line_lower="${line,,}"

        # Detect direct contradictions via negation markers
        if [[ "${new_lower}" =~ (not|never|no longer|removed|deprecated|disabled) ]] && \
           [[ ! "${line_lower}" =~ (not|never|no longer|removed|deprecated|disabled) ]]; then
            contradictions="${contradictions}[DRIFT] New fact negates existing: ${line}"$'\n'
        elif [[ ! "${new_lower}" =~ (not|never|no longer|removed|deprecated|disabled) ]] && \
             [[ "${line_lower}" =~ (not|never|no longer|removed|deprecated|disabled) ]]; then
            contradictions="${contradictions}[DRIFT] Existing negation conflicts with new fact: ${line}"$'\n'
        fi

        # Detect version/value changes (e.g., "uses JWT" vs "uses OAuth")
        if [[ "${new_lower}" =~ uses\ ([a-z]+) ]] && [[ "${line_lower}" =~ uses\ ([a-z]+) ]]; then
            local new_val="${BASH_REMATCH[1]}"
            local old_val
            [[ "${line_lower}" =~ uses\ ([a-z]+) ]] && old_val="${BASH_REMATCH[1]}"
            if [[ -n "${old_val}" && "${new_val}" != "${old_val}" ]]; then
                contradictions="${contradictions}[DRIFT] Value change detected — old: '${old_val}', new: '${new_val}': ${line}"$'\n'
            fi
        fi
    done <<< "${matches}"

    if [[ -n "${contradictions}" ]]; then
        printf '%s' "${contradictions}"
    fi
}

# ============================================================================
# Memory Contradiction Resolution
# When drift is detected, mark superseded memories and keep audit trail.
# ============================================================================

resolve_memory_contradiction() {
    local memory_file="$1"
    local new_fact="$2"
    local agent="$3"
    local resolution="${4:-supersede}"  # supersede | keep_both | reject

    _validate_memory_path "${memory_file}" || return 1
    case "${resolution}" in
        supersede)
            # Mark old contradicting entries as superseded
            local drift
            drift=$(detect_semantic_drift "${memory_file}" "${new_fact}")
            if [[ -n "${drift}" ]]; then
                # Append resolution record
                printf '[%s] SEMANTIC: [SUPERSEDED by %s] %s\n' \
                    "$(date +%Y-%m-%d)" "${agent}" "${drift}" >> "${memory_file}"
            fi
            # Append new fact
            printf '[%s] SEMANTIC: %s\n' "$(date +%Y-%m-%d)" "${new_fact}" >> "${memory_file}"
            ;;
        keep_both)
            printf '[%s] SEMANTIC: %s [CONFLICTS WITH EXISTING — KEPT BOTH]\n' \
                "$(date +%Y-%m-%d)" "${new_fact}" >> "${memory_file}"
            ;;
        reject)
            printf '[%s] SEMANTIC: [REJECTED — contradicts existing knowledge] %s\n' \
                "$(date +%Y-%m-%d)" "${new_fact}" >> "${memory_file}"
            ;;
    esac
}

# ============================================================================
# Knowledge Graph Evolution
# Automatically creates graph relationships from memory entries
# ============================================================================

auto_evolve_graph() {
    local memory_file="$1"
    local agent="$2"
    local graph_cmd="${EUXIS_HOME}/euxis-cli/bin/euxis-graph"

    [[ -x "${graph_cmd}" ]] || return 0
    [[ -f "${memory_file}" ]] || return 0

    # Extract recent entries (last 5) and create entity relationships
    local recent
    recent=$(tail -n 5 "${memory_file}" | grep -v '^#' | grep -v '^$') || true

    [[ -z "${recent}" ]] && return 0

    local line
    while IFS= read -r line; do
        # Extract entity-like terms (capitalized words or quoted strings)
        local entities
        entities=$(printf '%s\n' "${line}" | grep -oE '\b[A-Z][a-z]+([A-Z][a-z]+)+\b' 2>/dev/null | head -3) || true

        if [[ -n "${entities}" ]]; then
            local prev=""
            local entity
            while IFS= read -r entity; do
                local entity_lower="${entity,,}"
                entity_lower="${entity_lower// /-}"
                if [[ -n "${prev}" ]]; then
                    "${graph_cmd}" add-edge "${prev}" "${entity_lower}" --relation "related_to" 2>/dev/null || true
                fi
                # Link to agent
                "${graph_cmd}" add-edge "${entity_lower}" "${agent}" --relation "observed_by" 2>/dev/null || true
                prev="${entity_lower}"
            done <<< "${entities}"
        fi
    done <<< "${recent}"
}
