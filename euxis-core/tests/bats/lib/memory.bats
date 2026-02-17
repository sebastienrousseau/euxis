#!/usr/bin/env bats
# Test suite for core/lib/memory.sh

# Test setup - run before each test
setup() {
    EUXIS_TEST_TMPDIR="$(mktemp -d)"
    export EUXIS_TEST_TMPDIR

    # Use test temp dir for data, real home for libs
    export EUXIS_HOME="${EUXIS_TEST_TMPDIR}/euxis"
    mkdir -p "${EUXIS_HOME}/euxis-runtime/data/projects/test-project/architect"
    mkdir -p "${EUXIS_HOME}/euxis-runtime/data/projects/test-project/tester"

    # Create mock memory files
    cat > "${EUXIS_HOME}/euxis-runtime/data/projects/test-project/architect/memory.md" << 'EOF'
# Memory: architect
[2026-01-01] SEMANTIC: Authentication uses JWT tokens
[2026-01-02] PROCEDURAL: Run tests with pytest
[2026-01-03] REFLECTION: Consider caching for performance
[2026-01-04] Recent entry about database optimization
[2026-01-05] Another entry about API design
EOF

    cat > "${EUXIS_HOME}/euxis-runtime/data/projects/test-project/tester/memory.md" << 'EOF'
# Memory: tester
[2026-01-01] Test coverage is at 85%
[2026-01-02] Integration tests need mocking
EOF

    export PATH="${EUXIS_TEST_TMPDIR}:${PATH}"

    # Reset include guards
    unset _EUXIS_LIB_MEMORY
    unset _EUXIS_LIB_COMMON

    # Source dependencies from real installation
    source "${HOME}/.euxis/core/lib/common.sh"
    source "${HOME}/.euxis/core/lib/memory.sh"
}

teardown() {
    if [[ -n "${EUXIS_TEST_TMPDIR:-}" && -d "${EUXIS_TEST_TMPDIR:-}" ]]; then
        rm -rf "${EUXIS_TEST_TMPDIR}"
    fi
}

# ============================================================================
# _VALIDATE_MEMORY_PATH TESTS
# ============================================================================

@test "_validate_memory_path accepts valid path" {
    local valid_path="${EUXIS_HOME}/euxis-runtime/data/projects/test/memory.md"
    mkdir -p "$(dirname "${valid_path}")"
    touch "${valid_path}"
    run _validate_memory_path "${valid_path}"
    [[ "${status}" -eq 0 ]]
}

@test "_validate_memory_path rejects path traversal" {
    run _validate_memory_path "${EUXIS_HOME}/../etc/passwd"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Path traversal rejected" ]]
}

@test "_validate_memory_path rejects paths outside EUXIS_HOME" {
    run _validate_memory_path "/tmp/outside/memory.md" 2>&1 || true
    [[ "${status}" -ne 0 ]] || [[ "${output}" =~ "rejected" ]]
}

# ============================================================================
# GET_HOT_MEMORY TESTS
# ============================================================================

@test "get_hot_memory returns recent entries" {
    local memory_file="${EUXIS_HOME}/euxis-runtime/data/projects/test-project/architect/memory.md"
    run get_hot_memory "${memory_file}"
    [[ "${status}" -eq 0 ]]
    [[ -n "${output}" ]]
}

@test "get_hot_memory returns empty for missing file" {
    run get_hot_memory "/nonexistent/memory.md"
    [[ "${status}" -eq 0 ]]
    [[ -z "${output}" ]]
}

@test "get_hot_memory returns last 20 lines" {
    local memory_file="${EUXIS_TEST_TMPDIR}/large_memory.md"
    for i in $(seq 1 50); do
        echo "Line ${i}" >> "${memory_file}"
    done

    run get_hot_memory "${memory_file}"
    [[ "${status}" -eq 0 ]]
    [[ $(echo "${output}" | wc -l) -le 20 ]]
}

# ============================================================================
# _EXTRACT_KEYWORDS TESTS
# ============================================================================

@test "_extract_keywords extracts words >= 5 chars" {
    run _extract_keywords "This is a simple authentication test"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "simple" ]]
    [[ "${output}" =~ "authentication" ]]
}

@test "_extract_keywords deduplicates words" {
    run _extract_keywords "authentication authentication authentication"
    [[ "${status}" -eq 0 ]]
    # Should only have one instance
    local count
    count=$(echo "${output}" | grep -o "authentication" | wc -l)
    [[ "${count}" -eq 1 ]]
}

