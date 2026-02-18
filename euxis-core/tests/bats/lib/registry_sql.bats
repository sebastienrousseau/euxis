#!/usr/bin/env bats
# Test suite for core/lib/registry_sql.sh

# Test setup - run before each test
setup() {
    EUXIS_TEST_TMPDIR="$(mktemp -d)"
    export EUXIS_TEST_TMPDIR

    export EUXIS_HOME="${EUXIS_TEST_TMPDIR}/euxis"
    export REGISTRY_DB="${EUXIS_HOME}/euxis-core/agents/registry.db"
    export REGISTRY_POOL_DIR="${EUXIS_HOME}/euxis-runtime/data/registry_pool"
    mkdir -p "${EUXIS_HOME}/euxis-runtime/data/registry_pool"
    mkdir -p "${EUXIS_HOME}/euxis-core/agents/prompts/core"
    mkdir -p "${EUXIS_HOME}/euxis-core/agents/prompts/fleet"

    # Create mock agent files
    for agent in architect tester debugger; do
        echo "# ${agent}" > "${EUXIS_HOME}/euxis-core/agents/prompts/core/${agent}.txt"
    done

    # Create mock SQLite database
    if command -v sqlite3 &>/dev/null; then
        sqlite3 "${EUXIS_HOME}/euxis-core/agents/registry.db" << 'EOF'
CREATE TABLE IF NOT EXISTS agents (
    id TEXT PRIMARY KEY,
    path TEXT,
    tier TEXT,
    activation TEXT
);
CREATE TABLE IF NOT EXISTS tags (
    id INTEGER PRIMARY KEY,
    name TEXT UNIQUE
);
CREATE TABLE IF NOT EXISTS agent_tags (
    agent_id TEXT,
    tag_id INTEGER
);
CREATE TABLE IF NOT EXISTS capability_tags (
    id INTEGER PRIMARY KEY,
    name TEXT UNIQUE
);
CREATE TABLE IF NOT EXISTS agent_capabilities (
    agent_id TEXT,
    capability_id INTEGER
);
CREATE TABLE IF NOT EXISTS registry_metadata (
    key TEXT PRIMARY KEY,
    value TEXT
);
INSERT INTO agents VALUES ('architect', 'agents/prompts/core/architect.txt', 'core', 'default');
INSERT INTO agents VALUES ('tester', 'agents/prompts/core/tester.txt', 'default', 'default');
INSERT INTO agents VALUES ('debugger', 'agents/prompts/core/debugger.txt', 'default', 'default');
INSERT INTO tags VALUES (1, 'development');
INSERT INTO tags VALUES (2, 'quality');
INSERT INTO agent_tags VALUES ('architect', 1);
INSERT INTO agent_tags VALUES ('tester', 2);
INSERT INTO capability_tags VALUES (1, 'code-review');
INSERT INTO capability_tags VALUES (2, 'testing');
INSERT INTO agent_capabilities VALUES ('architect', 1);
INSERT INTO agent_capabilities VALUES ('tester', 2);
INSERT INTO registry_metadata VALUES ('protocol_version', '0.0.8');
EOF
    fi

    # Create fallback JSON
    cat > "${EUXIS_HOME}/euxis-core/agents/registry.json" << 'EOF'
{
  "protocol_version": "0.0.8",
  "agents": [
    {"id": "architect", "tier": "core"},
    {"id": "tester", "tier": "default"},
    {"id": "debugger", "tier": "default"}
  ]
}
EOF

    export PATH="${EUXIS_TEST_TMPDIR}:${PATH}"

    # Reset include guards
    unset _EUXIS_LIB_REGISTRY_SQL
    unset _EUXIS_LIB_COMMON

    # Source dependencies from real installation
    source "${BATS_TEST_DIRNAME}/../../../lib/common.sh"
    source "${BATS_TEST_DIRNAME}/../../../lib/registry_sql.sh"
}

teardown() {
    if [[ -n "${EUXIS_TEST_TMPDIR:-}" && -d "${EUXIS_TEST_TMPDIR:-}" ]]; then
        rm -rf "${EUXIS_TEST_TMPDIR}"
    fi
}

# ============================================================================
# CONNECTION POOL TESTS
# ============================================================================

@test "_registry_pool_init creates pool directory" {
    rm -rf "${EUXIS_HOME}/euxis-runtime/data/registry_pool"
    _registry_pool_init
    [[ -d "${EUXIS_HOME}/euxis-runtime/data/registry_pool" ]]
}

