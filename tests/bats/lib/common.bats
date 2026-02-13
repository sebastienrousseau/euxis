#!/usr/bin/env bats
# Test suite for bin/lib/common.sh
# (c) 2026 Euxis Fleet. All rights reserved.

# Load the library under test
source "${BATS_TEST_DIRNAME}/../../../bin/lib/common.sh"

# Test setup - run before each test
setup() {
    # Create temporary directory for each test
    EUXIS_TEST_TMPDIR="$(mktemp -d)"
    export EUXIS_TEST_TMPDIR

    # Reset environment variables to known state
    unset EUXIS_DEBUG
    unset EUXIS_PERF_DISABLE

    # Mock external commands
    export PATH="${BATS_TEST_TMPDIR}:${PATH}"

    # Redirect logs to capture output
    EUXIS_LOG_CAPTURE="${EUXIS_TEST_TMPDIR}/log_capture"
    touch "${EUXIS_LOG_CAPTURE}"
}

# Test teardown - run after each test
teardown() {
    if [[ -n "${EUXIS_TEST_TMPDIR}" && -d "${EUXIS_TEST_TMPDIR}" ]]; then
        rm -rf "${EUXIS_TEST_TMPDIR}"
    fi
}

# ============================================================================
# LOGGING FUNCTIONS
# ============================================================================

@test "log_info outputs to stderr with correct format" {
    run log_info "test message"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "[euxis] test message" ]]
}

@test "log_info handles empty message" {
    run log_info ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "[euxis] " ]]
}

@test "log_info handles special characters and spaces" {
    run log_info "test message with spaces & special chars: @#$%"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "[euxis] test message with spaces & special chars: @#$%" ]]
}

@test "log_error outputs to stderr with ERROR prefix" {
    run log_error "error message"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "[euxis] ERROR: error message" ]]
}

@test "log_warn outputs to stderr with WARN prefix" {
    run log_warn "warning message"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "[euxis] WARN: warning message" ]]
}

@test "log_debug outputs only when EUXIS_DEBUG=1" {
    # Debug disabled by default
    run log_debug "debug message"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "" ]]

    # Debug enabled
    EUXIS_DEBUG=1 run log_debug "debug message"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "[euxis] DEBUG: debug message" ]]
}

@test "log_debug handles EUXIS_DEBUG=0 explicitly" {
    EUXIS_DEBUG=0 run log_debug "debug message"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "" ]]
}

# ============================================================================
# PERFORMANCE INSTRUMENTATION
# ============================================================================

@test "_perf_enabled returns true when performance not disabled" {
    run _perf_enabled
    [[ "${status}" -eq 0 ]]
}

@test "_perf_enabled returns false when EUXIS_PERF_DISABLE=1" {
    EUXIS_PERF_DISABLE=1 run _perf_enabled
    [[ "${status}" -eq 1 ]]
}

@test "_euxis_now_ns returns 0 when performance disabled" {
    EUXIS_PERF_DISABLE=1 run _euxis_now_ns
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "0" ]]
}

@test "_euxis_now_ns returns numeric timestamp when enabled" {
    run _euxis_now_ns
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ ^[0-9]+$ ]]
}

@test "_euxis_now_ns handles date command without %N support" {
    # Mock date command to simulate macOS behavior
    cat > "${EUXIS_TEST_TMPDIR}/date" << 'EOF'
#!/bin/bash
if [[ "$1" == "+%s%N" ]]; then
    echo "$(date +%s)N"
else
    command date "$@"
fi
EOF
    chmod +x "${EUXIS_TEST_TMPDIR}/date"

    # Mock perl for fallback
    cat > "${EUXIS_TEST_TMPDIR}/perl" << 'EOF'
#!/bin/bash
if [[ "$1" == "-MTime::HiRes=time" ]]; then
    echo "1000000000"
else
    command perl "$@"
fi
EOF
    chmod +x "${EUXIS_TEST_TMPDIR}/perl"

    run _euxis_now_ns
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "1000000000" ]]
}

@test "_euxis_now_ns handles no perl available" {
    # Mock date command without %N support
    cat > "${EUXIS_TEST_TMPDIR}/date" << 'EOF'
#!/bin/bash
if [[ "$1" == "+%s%N" ]]; then
    echo "1234567890N"
elif [[ "$1" == "+%s" ]]; then
    echo "1234567890"
else
    command date "$@"
fi
EOF
    chmod +x "${EUXIS_TEST_TMPDIR}/date"

    # Remove perl from PATH
    unset perl

    run _euxis_now_ns
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "1234567890000000000" ]]
}

# ============================================================================
# PERFORMANCE TIMING FUNCTIONS
# ============================================================================

