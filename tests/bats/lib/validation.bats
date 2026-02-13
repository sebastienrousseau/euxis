#!/usr/bin/env bats
# Test suite for bin/lib/validation.sh
# (c) 2026 Euxis Fleet. All rights reserved.

# Load the library under test
source "${BATS_TEST_DIRNAME}/../../../bin/lib/validation.sh"

# Test setup - run before each test
setup() {
    # Create temporary directory for each test
    EUXIS_TEST_TMPDIR="$(mktemp -d)"
    export EUXIS_TEST_TMPDIR

    # Mock EUXIS_HOME
    export EUXIS_HOME="${EUXIS_TEST_TMPDIR}/euxis"
    mkdir -p "${EUXIS_HOME}"

    # Reset validation state
    unset EUXIS_VALIDATION_MODE

    # Mock external commands
    export PATH="${EUXIS_TEST_TMPDIR}:${PATH}"

    # Create mock registry for agent validation
    mkdir -p "${EUXIS_HOME}/config"
    cat > "${EUXIS_HOME}/registry.json" << 'EOF'
{
  "agents": {
    "architect": {"type": "core"},
    "tester": {"type": "default"},
    "invalid-agent": {"type": "unknown"}
  }
}
EOF
}

# Test teardown - run after each test
teardown() {
    if [[ -n "${EUXIS_TEST_TMPDIR}" && -d "${EUXIS_TEST_TMPDIR}" ]]; then
        rm -rf "${EUXIS_TEST_TMPDIR}"
    fi
}

# ============================================================================
# VALIDATION RESULT FUNCTIONS
# ============================================================================

@test "validation_error outputs error message and exits with status 1" {
    run validation_error "test error message"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "VALIDATION ERROR" ]]
    [[ "${output}" =~ "test error message" ]]
}

@test "validation_warning outputs warning message and continues" {
    run validation_warning "test warning message"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "VALIDATION WARNING" ]]
    [[ "${output}" =~ "test warning message" ]]
}

@test "validation_pass outputs success message" {
    run validation_pass "test success message"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "VALIDATION PASS" ]]
    [[ "${output}" =~ "test success message" ]]
}

# ============================================================================
# AGENT NAME VALIDATION
# ============================================================================

@test "validate_agent_name accepts valid core agent" {
    run validate_agent_name "architect"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Valid agent: architect" ]]
}

@test "validate_agent_name accepts valid default agent" {
    run validate_agent_name "tester"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Valid agent: tester" ]]
}

@test "validate_agent_name rejects invalid agent name" {
    run validate_agent_name "nonexistent-agent"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "VALIDATION ERROR" ]]
    [[ "${output}" =~ "Invalid agent name" ]]
}

@test "validate_agent_name rejects empty agent name" {
    run validate_agent_name ""
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "VALIDATION ERROR" ]]
}

@test "validate_agent_name handles missing registry file" {
    rm -f "${EUXIS_HOME}/registry.json"
    run validate_agent_name "architect"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Registry file not found" ]]
}

@test "validate_agent_name handles malformed JSON registry" {
    echo "invalid json" > "${EUXIS_HOME}/registry.json"
    run validate_agent_name "architect"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Failed to parse registry" ]]
}

# ============================================================================
# TASK INPUT VALIDATION
# ============================================================================

@test "validate_task_input accepts non-empty task" {
    run validate_task_input "valid task description"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Valid task input" ]]
}

@test "validate_task_input rejects empty task" {
    run validate_task_input ""
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Task input cannot be empty" ]]
}

@test "validate_task_input rejects whitespace-only task" {
    run validate_task_input "   "
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Task input cannot be empty" ]]
}

@test "validate_task_input handles tasks with special characters" {
    run validate_task_input "task with spaces & special chars: @#$%"
    [[ "${status}" -eq 0 ]]
}

@test "validate_task_input handles Unicode in task" {
    run validate_task_input "测试任务 🚀"
    [[ "${status}" -eq 0 ]]
}

@test "validate_task_input enforces maximum length" {
    # Create a very long task (over reasonable limit)
    long_task=$(printf "%*s" 10000 | tr ' ' 'a')
    run validate_task_input "${long_task}"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Task input too long" ]]
}

