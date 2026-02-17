#!/usr/bin/env bash
# Euxis Library: SQLite-backed registry queries with connection pooling
[[ -n "${_EUXIS_LIB_REGISTRY_SQL:-}" ]] && return; _EUXIS_LIB_REGISTRY_SQL=1

set -euo pipefail


EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"
REGISTRY_DB="${EUXIS_HOME}/euxis-core/agents/registry.db"

# Connection pool directory for named pipes
REGISTRY_POOL_DIR="${EUXIS_HOME}/euxis-runtime/data/registry_pool"
REGISTRY_POOL_SIZE="${EUXIS_REGISTRY_POOL_SIZE:-3}"

# ============================================================================
# Connection Pool Management
# ============================================================================

_registry_pool_init() {
    [[ -d "${REGISTRY_POOL_DIR}" ]] || mkdir -p "${REGISTRY_POOL_DIR}"

    # Resolve real pool path once for traversal validation
    local real_pool
    real_pool=$(cd "${REGISTRY_POOL_DIR}" 2>/dev/null && pwd -P) || return 0

    # Clean stale connections (check for dead processes)
    local lockdir pid real_lockdir
    for lockdir in "${REGISTRY_POOL_DIR}"/conn_*.lock; do
        [[ -d "${lockdir}" ]] || continue

        # Validate lock directory is within pool directory (prevent traversal)
        real_lockdir=$(cd "${lockdir}" 2>/dev/null && pwd -P) || continue
        [[ "${real_lockdir}" == "${real_pool}"/* ]] || continue

        if [[ -f "${lockdir}/pid" ]]; then
            pid=$(cat "${lockdir}/pid" 2>/dev/null) || continue
            # Validate PID is numeric before kill -0
            [[ "${pid}" =~ ^[0-9]+$ ]] || { rm -rf "${lockdir}" 2>/dev/null || true; continue; }
            # Check if process is still alive
            if ! kill -0 "${pid}" 2>/dev/null; then
                # Process dead, remove stale lock
                rm -rf "${lockdir}" 2>/dev/null || true
            fi
        else
            # No pid file — clean up if older than 30 minutes
            if [[ $(find "${lockdir}" -maxdepth 0 -mmin +30 -print 2>/dev/null | wc -l) -gt 0 ]]; then
                rm -rf "${lockdir}" 2>/dev/null || true
            fi
        fi
    done
}

_registry_get_connection() {
    _registry_pool_init

    local i lockdir
    for ((i = 0; i < REGISTRY_POOL_SIZE; i++)); do
        lockdir="${REGISTRY_POOL_DIR}/conn_${i}.lock"

        # Try to acquire exclusive lock using atomic mkdir operation
        # mkdir is atomic - either succeeds completely or fails completely
        if mkdir "${lockdir}" 2>/dev/null; then
            # Write PID immediately; if this fails, release lock and try next slot
            if ! printf '%s\n' "$$" > "${lockdir}/pid" 2>/dev/null; then
                rm -rf "${lockdir}" 2>/dev/null || true
                continue
            fi
            echo "${i}"
            return 0
        fi
    done

    # All connections busy, use -1 to indicate new connection needed
    echo "-1"
    return 1
}

_registry_release_connection() {
    local conn_id="$1"
    [[ "${conn_id}" == "-1" ]] && return 0  # No lock to release

    local lockdir="${REGISTRY_POOL_DIR}/conn_${conn_id}.lock"
    rm -rf "${lockdir}" 2>/dev/null || true
}

# ============================================================================
# Core Query Functions
# ============================================================================

# Execute SQL query with connection pooling and error handling
registry_query() {
    local sql="$1"
    shift
    local args=("$@")

    [[ -f "${REGISTRY_DB}" ]] || {
        log_error "Registry database not found: ${REGISTRY_DB}"
        log_error "Run: python3 migrate-registry-to-sqlite.py"
        return 1
    }

    local conn_id result
    conn_id=$(_registry_get_connection)

    # Execute query with timeout and error handling
    # Use restrictive umask for temp file creation (owner-only read/write)
    local temp_file old_umask
    old_umask=$(umask)
    umask 077
    temp_file=$(mktemp "${TMPDIR:-/tmp}/euxis_registry_query.XXXXXX") || { umask "${old_umask}"; log_error "mktemp failed for registry query"; _registry_release_connection "${conn_id}"; return 1; }
    umask "${old_umask}"

    {
        if [[ ${#args[@]} -gt 0 ]]; then
            printf '.parameter init\n'
            local i
            for ((i = 0; i < ${#args[@]}; i++)); do
                printf '.parameter set $%d "%s"\n' $((i + 1)) "${args[i]}"
            done
        fi
        printf '%s\n' "${sql}"
    } | sqlite3 -init /dev/null "${REGISTRY_DB}" > "${temp_file}" 2>&1

    local exit_code=$?
    result=$(cat "${temp_file}")
    rm -f "${temp_file}"

    _registry_release_connection "${conn_id}"

    if [[ ${exit_code} -ne 0 ]]; then
        log_error "Registry query failed: ${result}"
        return 1
    fi

    printf '%s\n' "${result}"
    return 0
}

# ============================================================================
# Agent Discovery Functions (SQLite-backed)
# ============================================================================

list_agents_sql() {
    registry_query "SELECT id FROM agents ORDER BY id"
}

resolve_agent_path_sql() {
    local agent="$1"
    [[ -z "${agent}" ]] && { log_error "resolve_agent_path_sql requires agent name"; return 1; }

    local path
    path=$(registry_query "SELECT path FROM agents WHERE id = ?" "${agent}")

    if [[ -n "${path}" ]]; then
        echo "${EUXIS_HOME}/${path}"
        return 0
    else
        return 1
    fi
}

get_agent_tier_sql() {
    local agent="$1"
    [[ -z "${agent}" ]] && { log_error "get_agent_tier_sql requires agent name"; return 1; }

    registry_query "SELECT tier FROM agents WHERE id = ?" "${agent}"
}

get_agent_activation_sql() {
    local agent="$1"
    [[ -z "${agent}" ]] && { log_error "get_agent_activation_sql requires agent name"; return 1; }

    registry_query "SELECT activation FROM agents WHERE id = ?" "${agent}"
}

# ============================================================================
# Advanced Query Functions (Only possible with SQL)
# ============================================================================

find_agents_by_tag_sql() {
    local tag="$1"
    [[ -z "${tag}" ]] && { log_error "find_agents_by_tag_sql requires tag name"; return 1; }

    registry_query "
        SELECT DISTINCT a.id
        FROM agents a
        JOIN agent_tags at ON a.id = at.agent_id
        JOIN tags t ON at.tag_id = t.id
        WHERE t.name = ?
        ORDER BY a.id
    " "${tag}"
}

find_agents_by_tier_sql() {
    local tier="$1"
    [[ -z "${tier}" ]] && { log_error "find_agents_by_tier_sql requires tier name"; return 1; }

    registry_query "SELECT id FROM agents WHERE tier = ? ORDER BY id" "${tier}"
}

find_agents_by_capability_sql() {
    local capability="$1"
    [[ -z "${capability}" ]] && { log_error "find_agents_by_capability_sql requires capability name"; return 1; }

    registry_query "
        SELECT DISTINCT a.id
        FROM agents a
        JOIN agent_capabilities ac ON a.id = ac.agent_id
        JOIN capability_tags c ON ac.capability_id = c.id
        WHERE c.name = ?
        ORDER BY a.id
    " "${capability}"
}

find_agents_by_activation_sql() {
    local activation="${1:-default}"

    if [[ "${activation}" == "all" ]]; then
        list_agents_sql
    else
        registry_query "SELECT id FROM agents WHERE activation = ? OR activation IS NULL ORDER BY id" "${activation}"
    fi
}

# Multi-criteria search
find_agents_by_criteria_sql() {
    local tier="${1:-}"
    local tag="${2:-}"
    local capability="${3:-}"
    local activation="${4:-}"

    local sql="SELECT DISTINCT a.id FROM agents a"
    local joins=""
    local conditions=()
    local params=()

    if [[ -n "${tag}" ]]; then
        joins+=" JOIN agent_tags at ON a.id = at.agent_id JOIN tags t ON at.tag_id = t.id"
        conditions+=("t.name = ?")
        params+=("${tag}")
    fi

    if [[ -n "${capability}" ]]; then
        joins+=" JOIN agent_capabilities ac ON a.id = ac.agent_id JOIN capability_tags c ON ac.capability_id = c.id"
        conditions+=("c.name = ?")
        params+=("${capability}")
    fi

    if [[ -n "${tier}" ]]; then
        conditions+=("a.tier = ?")
        params+=("${tier}")
    fi

    if [[ -n "${activation}" ]]; then
        conditions+=("(a.activation = ? OR a.activation IS NULL)")
        params+=("${activation}")
    fi

    sql+="${joins}"

    if [[ ${#conditions[@]} -gt 0 ]]; then
        sql+=" WHERE $(IFS=" AND "; echo "${conditions[*]}")"
    fi

    sql+=" ORDER BY a.id"

    registry_query "${sql}" "${params[@]}"
}

# ============================================================================
# Statistics and Health Functions
# ============================================================================

registry_stats_sql() {
    echo "=== Registry Statistics ==="

    local total_agents
    total_agents=$(registry_query "SELECT COUNT(*) FROM agents")
    echo "Total agents: ${total_agents}"

    echo ""
    echo "Agents by tier:"
    registry_query "SELECT tier, COUNT(*) FROM agents GROUP BY tier ORDER BY tier" | while IFS='|' read -r tier count; do
        printf "  %-10s: %s\n" "${tier}" "${count}"
    done

    echo ""
    echo "Agents by activation:"
    registry_query "
        SELECT
            CASE
                WHEN activation IS NULL THEN 'default'
                ELSE activation
            END as activation_type,
            COUNT(*) as count
        FROM agents
        GROUP BY activation_type
        ORDER BY activation_type
    " | while IFS='|' read -r activation count; do
        printf "  %-10s: %s\n" "${activation}" "${count}"
    done

    echo ""
    echo "Top tags:"
    registry_query "
        SELECT t.name, COUNT(*) as usage_count
        FROM tags t
        JOIN agent_tags at ON t.id = at.tag_id
        GROUP BY t.id, t.name
        ORDER BY usage_count DESC, t.name
        LIMIT 10
    " | while IFS='|' read -r tag count; do
        printf "  %-20s: %s agents\n" "${tag}" "${count}"
    done

    echo ""
    echo "Top capabilities:"
    registry_query "
        SELECT c.name, COUNT(*) as usage_count
        FROM capability_tags c
        JOIN agent_capabilities ac ON c.id = ac.capability_id
        GROUP BY c.id, c.name
        ORDER BY usage_count DESC, c.name
        LIMIT 10
    " | while IFS='|' read -r capability count; do
        printf "  %-20s: %s agents\n" "${capability}" "${count}"
    done
}

# Verify database integrity and performance
registry_health_sql() {
    echo "=== Registry Health Check ==="

    # Check database existence and accessibility
    if [[ ! -f "${REGISTRY_DB}" ]]; then
        echo "❌ Database file not found: ${REGISTRY_DB}"
        return 1
    fi

    if [[ ! -r "${REGISTRY_DB}" ]]; then
        echo "❌ Database file not readable: ${REGISTRY_DB}"
        return 1
    fi

    echo "✅ Database file accessible"

    # Check schema integrity
    local table_count
    table_count=$(registry_query "SELECT COUNT(*) FROM sqlite_master WHERE type='table'")
    if [[ "${table_count}" -lt 5 ]]; then
        echo "❌ Expected at least 5 tables, found ${table_count}"
        return 1
    fi
    echo "✅ Schema integrity (${table_count} tables)"

    # Check index usage
    local index_count
    index_count=$(registry_query "SELECT COUNT(*) FROM sqlite_master WHERE type='index'")
    echo "✅ Performance indexes (${index_count} indexes)"

    # Quick performance test
    local start_time end_time duration
    start_time=$(python3 -c "import time; print(time.perf_counter())")
    list_agents_sql > /dev/null
    end_time=$(python3 -c "import time; print(time.perf_counter())")
    duration=$(python3 -c "print(f'{($end_time - $start_time) * 1000:.2f}')")
    echo "✅ Query performance (${duration}ms for list_agents)"

    # Check data consistency
    local agent_count file_count
    agent_count=$(registry_query "SELECT COUNT(*) FROM agents")
    file_count=$(find "${EUXIS_HOME}/euxis-core/agents/prompts/core" "${EUXIS_HOME}/euxis-core/agents/prompts/fleet" -name "*.txt" 2>/dev/null | grep -v "/_" | wc -l | tr -d ' ')

    if [[ "${agent_count}" -ne "${file_count}" ]]; then
        echo "⚠️  Agent count mismatch: DB=${agent_count}, Files=${file_count}"
        echo "   Consider re-running migration script"
    else
        echo "✅ Data consistency (${agent_count} agents)"
    fi
}

# ============================================================================
# Fallback to Filesystem (when SQLite unavailable)
# ============================================================================

# Wrapper functions that fall back to filesystem if SQLite fails
list_agents_hybrid() {
    if list_agents_sql 2>/dev/null; then
        return 0
    else
        log_warn "SQLite query failed, falling back to filesystem"
        list_agents  # From agents.sh
    fi
}

resolve_agent_path_hybrid() {
    local agent="$1"
    local result

    result=$(resolve_agent_path_sql "${agent}" 2>/dev/null) || {
        log_warn "SQLite query failed, falling back to filesystem"
        resolve_agent_path "${agent}"  # From agents.sh
        return $?
    }

    echo "${result}"
}

# ============================================================================
# Migration and Maintenance
# ============================================================================

registry_rebuild() {
    local migrate_script="${EUXIS_HOME}/scripts/migration/migrate-registry-to-sqlite.py"

    # Fallback: use JSON directly if migration script doesn't exist
    if [[ ! -f "${migrate_script}" ]]; then
        # JSON is always available as fallback - no rebuild needed
        return 0
    fi

    echo "Rebuilding registry database..."

    if [[ -f "${REGISTRY_DB}" ]]; then
        local backup_file
        backup_file="${REGISTRY_DB}.backup.$(date +%Y%m%d-%H%M%S)"
        cp "${REGISTRY_DB}" "${backup_file}"
        echo "Created backup: ${backup_file}"
    fi

    if python3 "${migrate_script}" --force; then
        echo "✅ Registry database rebuilt successfully"
        registry_health_sql
    else
        echo "❌ Failed to rebuild registry database"
        return 1
    fi
}

# Auto-rebuild if database is missing or outdated
_registry_auto_rebuild() {
    [[ -f "${REGISTRY_DB}" ]] || {
        log_warn "Registry database missing, auto-rebuilding..."
        registry_rebuild
        return $?
    }

    # Check if JSON is newer than database
    if [[ "${EUXIS_HOME}/euxis-core/agents/registry.json" -nt "${REGISTRY_DB}" ]]; then
        log_warn "Registry JSON is newer than database, auto-rebuilding..."
        registry_rebuild
        return $?
    fi
}

# ============================================================================
# Convenience Helpers (used by certify, health, squad, version scripts)
# ============================================================================

registry_get_version() {
    if [[ -f "${REGISTRY_DB}" ]]; then
        sqlite3 -init /dev/null "${REGISTRY_DB}" "SELECT value FROM registry_metadata WHERE key='protocol_version'" 2>/dev/null && return
    fi
    if [[ -f "${EUXIS_HOME}/euxis-core/agents/registry.json" ]] && command -v jq &>/dev/null; then
        jq -r '.protocol_version // .version' "${EUXIS_HOME}/euxis-core/agents/registry.json" 2>/dev/null && return
    fi
    echo "0.0.8"
}

registry_agent_exists() {
    local agent="$1"
    if [[ -f "${REGISTRY_DB}" ]]; then
        [[ -n "$(registry_query "SELECT 1 FROM agents WHERE id = ?" "${agent}" 2>/dev/null)" ]] && return 0
    fi
    if [[ -f "${EUXIS_HOME}/euxis-core/agents/registry.json" ]] && command -v jq &>/dev/null; then
        jq -e --arg id "${agent}" '.agents[] | select(.id == $id)' "${EUXIS_HOME}/euxis-core/agents/registry.json" &>/dev/null && return 0
    fi
    return 1
}

registry_list_agent_ids() {
    if [[ -f "${REGISTRY_DB}" ]]; then
        sqlite3 -init /dev/null "${REGISTRY_DB}" "SELECT id FROM agents ORDER BY id" 2>/dev/null && return
    fi
    if [[ -f "${EUXIS_HOME}/euxis-core/agents/registry.json" ]] && command -v jq &>/dev/null; then
        jq -r '.agents[].id' "${EUXIS_HOME}/euxis-core/agents/registry.json" 2>/dev/null && return
    fi
}

# Initialize on import (only if interactive)
if [[ "${BASH_SOURCE[0]:-}" == "${0:-}" ]] || [[ -t 1 ]]; then
    _registry_auto_rebuild 2>/dev/null || true
fi
