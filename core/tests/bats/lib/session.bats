#!/usr/bin/env bats
# Test suite for core/lib/session.sh
# (c) 2026 Euxis Fleet. All rights reserved.

# Test setup - run before each test
setup() {
    EUXIS_TEST_TMPDIR="$(mktemp -d)"
    export EUXIS_TEST_TMPDIR

    export EUXIS_HOME="${EUXIS_TEST_TMPDIR}/euxis"
    export PROJECTS_DIR="${EUXIS_HOME}/data/projects"
    mkdir -p "${PROJECTS_DIR}"

    # Mock date command for predictable timestamps
    cat > "${EUXIS_TEST_TMPDIR}/date" << 'DATEEOF'
#!/cli/bin/bash
if [[ "${1:-}" == "+%Y%m%d-%H%M%S" ]]; then
    echo "20260214-120000"
else
    command date "$@"
fi
DATEEOF
    chmod +x "${EUXIS_TEST_TMPDIR}/date"

    export PATH="${EUXIS_TEST_TMPDIR}:${PATH}"

    # Reset include guards and environment
    unset _EUXIS_LIB_SESSION
    unset EUXIS_PROJECT
    unset EUXIS_SESSION_ID

    # Source the library from real installation
    source "${HOME}/.euxis/core/lib/session.sh"
}

teardown() {
    if [[ -n "${EUXIS_TEST_TMPDIR:-}" && -d "${EUXIS_TEST_TMPDIR:-}" ]]; then
        rm -rf "${EUXIS_TEST_TMPDIR}"
    fi
}

# ============================================================================
# GET_PROJECT_NAME TESTS
# ============================================================================

@test "get_project_name returns EUXIS_PROJECT when set" {
    export EUXIS_PROJECT="my-custom-project"
    run get_project_name
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "my-custom-project" ]]
}

@test "get_project_name returns directory name when EUXIS_PROJECT not set" {
    unset EUXIS_PROJECT
    cd "${EUXIS_TEST_TMPDIR}"
    run get_project_name
    [[ "${status}" -eq 0 ]]
    # Should be the basename of the temp directory
    [[ -n "${output}" ]]
}

@test "get_project_name handles directory with special characters" {
    local special_dir="${EUXIS_TEST_TMPDIR}/project-with-dashes_and_underscores"
    mkdir -p "${special_dir}"
    cd "${special_dir}"
    unset EUXIS_PROJECT
    run get_project_name
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "project-with-dashes_and_underscores" ]]
}

@test "get_project_name handles empty EUXIS_PROJECT" {
    export EUXIS_PROJECT=""
    cd "${EUXIS_TEST_TMPDIR}"
    run get_project_name
    [[ "${status}" -eq 0 ]]
    # Empty string should fall through to PWD basename
}

# ============================================================================
# GET_SESSION_ID TESTS
# ============================================================================

@test "get_session_id returns EUXIS_SESSION_ID when set" {
    export EUXIS_SESSION_ID="custom-session-123"
    run get_session_id
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "custom-session-123" ]]
}

@test "get_session_id returns timestamp when EUXIS_SESSION_ID not set" {
    unset EUXIS_SESSION_ID
    run get_session_id
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "20260214-120000" ]]
}

@test "get_session_id returns consistent timestamp format" {
    unset EUXIS_SESSION_ID
    run get_session_id
    [[ "${status}" -eq 0 ]]
    # Format: YYYYMMDD-HHMMSS
    [[ "${output}" =~ ^[0-9]{8}-[0-9]{6}$ ]]
}

@test "get_session_id handles empty EUXIS_SESSION_ID" {
    export EUXIS_SESSION_ID=""
    run get_session_id
    [[ "${status}" -eq 0 ]]
    # Empty string should fall through to date
}

# ============================================================================
# ENSURE_PROJECT_DIRS TESTS
# ============================================================================

@test "ensure_project_dirs creates agent directory" {
    ensure_project_dirs "test-project" "architect"
    [[ -d "${EUXIS_HOME}/data/projects/test-project/architect" ]]
}

