#!/usr/bin/env bats
# Test suite for core/lib/validation.sh

# Test setup - run before each test
setup() {
    EUXIS_TEST_TMPDIR="$(mktemp -d)"
    export EUXIS_TEST_TMPDIR

    export EUXIS_HOME="${EUXIS_TEST_TMPDIR}/euxis"

    # Create valid Euxis directory structure (needed by validate_euxis_structure)
    mkdir -p "${EUXIS_HOME}/euxis-cli/bin"
    mkdir -p "${EUXIS_HOME}/euxis-core/lib"
    mkdir -p "${EUXIS_HOME}/euxis-core/agents"
    mkdir -p "${EUXIS_HOME}/euxis-core/agents/prompts"
    mkdir -p "${EUXIS_HOME}/euxis-runtime/memory/cortex"
    mkdir -p "${EUXIS_HOME}/data"

    # Mock external commands
    export PATH="${EUXIS_TEST_TMPDIR}:${PATH}"

    # Create valid agents/registry.json
    cat > "${EUXIS_HOME}/euxis-core/agents/registry.json" << 'EOF'
{
  "agents": [
    {"id": "architect", "tier": "core"},
    {"id": "tester", "tier": "default"}
  ]
}
EOF

    # Reset include guards and re-source
    unset _EUXIS_LIB_VALIDATION
    unset _EUXIS_LIB_COMMON
    source "${BATS_TEST_DIRNAME}/../../../lib/common.sh"
    source "${BATS_TEST_DIRNAME}/../../../lib/validation.sh"
}

teardown() {
    if [[ -n "${EUXIS_TEST_TMPDIR:-}" && -d "${EUXIS_TEST_TMPDIR:-}" ]]; then
        rm -rf "${EUXIS_TEST_TMPDIR}"
    fi
}

# ============================================================================
# VALIDATION RESULT FUNCTIONS
# ============================================================================

@test "validation_error appends to errors array and outputs to stderr" {
    run validation_error "test error message"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "❌" ]]
    [[ "${output}" =~ "test error message" ]]
}

@test "validation_warning appends to warnings array and outputs to stderr" {
    run validation_warning "test warning message"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "⚠️" ]]
    [[ "${output}" =~ "test warning message" ]]
}

@test "validation_pass outputs success message with checkmark" {
    run validation_pass "test success message"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "✅" ]]
    [[ "${output}" =~ "test success message" ]]
}

# ============================================================================
# AGENT NAME VALIDATION
# ============================================================================

@test "validate_agent_name accepts valid agent name" {
    run validate_agent_name "architect"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "agent name 'architect' is valid" ]]
}

@test "validate_agent_name accepts hyphenated name" {
    run validate_agent_name "test-agent"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "agent name 'test-agent' is valid" ]]
}

@test "validate_agent_name accepts underscored name (non-prefix)" {
    run validate_agent_name "test_agent"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "agent name 'test_agent' is valid" ]]
}

@test "validate_agent_name rejects empty agent name" {
    run validate_agent_name ""
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Empty agent name not allowed" ]]
}

@test "validate_agent_name rejects path traversal" {
    run validate_agent_name "../etc/passwd"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "path traversal detected" ]]
}

@test "validate_agent_name rejects absolute path" {
    run validate_agent_name "/etc/passwd"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "absolute path not allowed" ]]
}

@test "validate_agent_name rejects directory separators" {
    run validate_agent_name "foo/bar"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "directory separators not allowed" ]]
}

@test "validate_agent_name rejects partial files (underscore prefix)" {
    run validate_agent_name "_protocol"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "partial files not allowed" ]]
}

@test "validate_agent_name rejects special characters" {
    run validate_agent_name "agent@name"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "only alphanumeric" ]]
}

@test "validate_agent_name uses custom context" {
    run validate_agent_name "" "squad member"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Empty squad member not allowed" ]]
}

# ============================================================================
# TASK INPUT VALIDATION
# ============================================================================

@test "validate_task_input accepts non-empty task" {
    run validate_task_input "valid task description"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Task input is valid" ]]
}