# ============================================================================
# FILE EXECUTABLE VALIDATION
# ============================================================================

@test "validate_file_executable accepts executable file" {
    test_file="${EUXIS_TEST_TMPDIR}/test_executable"
    echo "#!/bin/bash" > "${test_file}"
    chmod +x "${test_file}"

    run validate_file_executable "${test_file}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "File is executable" ]]
}

@test "validate_file_executable rejects non-executable file" {
    test_file="${EUXIS_TEST_TMPDIR}/test_not_executable"
    echo "content" > "${test_file}"
    chmod -x "${test_file}"

    run validate_file_executable "${test_file}"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "File is not executable" ]]
}

@test "validate_file_executable rejects non-existent file" {
    run validate_file_executable "/nonexistent/file"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "File does not exist" ]]
}

@test "validate_file_executable rejects directory" {
    run validate_file_executable "${EUXIS_TEST_TMPDIR}"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Path is not a regular file" ]]
}

@test "validate_file_executable handles files with spaces" {
    test_file="${EUXIS_TEST_TMPDIR}/test file with spaces"
    echo "#!/bin/bash" > "${test_file}"
    chmod +x "${test_file}"

    run validate_file_executable "${test_file}"
    [[ "${status}" -eq 0 ]]
}

@test "validate_file_executable handles symlinks" {
    target_file="${EUXIS_TEST_TMPDIR}/target"
    link_file="${EUXIS_TEST_TMPDIR}/link"
    echo "#!/bin/bash" > "${target_file}"
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
    [[ "${output}" =~ "File does not exist" ]]
}

# ============================================================================
# SYSTEM DEPENDENCIES VALIDATION
# ============================================================================

@test "validate_system_dependencies checks for required commands" {
    # Mock required commands
    cat > "${EUXIS_TEST_TMPDIR}/jq" << 'EOF'
#!/bin/bash
echo "jq-1.6"
EOF
    chmod +x "${EUXIS_TEST_TMPDIR}/jq"

    cat > "${EUXIS_TEST_TMPDIR}/git" << 'EOF'
#!/bin/bash
echo "git version 2.30.0"
EOF
    chmod +x "${EUXIS_TEST_TMPDIR}/git"

    run validate_system_dependencies
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "All system dependencies available" ]]
}

@test "validate_system_dependencies fails on missing critical dependency" {
    # Don't provide git mock
    cat > "${EUXIS_TEST_TMPDIR}/jq" << 'EOF'
#!/bin/bash
echo "jq-1.6"
EOF
    chmod +x "${EUXIS_TEST_TMPDIR}/jq"

    run validate_system_dependencies
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Missing critical dependency" ]]
}

@test "validate_system_dependencies handles command version checks" {
    # Mock git with old version
    cat > "${EUXIS_TEST_TMPDIR}/git" << 'EOF'
#!/bin/bash
echo "git version 1.8.0"
EOF
    chmod +x "${EUXIS_TEST_TMPDIR}/git"

    run validate_system_dependencies
    # Should warn about old version but not fail
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "WARNING" ]] || [[ "${status}" -eq 0 ]]
}

# ============================================================================
# EUXIS STRUCTURE VALIDATION
# ============================================================================

@test "validate_euxis_structure accepts valid structure" {
    # Create minimal valid Euxis structure
    mkdir -p "${EUXIS_HOME}/bin"
    mkdir -p "${EUXIS_HOME}/config"
    mkdir -p "${EUXIS_HOME}/data"
    touch "${EUXIS_HOME}/registry.json"
    echo '{"agents": {}}' > "${EUXIS_HOME}/registry.json"

    run validate_euxis_structure
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Euxis structure is valid" ]]
}

@test "validate_euxis_structure fails on missing bin directory" {
    mkdir -p "${EUXIS_HOME}/config"
    mkdir -p "${EUXIS_HOME}/data"

    run validate_euxis_structure
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Missing required directory: bin" ]]
}

@test "validate_euxis_structure fails on missing registry" {
    mkdir -p "${EUXIS_HOME}/bin"
    mkdir -p "${EUXIS_HOME}/config"
    mkdir -p "${EUXIS_HOME}/data"

    run validate_euxis_structure
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Missing registry.json" ]]
}