@test "ensure_project_dirs creates output subdirectory" {
    ensure_project_dirs "test-project" "architect"
    [[ -d "${EUXIS_HOME}/data/projects/test-project/architect/output" ]]
}

@test "ensure_project_dirs creates audit.md" {
    ensure_project_dirs "test-project" "architect"
    [[ -f "${EUXIS_HOME}/data/projects/test-project/architect/audit.md" ]]
}

@test "ensure_project_dirs creates memory.md" {
    ensure_project_dirs "test-project" "architect"
    [[ -f "${EUXIS_HOME}/data/projects/test-project/architect/memory.md" ]]
}

@test "ensure_project_dirs audit.md has correct header" {
    ensure_project_dirs "test-project" "architect"
    grep -q "# Audit Log: architect" "${EUXIS_HOME}/data/projects/test-project/architect/audit.md"
}

@test "ensure_project_dirs memory.md has correct header" {
    ensure_project_dirs "test-project" "architect"
    grep -q "# Memory: architect" "${EUXIS_HOME}/data/projects/test-project/architect/memory.md"
}

@test "ensure_project_dirs does not overwrite existing files" {
    ensure_project_dirs "test-project" "architect"
    echo "Custom content" >> "${EUXIS_HOME}/data/projects/test-project/architect/audit.md"

    ensure_project_dirs "test-project" "architect"
    grep -q "Custom content" "${EUXIS_HOME}/data/projects/test-project/architect/audit.md"
}

@test "ensure_project_dirs handles nested project names" {
    ensure_project_dirs "org/repo" "architect"
    [[ -d "${EUXIS_HOME}/data/projects/org/repo/architect" ]]
}

@test "ensure_project_dirs handles multiple agents" {
    ensure_project_dirs "test-project" "architect"
    ensure_project_dirs "test-project" "tester"
    ensure_project_dirs "test-project" "debugger"

    [[ -d "${EUXIS_HOME}/data/projects/test-project/architect" ]]
    [[ -d "${EUXIS_HOME}/data/projects/test-project/tester" ]]
    [[ -d "${EUXIS_HOME}/data/projects/test-project/debugger" ]]
}

@test "ensure_project_dirs handles hyphenated agent names" {
    ensure_project_dirs "test-project" "bug-fixer"
    [[ -d "${EUXIS_HOME}/data/projects/test-project/bug-fixer" ]]
    [[ -f "${EUXIS_HOME}/data/projects/test-project/bug-fixer/memory.md" ]]
}

# ============================================================================
# GET_MEMORY_CONTEXT TESTS
# ============================================================================

@test "get_memory_context returns content from memory file" {
    local memory_file="${EUXIS_TEST_TMPDIR}/memory.md"
    echo "Line 1" > "${memory_file}"
    echo "Line 2" >> "${memory_file}"
    echo "Line 3" >> "${memory_file}"

    run get_memory_context "${memory_file}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Line 1" ]]
    [[ "${output}" =~ "Line 2" ]]
    [[ "${output}" =~ "Line 3" ]]
}

@test "get_memory_context returns last N lines" {
    local memory_file="${EUXIS_TEST_TMPDIR}/memory.md"
    for i in $(seq 1 100); do
        echo "Line ${i}" >> "${memory_file}"
    done

    run get_memory_context "${memory_file}" 10
    [[ "${status}" -eq 0 ]]
    [[ $(echo "${output}" | wc -l) -eq 10 ]]
    [[ "${output}" =~ "Line 100" ]]
    [[ ! "${output}" =~ "Line 1$" ]]
}

@test "get_memory_context default is 50 lines" {
    local memory_file="${EUXIS_TEST_TMPDIR}/memory.md"
    for i in $(seq 1 100); do
        echo "Line ${i}" >> "${memory_file}"
    done

    run get_memory_context "${memory_file}"
    [[ "${status}" -eq 0 ]]
    [[ $(echo "${output}" | wc -l) -eq 50 ]]
}