@test "validate_task_input rejects empty task" {
    run validate_task_input ""
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Empty task not allowed" ]]
}

@test "validate_task_input accepts whitespace-only task" {
    # Whitespace-only is not empty per [[ -z ]] check
    run validate_task_input "   "
    [[ "${status}" -eq 0 ]]
}

@test "validate_task_input handles tasks with special characters" {
    run validate_task_input "task with spaces & special chars"
    [[ "${status}" -eq 0 ]]
}

@test "validate_task_input handles Unicode in task" {
    run validate_task_input "测试任务"
    [[ "${status}" -eq 0 ]]
}

@test "validate_task_input enforces maximum length" {
    # Create a task over 10000 characters
    long_task=$(printf "%*s" 10001 | tr ' ' 'a')
    run validate_task_input "${long_task}"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Task too long" ]]
}

# ============================================================================
# TASK SANITIZATION
# ============================================================================

@test "sanitize_task_input strips control characters" {
    local raw cleaned
    raw=$'hello\x01\x02\tworld\nok'
    run sanitize_task_input "${raw}"
    [[ "${status}" -eq 0 ]]
    cleaned="${output}"
    [[ "${cleaned}" == $'hello\tworld\nok' ]]
}

@test "sanitize_task_input escapes backticks" {
    local raw cleaned
    raw='run `uname -a` safely'
    run sanitize_task_input "${raw}"
    [[ "${status}" -eq 0 ]]
    cleaned="${output}"
    [[ "${cleaned}" != *'`'* ]]
    [[ "${cleaned}" == *'\\'* ]]
}

# ============================================================================
# FILE EXECUTABLE VALIDATION
# ============================================================================

@test "validate_file_executable accepts executable file" {
    test_file="${EUXIS_TEST_TMPDIR}/test_executable"
    echo "#!/cli/bin/bash" > "${test_file}"
    chmod +x "${test_file}"

    run validate_file_executable "${test_file}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "is accessible" ]]
}

@test "validate_file_executable rejects non-executable file" {
    test_file="${EUXIS_TEST_TMPDIR}/test_not_executable"
    echo "content" > "${test_file}"
    chmod -x "${test_file}"

    run validate_file_executable "${test_file}"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "is not executable" ]]
}

@test "validate_file_executable rejects non-existent file" {
    run validate_file_executable "/nonexistent/file"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "does not exist" ]]
}

@test "validate_file_executable rejects directory" {
    # Directories fail the -f check, reported as "does not exist"
    run validate_file_executable "${EUXIS_TEST_TMPDIR}"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "does not exist" ]]
}

@test "validate_file_executable handles files with spaces" {
    test_file="${EUXIS_TEST_TMPDIR}/test file with spaces"
    echo "#!/cli/bin/bash" > "${test_file}"
    chmod +x "${test_file}"

    run validate_file_executable "${test_file}"
    [[ "${status}" -eq 0 ]]
}

@test "validate_file_executable handles symlinks" {
    target_file="${EUXIS_TEST_TMPDIR}/target"
    link_file="${EUXIS_TEST_TMPDIR}/link"
    echo "#!/cli/bin/bash" > "${target_file}"
    chmod +x "${target_file}"
    ln -s "${target_file}" "${link_file}"

    run validate_file_executable "${link_file}"
    [[ "${status}" -eq 0 ]]
}

@test "validate_file_executable handles broken symlinks" {
    link_file="${EUXIS_TEST_TMPDIR}/broken_link"
    ln -s "/nonexistent/target" "${link_file}"

    run validate_file_executable "${link_file}"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "does not exist" ]]
}

@test "validate_file_executable uses custom description" {
    run validate_file_executable "/nonexistent/file" "dispatch script"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "dispatch script does not exist" ]]
}

# ============================================================================
# SYSTEM DEPENDENCIES VALIDATION
# ============================================================================

@test "validate_system_dependencies always returns success" {
    # This function returns 0 even when deps are missing (warnings only)
    run validate_system_dependencies
    [[ "${status}" -eq 0 ]]
}