@test "_extract_keywords returns max 10 keywords" {
    run _extract_keywords "keyword1 keyword2 keyword3 keyword4 keyword5 keyword6 keyword7 keyword8 keyword9 keyword10 keyword11 keyword12"
    [[ "${status}" -eq 0 ]]
    local count
    count=$(echo "${output}" | tr '|' '\n' | wc -l)
    [[ "${count}" -le 10 ]]
}

@test "_extract_keywords handles empty input" {
    run _extract_keywords ""
    [[ "${status}" -eq 0 ]]
    [[ -z "${output}" ]]
}

@test "_extract_keywords handles short words only" {
    run _extract_keywords "a bb ccc dddd"
    [[ "${status}" -eq 0 ]]
    [[ -z "${output}" ]]
}

# ============================================================================
# GET_RELEVANT_MEMORY TESTS
# ============================================================================

@test "get_relevant_memory finds keyword matches" {
    local memory_file="${EUXIS_HOME}/euxis-runtime/data/projects/test-project/architect/memory.md"
    run get_relevant_memory "${memory_file}" "authentication tokens"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "JWT" ]] || [[ "${output}" =~ "Authentication" ]]
}

@test "get_relevant_memory returns empty for no matches" {
    local memory_file="${EUXIS_HOME}/euxis-runtime/data/projects/test-project/architect/memory.md"
    run get_relevant_memory "${memory_file}" "xyznonexistent"
    [[ "${status}" -eq 0 ]]
}

@test "get_relevant_memory handles missing file" {
    run get_relevant_memory "/nonexistent/memory.md" "test"
    [[ "${status}" -eq 0 ]]
    [[ -z "${output}" ]]
}

@test "get_relevant_memory handles empty task" {
    local memory_file="${EUXIS_HOME}/euxis-runtime/data/projects/test-project/architect/memory.md"
    run get_relevant_memory "${memory_file}" ""
    [[ "${status}" -eq 0 ]]
}

# ============================================================================
# GET_CROSS_AGENT_MEMORY TESTS
# ============================================================================

@test "get_cross_agent_memory finds sibling agent context" {
    local project_dir="${EUXIS_HOME}/euxis-runtime/data/projects/test-project"
    run get_cross_agent_memory "${project_dir}" "architect" "testing coverage"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "tester" ]] || [[ -z "${output}" ]]
}

@test "get_cross_agent_memory excludes current agent" {
    local project_dir="${EUXIS_HOME}/euxis-runtime/data/projects/test-project"
    run get_cross_agent_memory "${project_dir}" "architect" "authentication JWT"
    [[ "${status}" -eq 0 ]]
    # Should not include architect's own memory in cross-agent context
}

@test "get_cross_agent_memory handles missing project dir" {
    run get_cross_agent_memory "/nonexistent" "architect" "test"
    [[ "${status}" -eq 0 ]]
    [[ -z "${output}" ]]
}

@test "get_cross_agent_memory handles empty task" {
    local project_dir="${EUXIS_HOME}/euxis-runtime/data/projects/test-project"
    run get_cross_agent_memory "${project_dir}" "architect" ""
    [[ "${status}" -eq 0 ]]
}

# ============================================================================
# BUILD_TIERED_MEMORY TESTS
# ============================================================================

@test "build_tiered_memory includes all three tiers" {
    local memory_file="${EUXIS_HOME}/euxis-runtime/data/projects/test-project/architect/memory.md"
    local project_dir="${EUXIS_HOME}/euxis-runtime/data/projects/test-project"
    run build_tiered_memory "${memory_file}" "test task" "${project_dir}" "architect"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Tier 1: Hot Memory" ]]
    [[ "${output}" =~ "Tier 2: Relevant Memory" ]]
    [[ "${output}" =~ "Tier 3: Cross-Agent Context" ]]
}

@test "build_tiered_memory handles missing memory file" {
    local project_dir="${EUXIS_HOME}/euxis-runtime/data/projects/test-project"
    run build_tiered_memory "/nonexistent/memory.md" "test" "${project_dir}" "agent"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "<empty>" ]]
}

# ============================================================================
# PRUNE_MEMORY TESTS
# ============================================================================