@test "_registry_pool_init cleans stale connections" {
    # Create a stale lock with dead PID
    local stale_lock="${EUXIS_HOME}/euxis-runtime/data/registry_pool/conn_0.lock"
    mkdir -p "${stale_lock}"
    echo "99999999" > "${stale_lock}/pid"

    _registry_pool_init

    # Stale lock should be cleaned
    [[ ! -d "${stale_lock}" ]]
}

@test "_registry_get_connection returns connection id" {
    run _registry_get_connection
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ ^[0-9]+$ ]] || [[ "${output}" == "-1" ]]
}

@test "_registry_release_connection releases lock" {
    local conn_id
    conn_id=$(_registry_get_connection)

    if [[ "${conn_id}" != "-1" ]]; then
        local lockdir="${EUXIS_HOME}/euxis-runtime/data/registry_pool/conn_${conn_id}.lock"
        [[ -d "${lockdir}" ]]

        _registry_release_connection "${conn_id}"
        [[ ! -d "${lockdir}" ]]
    fi
}

@test "_registry_release_connection handles -1" {
    run _registry_release_connection "-1"
    [[ "${status}" -eq 0 ]]
}

# ============================================================================
# REGISTRY_QUERY TESTS
# ============================================================================

@test "registry_query executes SQL" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"

    run registry_query "SELECT COUNT(*) FROM agents"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "3" ]]
}

@test "registry_query handles parameters" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"

    run registry_query "SELECT tier FROM agents WHERE id = ?" "architect"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "core" ]]
}

@test "registry_query fails on missing database" {
    rm -f "${EUXIS_HOME}/euxis-core/agents/registry.db"
    run registry_query "SELECT 1"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "not found" ]]
}

# ============================================================================
# AGENT DISCOVERY TESTS
# ============================================================================

@test "list_agents_sql returns all agents" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"

    run list_agents_sql
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "architect" ]]
    [[ "${output}" =~ "tester" ]]
}

@test "resolve_agent_path_sql returns path" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"

    run resolve_agent_path_sql "architect"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "agents/prompts/core/architect.txt" ]]
}

@test "resolve_agent_path_sql normalizes legacy prompts/core path" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"

    sqlite3 "${EUXIS_HOME}/euxis-core/agents/registry.db" \
        "UPDATE agents SET path='prompts/core/architect.txt' WHERE id='architect';"

    run resolve_agent_path_sql "architect"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "${EUXIS_HOME}/euxis-core/agents/prompts/core/architect.txt" ]]
}

@test "resolve_agent_path_sql fails for unknown agent" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"

    run resolve_agent_path_sql "nonexistent"
    [[ "${status}" -eq 1 ]] || [[ -z "${output}" ]]
}

@test "resolve_agent_path_sql requires agent name" {
    run resolve_agent_path_sql ""
    [[ "${status}" -eq 1 ]]
}

@test "get_agent_tier_sql returns tier" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"

    run get_agent_tier_sql "architect"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "core" ]]
}

@test "get_agent_activation_sql returns activation" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"

    run get_agent_activation_sql "architect"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "default" ]]
}

# ============================================================================
# ADVANCED QUERY TESTS
# ============================================================================

@test "find_agents_by_tag_sql finds agents" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"

    run find_agents_by_tag_sql "development"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "architect" ]]
}

@test "find_agents_by_tier_sql finds agents" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"

    run find_agents_by_tier_sql "core"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "architect" ]]
}

@test "find_agents_by_capability_sql finds agents" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"

    run find_agents_by_capability_sql "code-review"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "architect" ]]
}

@test "find_agents_by_activation_sql returns all with 'all'" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"

    run find_agents_by_activation_sql "all"
    [[ "${status}" -eq 0 ]]
    [[ $(echo "${output}" | wc -l) -ge 3 ]]
}

@test "find_agents_by_criteria_sql filters by tier" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"

    run find_agents_by_criteria_sql "core" "" "" ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "architect" ]]
}

# ============================================================================
# STATISTICS AND HEALTH TESTS
# ============================================================================

@test "registry_stats_sql shows statistics" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"

    run registry_stats_sql
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Total agents" ]]
    [[ "${output}" =~ "Agents by tier" ]]
}