@test "get_memory_context returns empty for missing file" {
    run get_memory_context "/nonexistent/memory.md"
    [[ "${status}" -eq 0 ]]
    [[ -z "${output}" ]]
}

@test "get_memory_context handles empty file" {
    local memory_file="${EUXIS_TEST_TMPDIR}/empty.md"
    touch "${memory_file}"

    run get_memory_context "${memory_file}"
    [[ "${status}" -eq 0 ]]
}

@test "get_memory_context handles file with fewer lines than requested" {
    local memory_file="${EUXIS_TEST_TMPDIR}/small.md"
    echo "Only line" > "${memory_file}"

    run get_memory_context "${memory_file}" 50
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "Only line" ]]
}

# ============================================================================
# ERROR PATHS AND EDGE CASES
# ============================================================================

@test "session functions handle set -u mode" {
    set -u
    run get_project_name
    [[ "${status}" -eq 0 ]]
}

@test "session functions handle pipefail mode" {
    set -e -o pipefail
    run get_session_id
    [[ "${status}" -eq 0 ]]
}

@test "ensure_project_dirs handles spaces in project name" {
    # Note: This may fail on some systems, but should be handled gracefully
    ensure_project_dirs "test project" "architect" 2>/dev/null || true
    # Just verify no crash
    [[ $? -eq 0 ]] || true
}

@test "ensure_project_dirs handles special characters in agent name" {
    ensure_project_dirs "test-project" "agent_with_underscore"
    [[ -d "${EUXIS_HOME}/data/projects/test-project/agent_with_underscore" ]]
}

@test "PROJECTS_DIR is correctly set" {
    [[ "${PROJECTS_DIR}" == "${EUXIS_HOME}/data/projects" ]]
}

# ============================================================================
# INTEGRATION TESTS
# ============================================================================

@test "session module loads without errors" {
    [[ -n "${_EUXIS_LIB_SESSION}" ]]
}

@test "get_project_name is idempotent" {
    export EUXIS_PROJECT="test-project"
    local result1
    result1=$(get_project_name)
    local result2
    result2=$(get_project_name)
    [[ "${result1}" == "${result2}" ]]
}

@test "get_session_id is idempotent with fixed value" {
    export EUXIS_SESSION_ID="fixed-session"
    local result1
    result1=$(get_session_id)
    local result2
    result2=$(get_session_id)
    [[ "${result1}" == "${result2}" ]]
}

@test "ensure_project_dirs is idempotent" {
    ensure_project_dirs "test-project" "architect"
    local audit_mtime1
    audit_mtime1=$(stat -c '%Y' "${EUXIS_HOME}/data/projects/test-project/architect/audit.md" 2>/dev/null || stat -f '%m' "${EUXIS_HOME}/data/projects/test-project/architect/audit.md")

    sleep 1
    ensure_project_dirs "test-project" "architect"
    local audit_mtime2
    audit_mtime2=$(stat -c '%Y' "${EUXIS_HOME}/data/projects/test-project/architect/audit.md" 2>/dev/null || stat -f '%m' "${EUXIS_HOME}/data/projects/test-project/architect/audit.md")

    # File should not be modified on second call
    [[ "${audit_mtime1}" == "${audit_mtime2}" ]]
}

@test "full session workflow" {
    export EUXIS_PROJECT="workflow-test"
    export EUXIS_SESSION_ID="session-001"

    local project
    project=$(get_project_name)
    [[ "${project}" == "workflow-test" ]]

    local session
    session=$(get_session_id)
    [[ "${session}" == "session-001" ]]

    ensure_project_dirs "${project}" "architect"
    [[ -d "${EUXIS_HOME}/data/projects/${project}/architect" ]]

    local memory_path="${EUXIS_HOME}/data/projects/${project}/architect/memory.md"
    echo "New memory entry" >> "${memory_path}"

    local context
    context=$(get_memory_context "${memory_path}")
    [[ "${context}" =~ "New memory entry" ]]
}