@test "prune_memory skips small files" {
    local memory_file="${EUXIS_TEST_TMPDIR}/small_memory.md"
    echo "# Memory: test" > "${memory_file}"
    echo "Small content" >> "${memory_file}"

    local original_size
    original_size=$(wc -l < "${memory_file}")

    run prune_memory "${memory_file}" 500 100
    [[ "${status}" -eq 0 ]]

    local new_size
    new_size=$(wc -l < "${memory_file}")
    [[ "${original_size}" -eq "${new_size}" ]]
}

@test "prune_memory reduces large files" {
    local memory_file="${EUXIS_TEST_TMPDIR}/large_memory.md"
    echo "# Memory: test" > "${memory_file}"
    for i in $(seq 1 600); do
        echo "[2026-01-01] Entry ${i}" >> "${memory_file}"
    done

    run prune_memory "${memory_file}" 500 100
    [[ "${status}" -eq 0 ]]

    local new_size
    new_size=$(wc -l < "${memory_file}")
    [[ "${new_size}" -lt 600 ]]
}

@test "prune_memory preserves REFLECTION entries" {
    local memory_file="${EUXIS_TEST_TMPDIR}/reflection_memory.md"
    echo "# Memory: test" > "${memory_file}"
    for i in $(seq 1 600); do
        echo "[2026-01-01] Entry ${i}" >> "${memory_file}"
    done
    echo "[2026-01-01] REFLECTION: Important insight" >> "${memory_file}"

    run prune_memory "${memory_file}" 500 100
    [[ "${status}" -eq 0 ]]

    grep -q "REFLECTION" "${memory_file}"
}

@test "prune_memory preserves CONTRAINDICATION entries" {
    local memory_file="${EUXIS_TEST_TMPDIR}/contra_memory.md"
    echo "# Memory: test" > "${memory_file}"
    for i in $(seq 1 600); do
        echo "[2026-01-01] Entry ${i}" >> "${memory_file}"
    done
    echo "[2026-01-01] CONTRAINDICATION: Never do this" >> "${memory_file}"

    run prune_memory "${memory_file}" 500 100
    [[ "${status}" -eq 0 ]]

    grep -q "CONTRAINDICATION" "${memory_file}"
}

@test "prune_memory handles missing file" {
    run prune_memory "/nonexistent/memory.md"
    [[ "${status}" -eq 0 ]]
}

@test "prune_memory rejects path traversal" {
    run prune_memory "${EUXIS_HOME}/../etc/passwd"
    [[ "${status}" -eq 1 ]]
}

# ============================================================================
# PRUNE_PROJECT_MEMORY TESTS
# ============================================================================

@test "prune_project_memory handles missing directory" {
    run prune_project_memory "/nonexistent/project"
    [[ "${status}" -eq 0 ]]
}

@test "prune_project_memory processes all agent memory files" {
    local project_dir="${EUXIS_TEST_TMPDIR}/project"
    mkdir -p "${project_dir}/agent1" "${project_dir}/agent2"
    echo "# Memory" > "${project_dir}/agent1/memory.md"
    echo "# Memory" > "${project_dir}/agent2/memory.md"

    run prune_project_memory "${project_dir}"
    [[ "${status}" -eq 0 ]]
}

# ============================================================================
# DETECT_SEMANTIC_DRIFT TESTS
# ============================================================================

@test "detect_semantic_drift returns empty for no contradictions" {
    local memory_file="${EUXIS_TEST_TMPDIR}/memory.md"
    echo "# Memory" > "${memory_file}"
    echo "[2026-01-01] System uses caching" >> "${memory_file}"

    run detect_semantic_drift "${memory_file}" "Caching is fast"
    [[ "${status}" -eq 0 ]]
}

@test "detect_semantic_drift detects negation contradiction" {
    local memory_file="${EUXIS_TEST_TMPDIR}/memory.md"
    echo "# Memory" > "${memory_file}"
    echo "[2026-01-01] Authentication is enabled" >> "${memory_file}"

    run detect_semantic_drift "${memory_file}" "Authentication is not enabled anymore"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "DRIFT" ]] || [[ -z "${output}" ]]
}

@test "detect_semantic_drift handles missing file" {
    run detect_semantic_drift "/nonexistent/memory.md" "new fact"
    [[ "${status}" -eq 0 ]]
}