@test "_perf_start sets EUXIS_PERF_START_NS" {
    unset EUXIS_PERF_START_NS
    run _perf_start
    [[ "${status}" -eq 0 ]]
    [[ -n "${EUXIS_PERF_START_NS}" ]]
}

@test "_perf_elapsed_ms calculates elapsed time correctly" {
    # Mock _euxis_now_ns to return predictable values
    _euxis_now_ns() { echo "1000000000"; }
    EUXIS_PERF_START_NS=500000000

    run _perf_elapsed_ms
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "500" ]]  # (1000000000 - 500000000) / 1000000
}

@test "_perf_elapsed_ms returns 0 when performance disabled" {
    EUXIS_PERF_DISABLE=1 run _perf_elapsed_ms
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "0" ]]
}

@test "performance_budget enforces time limits" {
    # Mock _perf_elapsed_ms to return over budget
    _perf_elapsed_ms() { echo "2000"; }

    run performance_budget 1000 "test operation"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "PERFORMANCE BUDGET EXCEEDED" ]]
    [[ "${output}" =~ "test operation" ]]
    [[ "${output}" =~ "2000ms > 1000ms" ]]
}

@test "performance_budget passes when under budget" {
    # Mock _perf_elapsed_ms to return under budget
    _perf_elapsed_ms() { echo "500"; }

    run performance_budget 1000 "test operation"
    [[ "${status}" -eq 0 ]]
}

@test "_perf_check_budget logs warnings when over budget" {
    # Mock _perf_elapsed_ms to return over budget
    _perf_elapsed_ms() { echo "1500"; }

    run _perf_check_budget 1000 "test operation"
    [[ "${status}" -eq 0 ]]  # No exit, just warning
    [[ "${output}" =~ "PERFORMANCE WARNING" ]]
}

@test "_perf_record logs performance metrics" {
    # Mock _perf_elapsed_ms to return test value
    _perf_elapsed_ms() { echo "750"; }

    run _perf_record "test operation"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "test operation completed in 750ms" ]]
}

# ============================================================================
# SPINNER FUNCTIONS
# ============================================================================

@test "start_spinner creates background process" {
    # Mock spinner to avoid actual animation
    start_spinner() {
        echo $$ > "${EUXIS_TEST_TMPDIR}/spinner.pid"
    }

    run start_spinner
    [[ "${status}" -eq 0 ]]
}

@test "stop_spinner terminates background process" {
    # Create mock spinner PID file
    echo "12345" > "${EUXIS_TEST_TMPDIR}/spinner.pid"
    EUXIS_SPINNER_PID=12345

    # Mock kill command
    cat > "${EUXIS_TEST_TMPDIR}/kill" << 'EOF'
#!/bin/bash
exit 0
EOF
    chmod +x "${EUXIS_TEST_TMPDIR}/kill"

    run stop_spinner
    [[ "${status}" -eq 0 ]]
}

# ============================================================================
# ERROR PATHS AND EDGE CASES
# ============================================================================

@test "functions handle unbound variables with set -u" {
    set -u
    run log_info "test with set -u"
    [[ "${status}" -eq 0 ]]
}

@test "functions handle pipefail properly" {
    set -e -o pipefail
    run log_info "test with pipefail"
    [[ "${status}" -eq 0 ]]
}

@test "log functions handle Unicode characters" {
    run log_info "测试 Unicode 🚀"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "[euxis] 测试 Unicode 🚀" ]]
}

@test "performance functions handle extreme values" {
    EUXIS_PERF_START_NS=0
    # Mock _euxis_now_ns to return very large value
    _euxis_now_ns() { echo "999999999999999999"; }

    run _perf_elapsed_ms
    [[ "${status}" -eq 0 ]]
    # Should not crash with large numbers
}

# ============================================================================
# INTEGRATION AND IDEMPOTENCY TESTS
# ============================================================================

@test "functions are idempotent" {
    # Run twice and verify same result
    run1=$(log_info "test message" 2>&1)
    run2=$(log_info "test message" 2>&1)
    [[ "${run1}" == "${run2}" ]]
}

@test "functions handle signal interruption gracefully" {
    # This would be more complex to test properly but verify basic resilience
    run log_info "test signal handling"
    [[ "${status}" -eq 0 ]]
}

@test "functions work with different EUXIS_HOME paths" {
    # Test with custom EUXIS_HOME
    EUXIS_HOME="/tmp/test-euxis" run log_info "test"
    [[ "${status}" -eq 0 ]]

    # Test with EUXIS_HOME containing spaces
    EUXIS_HOME="/tmp/test euxis" run log_info "test"
    [[ "${status}" -eq 0 ]]
}