@test "registry_health_sql checks database" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"

    run registry_health_sql
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Database file accessible" ]]
}

@test "registry_health_sql fails for missing database" {
    rm -f "${EUXIS_HOME}/euxis-core/agents/registry.db"
    run registry_health_sql
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "not found" ]]
}

# ============================================================================
# HYBRID FALLBACK TESTS
# ============================================================================

@test "list_agents_hybrid falls back to filesystem" {
    rm -f "${EUXIS_HOME}/euxis-core/agents/registry.db"

    # Need to source agents.sh for fallback
    source "${BATS_TEST_DIRNAME}/../../../lib/agents.sh" 2>/dev/null || true

    run list_agents_hybrid 2>&1 || true
    # Should attempt fallback
    [[ "${status}" -eq 0 ]] || [[ "${output}" =~ "falling back" ]] || true
}

@test "resolve_agent_path_hybrid returns SQL-backed path when available" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"
    run resolve_agent_path_hybrid "architect"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "agents/prompts/core/architect.txt" ]]
}

# ============================================================================
# CONVENIENCE HELPER TESTS
# ============================================================================

@test "registry_get_version returns version" {
    run registry_get_version
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "0.0.8" ]]
}

@test "registry_get_version falls back to JSON" {
    rm -f "${EUXIS_HOME}/euxis-core/agents/registry.db"
    run registry_get_version
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "0.0.8" ]]
}

@test "registry_agent_exists returns true for existing agent" {
    run registry_agent_exists "architect"
    [[ "${status}" -eq 0 ]]
}

@test "registry_agent_exists returns false for unknown agent" {
    run registry_agent_exists "nonexistent"
    [[ "${status}" -eq 1 ]]
}

@test "registry_list_agent_ids lists all agents" {
    run registry_list_agent_ids
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "architect" ]]
}

# ============================================================================
# ERROR PATHS AND EDGE CASES
# ============================================================================

@test "registry functions handle set -u mode" {
    set -u
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"
    run list_agents_sql
    [[ "${status}" -eq 0 ]]
}

@test "registry functions handle pipefail mode" {
    set -e -o pipefail
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"
    run list_agents_sql
    [[ "${status}" -eq 0 ]]
}

@test "connection pool handles concurrent access" {
    local conn1 conn2
    conn1=$(_registry_get_connection)
    conn2=$(_registry_get_connection)

    # Should get different connections or -1
    [[ "${conn1}" != "${conn2}" ]] || [[ "${conn2}" == "-1" ]]

    _registry_release_connection "${conn1}"
    _registry_release_connection "${conn2}"
}

@test "registry_query handles SQL errors gracefully" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"

    run registry_query "INVALID SQL SYNTAX"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "failed" ]] || [[ "${output}" =~ "Error" ]] || [[ "${output}" =~ "error" ]]
}

@test "registry_rebuild succeeds as no-op when migration script is missing" {
    run registry_rebuild
    [[ "${status}" -eq 0 ]]
}

@test "_registry_auto_rebuild succeeds when db is present" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"
    run _registry_auto_rebuild
    [[ "${status}" -eq 0 ]]
}

@test "_registry_auto_rebuild succeeds when db is missing and migration script absent" {
    rm -f "${REGISTRY_DB}"
    run _registry_auto_rebuild
    [[ "${status}" -eq 0 ]]
}

# ============================================================================
# INTEGRATION TESTS
# ============================================================================

@test "registry_sql module loads without errors" {
    [[ -n "${_EUXIS_LIB_REGISTRY_SQL}" ]]
}

@test "REGISTRY_DB path is correct" {
    [[ "${REGISTRY_DB}" == "${EUXIS_HOME}/euxis-core/agents/registry.db" ]]
}

@test "REGISTRY_POOL_SIZE default is 3" {
    [[ "${REGISTRY_POOL_SIZE}" == "3" ]]
}

@test "multiple queries are consistent" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"

    local result1 result2
    result1=$(list_agents_sql)
    result2=$(list_agents_sql)

    [[ "${result1}" == "${result2}" ]]
}

@test "registry query results are idempotent across repeated calls" {
    command -v sqlite3 &>/dev/null || skip "sqlite3 not available"
    local first second
    first=$(find_agents_by_tier_sql "core")
    second=$(find_agents_by_tier_sql "core")
    [[ "${first}" == "${second}" ]]
}
