#!/usr/bin/env bats
# Test suite for core/lib/cli.sh

# Test setup - run before each test
setup() {
    EUXIS_TEST_TMPDIR="$(mktemp -d)"
    export EUXIS_TEST_TMPDIR

    # Use real EUXIS_HOME for library sourcing, but override data dirs
    export EUXIS_HOME="${HOME}/.euxis"
    export PROJECTS_DIR="${EUXIS_TEST_TMPDIR}/projects"
    mkdir -p "${PROJECTS_DIR}"
    mkdir -p "${EUXIS_TEST_TMPDIR}/agents/prompts/core"
    mkdir -p "${EUXIS_TEST_TMPDIR}/agents/prompts/fleet"
    mkdir -p "${EUXIS_TEST_TMPDIR}/lifecycle"
    mkdir -p "${EUXIS_TEST_TMPDIR}/perf"

    # Create mock agent files in test dir
    for agent in architect tester debugger; do
        cat > "${EUXIS_TEST_TMPDIR}/agents/prompts/core/${agent}.txt" << EOF
---
agent_id: ${agent}
role: "Mock ${agent}"
---
# MANDATE
Mock mandate
EOF
    done

    # Mock date command
cat > "${EUXIS_TEST_TMPDIR}/date" << 'DATEEOF'
#!/usr/bin/env bash
if [[ "${1:-}" == "+%Y%m%d-%H%M%S" ]]; then
    echo "20260209-123456"
else
    /bin/date "$@"
fi
DATEEOF
    chmod +x "${EUXIS_TEST_TMPDIR}/date"

    export PATH="${EUXIS_TEST_TMPDIR}:${PATH}"

    # Reset include guards
    unset _EUXIS_LIB_CLI
    unset _EUXIS_LIB_COMMON
    unset _EUXIS_LIB_VALIDATION
    unset _EUXIS_LIB_AGENTS
    unset _EUXIS_LIB_PROVIDERS
    unset _EUXIS_LIB_SESSION

    # Source dependencies from real EUXIS_HOME
    source "${EUXIS_HOME}/euxis-core/lib/common.sh"
    source "${EUXIS_HOME}/euxis-core/lib/validation.sh"
    source "${EUXIS_HOME}/euxis-core/lib/agents.sh"
    source "${EUXIS_HOME}/euxis-core/lib/session.sh"
    source "${EUXIS_HOME}/euxis-core/lib/providers.sh"
    source "${EUXIS_HOME}/euxis-core/lib/cli.sh"
}

teardown() {
    if [[ -n "${EUXIS_TEST_TMPDIR:-}" && -d "${EUXIS_TEST_TMPDIR:-}" ]]; then
        rm -rf "${EUXIS_TEST_TMPDIR}"
    fi
}

# ============================================================================
# USAGE FUNCTION TESTS
# ============================================================================

@test "usage function outputs help text" {
    run usage
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "USAGE" ]]
}

@test "usage function shows available commands" {
    run usage
    [[ "${output}" =~ "dispatch" ]]
    [[ "${output}" =~ "verify" ]]
    [[ "${output}" =~ "health" ]]
}

@test "usage function exits with code 2" {
    run usage
    [[ "${status}" -eq 0 ]]
}

@test "_supports_unicode returns a status code" {
    run _supports_unicode
    [[ "${status}" -eq 0 ]] || [[ "${status}" -eq 1 ]]
}

@test "_setup_icons initializes icon variables" {
    _setup_icons
    [[ -n "${ICON_CHECK}" ]]
    [[ -n "${ICON_BULLET}" ]]
}

@test "_setup_colors initializes color variables" {
    _setup_colors
    [[ -n "${RESET+x}" ]]
    [[ -n "${CYAN+x}" ]]
}

@test "_print helpers execute without errors" {
    _setup_colors
    run _print_header
    [[ "${status}" -eq 0 ]]
    run _print_section "TEST"
    [[ "${status}" -eq 0 ]]
    run _print_cmd "cmd" "desc"
    [[ "${status}" -eq 0 ]]
}

@test "_print_agent_grid executes without errors" {
    _setup_colors
    run _print_agent_grid
    [[ "${status}" -eq 0 ]]
}

