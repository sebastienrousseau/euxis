#!/usr/bin/env bash
# Tests for lib/memory.sh

source "${EUXIS_HOME}/bin/lib/memory.sh"

# Setup: create a temporary memory file
TEST_TMPDIR=$(mktemp -d)
MEMORY_FILE="${TEST_TMPDIR}/memory.md"
cat > "${MEMORY_FILE}" <<'EOF'
# Memory: test-agent
[2026-01-28] SEMANTIC: The auth module uses JWT tokens with RS256 signing
[2026-01-28] EPISODIC: Fixed null pointer in auth.py line 89
[2026-01-29] SEMANTIC: Database connection pool max is 20
[2026-01-29] PROCEDURAL: To deploy: run tests → build → tag → push
[2026-01-30] EPISODIC: Performance regression found in search endpoint
[2026-01-30] SEMANTIC: Redis cache TTL is 300 seconds
[2026-01-31] EPISODIC: Updated CI pipeline to Node 20
[2026-01-31] PROCEDURAL: Debug auth: check token expiry → verify key → inspect claims
[2026-02-01] EPISODIC: Migrated from REST to GraphQL for user service
[2026-02-01] SEMANTIC: GraphQL schema version is 3.2
[2026-02-02] EPISODIC: Added rate limiting to public API endpoints
[2026-02-02] SEMANTIC: Rate limit is 100 req/min per user
EOF

# Test get_hot_memory: returns last 20 lines
hot=$(get_hot_memory "${MEMORY_FILE}")
line_count=$(echo "${hot}" | wc -l | tr -d ' ')
assert_eq "get_hot_memory returns all 13 lines (file < 20)" "13" "${line_count}"

# Test get_hot_memory: nonexistent file returns empty
empty_hot=$(get_hot_memory "${TEST_TMPDIR}/nonexistent.md")
assert_eq "get_hot_memory on missing file returns empty" "" "${empty_hot}"

# Test get_relevant_memory: keyword match
relevant=$(get_relevant_memory "${MEMORY_FILE}" "auth token signing")
assert_contains "relevant memory matches auth" "auth" "${relevant}"
assert_contains "relevant memory matches token" "token" "${relevant}"

# Test get_relevant_memory: no matches for irrelevant task
no_match=$(get_relevant_memory "${MEMORY_FILE}" "xy zw ab")
assert_eq "no relevant memory for gibberish" "" "${no_match}"

# Test get_cross_agent_memory: setup sibling
SIBLING_DIR="${TEST_TMPDIR}/project/sibling-agent"
CURRENT_DIR="${TEST_TMPDIR}/project/test-agent"
mkdir -p "${SIBLING_DIR}" "${CURRENT_DIR}"
echo "[2026-02-01] SEMANTIC: Authentication requires HTTPS in production" > "${SIBLING_DIR}/memory.md"
echo "[2026-02-01] SEMANTIC: Current test data" > "${CURRENT_DIR}/memory.md"

cross=$(get_cross_agent_memory "${TEST_TMPDIR}/project" "test-agent" "authentication production setup")
assert_contains "cross-agent finds sibling memory" "sibling-agent" "${cross}"
assert_contains "cross-agent matches keyword" "Authentication" "${cross}"

# Test build_tiered_memory: assembles all tiers
tiered=$(build_tiered_memory "${MEMORY_FILE}" "auth deployment" "${TEST_TMPDIR}/project" "test-agent")
assert_contains "tiered memory has Tier 1" "Tier 1" "${tiered}"
assert_contains "tiered memory has Tier 2" "Tier 2" "${tiered}"
assert_contains "tiered memory has Tier 3" "Tier 3" "${tiered}"

# Cleanup
rm -rf "${TEST_TMPDIR}"