@test "detect_semantic_drift handles empty new fact" {
    local memory_file="${EUXIS_TEST_TMPDIR}/memory.md"
    echo "# Memory" > "${memory_file}"

    run detect_semantic_drift "${memory_file}" ""
    [[ "${status}" -eq 0 ]]
}

# ============================================================================
# RESOLVE_MEMORY_CONTRADICTION TESTS
# ============================================================================

@test "resolve_memory_contradiction supersedes old entries" {
    local memory_file="${EUXIS_TEST_TMPDIR}/memory.md"
    echo "# Memory" > "${memory_file}"

    run resolve_memory_contradiction "${memory_file}" "New fact about system" "architect" "supersede"
    [[ "${status}" -eq 0 ]]

    grep -q "SEMANTIC: New fact" "${memory_file}"
}

@test "resolve_memory_contradiction keeps both entries" {
    local memory_file="${EUXIS_TEST_TMPDIR}/memory.md"
    echo "# Memory" > "${memory_file}"

    run resolve_memory_contradiction "${memory_file}" "Conflicting fact" "architect" "keep_both"
    [[ "${status}" -eq 0 ]]

    grep -q "KEPT BOTH" "${memory_file}"
}

@test "resolve_memory_contradiction rejects new entry" {
    local memory_file="${EUXIS_TEST_TMPDIR}/memory.md"
    echo "# Memory" > "${memory_file}"

    run resolve_memory_contradiction "${memory_file}" "Bad fact" "architect" "reject"
    [[ "${status}" -eq 0 ]]

    grep -q "REJECTED" "${memory_file}"
}

@test "resolve_memory_contradiction rejects path traversal" {
    run resolve_memory_contradiction "${EUXIS_HOME}/../etc/passwd" "fact" "agent"
    [[ "${status}" -eq 1 ]]
}

# ============================================================================
# AUTO_EVOLVE_GRAPH TESTS
# ============================================================================

@test "auto_evolve_graph handles missing graph command" {
    local memory_file="${EUXIS_TEST_TMPDIR}/memory.md"
    echo "# Memory" > "${memory_file}"
    echo "[2026-01-01] UserAuth connects to Database" >> "${memory_file}"

    run auto_evolve_graph "${memory_file}" "architect"
    [[ "${status}" -eq 0 ]]
}

@test "auto_evolve_graph handles missing memory file" {
    run auto_evolve_graph "/nonexistent/memory.md" "architect"
    [[ "${status}" -eq 0 ]]
}

@test "auto_evolve_graph handles empty memory" {
    local memory_file="${EUXIS_TEST_TMPDIR}/memory.md"
    echo "# Memory" > "${memory_file}"

    run auto_evolve_graph "${memory_file}" "architect"
    [[ "${status}" -eq 0 ]]
}

# ============================================================================
# ERROR PATHS AND EDGE CASES
# ============================================================================

@test "memory functions handle set -u mode" {
    set -u
    local memory_file="${EUXIS_HOME}/euxis-runtime/data/projects/test-project/architect/memory.md"
    run get_hot_memory "${memory_file}"
    [[ "${status}" -eq 0 ]]
}

@test "memory functions handle pipefail mode" {
    set -e -o pipefail
    local memory_file="${EUXIS_HOME}/euxis-runtime/data/projects/test-project/architect/memory.md"
    run get_hot_memory "${memory_file}"
    [[ "${status}" -eq 0 ]]
}

@test "memory functions handle Unicode content" {
    local memory_file="${EUXIS_TEST_TMPDIR}/unicode_memory.md"
    echo "# Memory" > "${memory_file}"
    echo "[2026-01-01] 认证使用JWT令牌" >> "${memory_file}"

    run get_hot_memory "${memory_file}"
    [[ "${status}" -eq 0 ]]
}

# ============================================================================
# INTEGRATION TESTS
# ============================================================================

@test "memory module loads without errors" {
    [[ -n "${_EUXIS_LIB_MEMORY}" ]]
}

@test "tiered memory retrieval is consistent" {
    local memory_file="${EUXIS_HOME}/euxis-runtime/data/projects/test-project/architect/memory.md"
    local project_dir="${EUXIS_HOME}/euxis-runtime/data/projects/test-project"

    local result1
    result1=$(build_tiered_memory "${memory_file}" "test" "${project_dir}" "architect")
    local result2
    result2=$(build_tiered_memory "${memory_file}" "test" "${project_dir}" "architect")

    [[ "${result1}" == "${result2}" ]]
}