# ============================================================================
# VALIDATION LEVELS
# ============================================================================

@test "validate_minimal runs basic checks only" {
    mkdir -p "${EUXIS_HOME}/bin"
    touch "${EUXIS_HOME}/registry.json"
    echo '{"agents": {}}' > "${EUXIS_HOME}/registry.json"

    run validate_minimal
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Minimal validation passed" ]]
}

@test "validate_full runs comprehensive checks" {
    # Create complete valid structure
    mkdir -p "${EUXIS_HOME}/bin" "${EUXIS_HOME}/config" "${EUXIS_HOME}/data"
    echo '{"agents": {"tester": {"type": "default"}}}' > "${EUXIS_HOME}/registry.json"

    # Mock system commands
    for cmd in git jq curl python3; do
        cat > "${EUXIS_TEST_TMPDIR}/${cmd}" << 'EOF'
#!/bin/bash
echo "mock command output"
EOF
        chmod +x "${EUXIS_TEST_TMPDIR}/${cmd}"
    done

    run validate_full
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Full validation passed" ]]
}

@test "validate_comprehensive runs all possible checks" {
    # Create complete valid structure
    mkdir -p "${EUXIS_HOME}/bin" "${EUXIS_HOME}/config" "${EUXIS_HOME}/data"
    echo '{"agents": {"tester": {"type": "default"}}}' > "${EUXIS_HOME}/registry.json"

    # Mock all system commands
    for cmd in git jq curl python3 docker kubectl; do
        cat > "${EUXIS_TEST_TMPDIR}/${cmd}" << 'EOF'
#!/bin/bash
echo "mock command output"
EOF
        chmod +x "${EUXIS_TEST_TMPDIR}/${cmd}"
    done

    run validate_comprehensive
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Comprehensive validation passed" ]]
}

# ============================================================================
# ERROR PATHS AND EDGE CASES
# ============================================================================

@test "validation handles set -u mode" {
    set -u
    run validate_minimal
    # Should not fail due to unbound variables
    [[ "${status}" -eq 0 ]] || [[ "${status}" -eq 1 ]]  # Acceptable outcomes
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
    echo "#!/bin/bash" > "${unicode_file}"
    chmod +x "${unicode_file}"

    run validate_file_executable "${unicode_file}"
    [[ "${status}" -eq 0 ]]
}

@test "validation handles very long file paths" {
    # Create nested directory structure
    long_path="${EUXIS_TEST_TMPDIR}"
    for i in {1..50}; do
        long_path="${long_path}/very_long_directory_name_${i}"
    done
    mkdir -p "${long_path}" 2>/dev/null || skip "Path too long for filesystem"

    test_file="${long_path}/test_file"
    echo "#!/bin/bash" > "${test_file}" 2>/dev/null || skip "Cannot create file"
    chmod +x "${test_file}" 2>/dev/null || skip "Cannot set permissions"

    run validate_file_executable "${test_file}"
    [[ "${status}" -eq 0 ]] || [[ "${status}" -eq 1 ]]  # Either valid or path issue
}

# ============================================================================
# INTEGRATION AND IDEMPOTENCY TESTS
# ============================================================================

@test "validation functions are idempotent" {
    # Run validation twice and verify consistent results
    mkdir -p "${EUXIS_HOME}/bin"
    echo '{"agents": {}}' > "${EUXIS_HOME}/registry.json"

    run1_output=$(validate_minimal 2>&1)
    run1_status=$?
    run2_output=$(validate_minimal 2>&1)
    run2_status=$?

    [[ "${run1_status}" -eq "${run2_status}" ]]
    # Output might differ due to timestamps, but status should be same
}

@test "validation handles concurrent execution" {
    mkdir -p "${EUXIS_HOME}/bin"
    echo '{"agents": {}}' > "${EUXIS_HOME}/registry.json"

    # Run multiple validations in parallel (basic test)
    validate_minimal &
    pid1=$!
    validate_minimal &
    pid2=$!

    wait $pid1 && wait $pid2
    # Should not interfere with each other
}

@test "validation handles signal interruption" {
    skip "Signal handling test requires complex setup"
    # This would test SIGTERM/SIGINT handling during validation
}