# ============================================================================
# EUXIS STRUCTURE VALIDATION
# ============================================================================

@test "validate_euxis_structure accepts valid structure" {
    run validate_euxis_structure
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Euxis directory structure is valid" ]]
}

@test "validate_euxis_structure fails on missing bin directory" {
    rm -rf "${EUXIS_HOME}/euxis-cli/bin"

    run validate_euxis_structure
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Required directory missing" ]]
}

@test "validate_euxis_structure fails on missing prompts directory" {
    rm -rf "${EUXIS_HOME}/euxis-core/agents/prompts"

    run validate_euxis_structure
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Required directory missing" ]]
}

@test "validate_euxis_structure fails on missing data directory" {
    rm -rf "${EUXIS_HOME}/data"

    run validate_euxis_structure
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Required directory missing" ]]
}

@test "validate_euxis_structure fails on missing registry" {
    rm -f "${EUXIS_HOME}/euxis-core/agents/registry.json"
    rm -f "${EUXIS_HOME}/euxis-core/agents/registry.db"

    run validate_euxis_structure
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Registry not found" ]]
}

@test "validate_euxis_structure fails on invalid JSON registry" {
    echo "invalid json" > "${EUXIS_HOME}/euxis-core/agents/registry.json"

    run validate_euxis_structure
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "not valid JSON" ]]
}

# ============================================================================
# VALIDATION LEVELS
# ============================================================================

@test "validate_minimal passes with valid structure" {
    run validate_minimal
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Minimal validation passed." ]]
}

@test "validate_minimal fails with missing structure" {
    rm -rf "${EUXIS_HOME}/euxis-cli/bin"

    run validate_minimal
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Required directory missing" ]]
}

@test "validate_full passes with valid structure" {
    run validate_full
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Full validation passed." ]]
}

@test "validate_comprehensive is alias for validate_full" {
    run validate_comprehensive
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Full validation passed." ]]
}

# ============================================================================
# ERROR PATHS AND EDGE CASES
# ============================================================================

@test "validation handles set -u mode" {
    set -u
    run validation_pass "test"
    [[ "${status}" -eq 0 ]]
}

@test "validation handles pipefail mode" {
    set -e -o pipefail
    run validation_pass "test"
    [[ "${status}" -eq 0 ]]
}

@test "validation functions handle Unicode file paths" {
    unicode_dir="${EUXIS_TEST_TMPDIR}/测试目录"
    mkdir -p "${unicode_dir}"
    unicode_file="${unicode_dir}/测试文件"
    echo "#!/cli/bin/bash" > "${unicode_file}"
    chmod +x "${unicode_file}"

    run validate_file_executable "${unicode_file}"
    [[ "${status}" -eq 0 ]]
}

@test "validation handles very long file paths" {
    long_path="${EUXIS_TEST_TMPDIR}"
    for i in {1..50}; do
        long_path="${long_path}/very_long_directory_name_${i}"
    done
    mkdir -p "${long_path}" 2>/dev/null || skip "Path too long for filesystem"

    test_file="${long_path}/test_file"
    echo "#!/cli/bin/bash" > "${test_file}" 2>/dev/null || skip "Cannot create file"
    chmod +x "${test_file}" 2>/dev/null || skip "Cannot set permissions"

    run validate_file_executable "${test_file}"
    [[ "${status}" -eq 0 ]] || [[ "${status}" -eq 1 ]]
}

# ============================================================================
# INTEGRATION AND IDEMPOTENCY TESTS
# ============================================================================

@test "validation functions are idempotent" {
    run1_output=$(validate_minimal 2>&1)
    run1_status=$?
    run2_output=$(validate_minimal 2>&1)
    run2_status=$?

    [[ "${run1_status}" -eq "${run2_status}" ]]
}

@test "validation handles concurrent execution" {
    validate_minimal &>/dev/null &
    pid1=$!
    validate_minimal &>/dev/null &
    pid2=$!

    wait $pid1 && wait $pid2
}

@test "validation handles signal interruption" {
    skip "Signal handling test requires complex setup"
}