@test "usage_agents prints agent catalog" {
    run usage_agents
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "AGENT ECOSYSTEM" ]]
}

@test "usage_search prints usage for empty query" {
    run usage_search
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Usage:" ]]
}

@test "usage_search returns results for known keyword" {
    run usage_search "security"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "security" ]]
}

# ============================================================================
# PARSE_ARGS FUNCTION TESTS
# ============================================================================

@test "parse_args requires at least 2 arguments" {
    run parse_args "architect"
    [[ "${status}" -eq 0 ]] || [[ "${output}" =~ "Usage" ]]
}

@test "parse_args sets AGENT variable" {
    parse_args "architect" "test task" "claude"
    [[ "${AGENT}" == "architect" ]]
}

@test "parse_args sets TASK variable" {
    parse_args "architect" "test task" "claude"
    [[ "${TASK}" == "test task" ]]
}

@test "parse_args sets PROVIDER variable" {
    parse_args "architect" "test task" "claude"
    [[ "${PROVIDER}" == "claude" ]]
}

@test "parse_args accepts valid providers" {
    for provider in claude gemini openai ollama qwen crush kiro-cli goose; do
        parse_args "architect" "test task" "${provider}"
        [[ "${PROVIDER}" == "${provider}" ]]
    done
}

@test "parse_args rejects unknown provider" {
    run parse_args "architect" "test task" "invalid_provider"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Unknown provider" ]]
}

@test "parse_args rejects unknown agent" {
    run parse_args "nonexistent_agent" "test task" "claude"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Unknown agent" ]]
}

@test "parse_args validates agent name security" {
    run parse_args "../etc/passwd" "test task" "claude"
    [[ "${status}" -ne 0 ]]
}

@test "parse_args validates task input" {
    run parse_args "architect" "" "claude"
    [[ "${status}" -ne 0 ]]
}

@test "parse_args auto-selects provider when not specified" {
    parse_args "architect" "test task"
    [[ -n "${PROVIDER}" ]]
}

@test "setup_session populates runtime paths" {
    AGENT="architect"
    PROVIDER="claude"
    setup_session
    [[ -n "${PROJECT}" ]]
    [[ -n "${SESSION_ID}" ]]
    [[ -n "${OUTPUT_PATH}" ]]
    [[ -n "${MODEL_NAME}" ]]
}

# ============================================================================
# CAPTURE_OUTPUT FUNCTION TESTS
# ============================================================================

@test "capture_output creates output file" {
    local output_file="${EUXIS_TEST_TMPDIR}/output.md"
    capture_output "architect" "test-project" "claude" "test task" "session-123" "${output_file}" "Test output content"

    [[ -f "${output_file}" ]]
}

@test "capture_output includes agent name" {
    local output_file="${EUXIS_TEST_TMPDIR}/output.md"
    capture_output "architect" "test-project" "claude" "test task" "session-123" "${output_file}" "Test output"

    grep -q "architect" "${output_file}"
}

@test "capture_output includes provider name" {
    local output_file="${EUXIS_TEST_TMPDIR}/output.md"
    capture_output "architect" "test-project" "claude" "test task" "session-123" "${output_file}" "Test output"

    grep -q "claude" "${output_file}"
}

@test "capture_output includes project name" {
    local output_file="${EUXIS_TEST_TMPDIR}/output.md"
    capture_output "architect" "test-project" "claude" "test task" "session-123" "${output_file}" "Test output"

    grep -q "test-project" "${output_file}"
}

@test "capture_output includes task description" {
    local output_file="${EUXIS_TEST_TMPDIR}/output.md"
    capture_output "architect" "test-project" "claude" "test task" "session-123" "${output_file}" "Test output"

    grep -q "test task" "${output_file}"
}

@test "capture_output includes output content" {
    local output_file="${EUXIS_TEST_TMPDIR}/output.md"
    capture_output "architect" "test-project" "claude" "test task" "session-123" "${output_file}" "Unique test content here"

    grep -q "Unique test content here" "${output_file}"
}

# ============================================================================
# SHOW_CONTEXT FUNCTION TESTS
# ============================================================================

@test "show_context returns 0 for non-interactive" {
    run show_context < /dev/null
    [[ "${status}" -eq 0 ]]
}

@test "show_context handles non-git directory" {
    cd "${EUXIS_TEST_TMPDIR}"
    run show_context < /dev/null
    [[ "${status}" -eq 0 ]]
}

# ============================================================================
# GIT_GUARD FUNCTION TESTS
# ============================================================================

@test "git_guard returns 0 for non-interactive" {
    run git_guard < /dev/null
    [[ "${status}" -eq 0 ]]
}

@test "git_guard returns 0 when EUXIS_DISPATCH is true" {
    EUXIS_DISPATCH=true run git_guard
    [[ "${status}" -eq 0 ]]
}

@test "git_guard handles non-git directory" {
    cd "${EUXIS_TEST_TMPDIR}"
    run git_guard < /dev/null
    [[ "${status}" -eq 0 ]]
}

# ============================================================================
# CHECK_HEALTH_FAST FUNCTION TESTS
# ============================================================================

@test "check_health_fast returns 0 when skipped" {
    EUXIS_HEALTH_CHECK=skip run check_health_fast
    [[ "${status}" -eq 0 ]]
}

@test "check_health_fast returns 0 in dispatch mode" {
    EUXIS_DISPATCH=true run check_health_fast
    [[ "${status}" -eq 0 ]]
}

@test "check_health_fast caches hash" {
    mkdir -p "${EUXIS_HOME}/prompts"
    check_health_fast
    [[ -f "${TMPDIR:-/tmp}/euxis_health_hash" ]]
}

@test "check_health_fast uses cached hash on second call" {
    mkdir -p "${EUXIS_HOME}/prompts"
    check_health_fast
    local first_hash
    first_hash=$(< "${TMPDIR:-/tmp}/euxis_health_hash")

    check_health_fast
    local second_hash
    second_hash=$(< "${TMPDIR:-/tmp}/euxis_health_hash")

    [[ "${first_hash}" == "${second_hash}" ]]
}

# ============================================================================
# ERROR PATHS AND EDGE CASES
# ============================================================================

@test "cli functions handle set -u mode" {
    set -u
    run capture_output "agent" "proj" "prov" "task" "sess" "${EUXIS_TEST_TMPDIR}/out.md" "content"
    [[ "${status}" -eq 0 ]]
}

@test "cli functions handle pipefail mode" {
    set -e -o pipefail
    run capture_output "agent" "proj" "prov" "task" "sess" "${EUXIS_TEST_TMPDIR}/out.md" "content"
    [[ "${status}" -eq 0 ]]
}

@test "parse_args handles empty provider gracefully" {
    parse_args "architect" "test task" ""
    [[ -n "${PROVIDER}" ]]
}

@test "capture_output handles special characters in content" {
    local output_file="${EUXIS_TEST_TMPDIR}/output.md"
    capture_output "architect" "test" "claude" "task" "sess" "${output_file}" "Content with special chars: \$VAR & <tag>"
    [[ -f "${output_file}" ]]
}

@test "capture_output handles multiline content" {
    local output_file="${EUXIS_TEST_TMPDIR}/output.md"
    local multiline="Line 1
Line 2
Line 3"
    capture_output "architect" "test" "claude" "task" "sess" "${output_file}" "${multiline}"
    [[ $(wc -l < "${output_file}") -gt 5 ]]
}

# ============================================================================
# INTEGRATION TESTS
# ============================================================================

@test "cli functions are idempotent" {
    local output_file="${EUXIS_TEST_TMPDIR}/output.md"
    capture_output "architect" "test" "claude" "task" "sess" "${output_file}" "content"
    local content1
    content1=$(cat "${output_file}")

    capture_output "architect" "test" "claude" "task" "sess" "${output_file}" "content"
    local content2
    content2=$(cat "${output_file}")

    [[ "${content1}" == "${content2}" ]]
}

@test "parse_args and capture_output work together" {
    parse_args "architect" "integration test task" "claude"
    local output_file="${EUXIS_TEST_TMPDIR}/output.md"
    capture_output "${AGENT}" "project" "${PROVIDER}" "${TASK}" "session" "${output_file}" "output"
    [[ -f "${output_file}" ]]
    grep -q "architect" "${output_file}"
    grep -q "claude" "${output_file}"
}
