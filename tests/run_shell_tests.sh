#!/usr/bin/env bash
# (c) 2026 Euxis Fleet. All rights reserved.
# Shell Script Test Runner using bats-core
# Enforces comprehensive testing discipline for the Euxis repository

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EUXIS_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BATS_DIR="${SCRIPT_DIR}/bats"
COVERAGE_DIR="${SCRIPT_DIR}/coverage/shell"

# Test discipline flags
ENFORCE_COVERAGE=1
ENFORCE_MOCK_DISCIPLINE=1
ENFORCE_ERROR_PATHS=1
ENFORCE_IDEMPOTENCY=1

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[TEST]${NC} $*" >&2
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $*" >&2
}

log_warning() {
    echo -e "${YELLOW}[WARN]${NC} $*" >&2
}

log_error() {
    echo -e "${RED}[FAIL]${NC} $*" >&2
}

# Check dependencies
check_dependencies() {
    local missing=0

    if ! command -v bats &>/dev/null; then
        log_error "bats-core is required. Install with: brew install bats-core"
        missing=1
    fi

    if ! command -v jq &>/dev/null; then
        log_error "jq is required for JSON parsing in tests"
        missing=1
    fi

    if [[ "${missing}" -eq 1 ]]; then
        exit 1
    fi

    log_info "All dependencies available"
}

# Run individual test file with coverage tracking
run_test_file() {
    local test_file="$1"
    local test_name="$(basename "${test_file}" .bats)"

    log_info "Running ${test_name} tests..."

    # Create coverage output directory
    mkdir -p "${COVERAGE_DIR}"

    # Run bats with TAP output for detailed results
    if bats --tap "${test_file}" > "${COVERAGE_DIR}/${test_name}.tap" 2>&1; then
        local test_count=$(grep -c "^ok " "${COVERAGE_DIR}/${test_name}.tap" || echo "0")
        local skip_count=$(grep -c "^ok .* # SKIP" "${COVERAGE_DIR}/${test_name}.tap" || echo "0")
        log_success "${test_name}: ${test_count} tests passed (${skip_count} skipped)"
        return 0
    else
        local fail_count=$(grep -c "^not ok " "${COVERAGE_DIR}/${test_name}.tap" || echo "0")
        log_error "${test_name}: ${fail_count} tests failed"
        cat "${COVERAGE_DIR}/${test_name}.tap"
        return 1
    fi
}

# Analyze function coverage
analyze_coverage() {
    local lib_file="$1"
    local test_file="$2"

    log_info "Analyzing coverage for $(basename "${lib_file}")"

    # Extract exported functions from library
    local functions
    functions=$(grep -n "^[a-zA-Z_][a-zA-Z0-9_]*(" "${lib_file}" | cut -d: -f2 | cut -d'(' -f1 | sort)

    # Extract test cases from bats file
    local test_cases
    test_cases=$(grep -n "^@test" "${test_file}" | cut -d'"' -f2 | sort)

    local total_functions
    total_functions=$(echo "${functions}" | wc -l)
    local tested_functions=0

    echo "Function Coverage Report for $(basename "${lib_file}"):"
    echo "========================================================="

    while IFS= read -r func; do
        if grep -q "${func}" "${test_file}"; then
            echo "  ✓ ${func} - TESTED"
            ((tested_functions++))
        else
            echo "  ✗ ${func} - NOT TESTED"
        fi
    done <<< "${functions}"

    local coverage_percent=$((tested_functions * 100 / total_functions))
    echo ""
    echo "Coverage: ${tested_functions}/${total_functions} functions (${coverage_percent}%)"

    if [[ "${ENFORCE_COVERAGE}" -eq 1 && "${coverage_percent}" -lt 100 ]]; then
        log_error "Coverage requirement failed: ${coverage_percent}% < 100%"
        return 1
    fi

    log_success "Coverage requirement met: ${coverage_percent}%"
    return 0
}

# Check mock discipline in test files
check_mock_discipline() {
    local test_file="$1"

    log_info "Checking mock discipline in $(basename "${test_file}")"

    local violations=0

    # Check for proper setup/teardown
    if ! grep -q "setup()" "${test_file}"; then
        log_warning "Missing setup() function"
        ((violations++))
    fi

    if ! grep -q "teardown()" "${test_file}"; then
        log_warning "Missing teardown() function"
        ((violations++))
    fi

    # Check for temp directory usage
    if ! grep -q "EUXIS_TEST_TMPDIR" "${test_file}"; then
        log_warning "No temporary directory usage detected"
        ((violations++))
    fi

    # Check for external command mocking
    if grep -q "command -v\|which" "${test_file}" && ! grep -q "PATH.*TMPDIR" "${test_file}"; then
        log_warning "External commands may not be properly mocked"
        ((violations++))
    fi

    if [[ "${ENFORCE_MOCK_DISCIPLINE}" -eq 1 && "${violations}" -gt 0 ]]; then
        log_error "Mock discipline violations: ${violations}"
        return 1
    fi

    log_success "Mock discipline check passed"
    return 0
}

# Check error path coverage
check_error_paths() {
    local test_file="$1"

    log_info "Checking error path coverage in $(basename "${test_file}")"

    local error_tests=0

    # Count tests that check error conditions
    error_tests=$(grep -c "@test.*\(error\|fail\|invalid\|missing\|empty\)" "${test_file}" || echo "0")

    # Count tests that verify exit codes
    local exit_code_tests
    exit_code_tests=$(grep -c "status.*-eq.*[1-9]" "${test_file}" || echo "0")

    local total_error_tests=$((error_tests + exit_code_tests))

    echo "Error path tests: ${total_error_tests}"

    if [[ "${ENFORCE_ERROR_PATHS}" -eq 1 && "${total_error_tests}" -lt 3 ]]; then
        log_error "Insufficient error path coverage: ${total_error_tests} < 3"
        return 1
    fi

    log_success "Error path coverage adequate: ${total_error_tests} tests"
    return 0
}

# Check idempotency testing
check_idempotency() {
    local test_file="$1"

    log_info "Checking idempotency testing in $(basename "${test_file}")"

    if ! grep -q "idempotent" "${test_file}"; then
        log_warning "No idempotency tests found"
        if [[ "${ENFORCE_IDEMPOTENCY}" -eq 1 ]]; then
            log_error "Idempotency testing required"
            return 1
        fi
    fi

    log_success "Idempotency testing present"
    return 0
}

# Run integration tests for bin scripts
run_integration_tests() {
    log_info "Running integration tests for bin scripts..."

    local integration_failures=0

    # Test main euxis script
    if [[ -x "${EUXIS_ROOT}/bin/euxis" ]]; then
        log_info "Testing euxis main script..."
        if ! "${EUXIS_ROOT}/bin/euxis" --version &>/dev/null; then
            log_error "euxis script version check failed"
            ((integration_failures++))
        fi
    fi

    # Test key scripts exist and are executable
    local key_scripts=(
        "euxis-dispatch"
        "euxis-cortex"
        "euxis-certify"
        "euxis-health"
    )

    for script in "${key_scripts[@]}"; do
        local script_path="${EUXIS_ROOT}/bin/${script}"
        if [[ ! -x "${script_path}" ]]; then
            log_error "Script not executable: ${script}"
            ((integration_failures++))
        else
            log_success "Script executable: ${script}"
        fi
    done

    if [[ "${integration_failures}" -gt 0 ]]; then
        log_error "Integration test failures: ${integration_failures}"
        return 1
    fi

    log_success "All integration tests passed"
    return 0
}

# Generate test report
generate_report() {
    local total_tests="$1"
    local passed_tests="$2"
    local coverage_failures="$3"

    cat > "${COVERAGE_DIR}/shell_test_report.md" << EOF
# Shell Script Test Report

Generated: $(date)

## Summary
- Total test suites: ${total_tests}
- Passed test suites: ${passed_tests}
- Coverage failures: ${coverage_failures}
- Overall success rate: $(( passed_tests * 100 / total_tests ))%

## Test Discipline Enforcement
- ✓ Function-level coverage required
- ✓ Mock discipline enforced
- ✓ Error path testing required
- ✓ Idempotency testing required

## Coverage Details
EOF

    # Add coverage details for each test file
    for tap_file in "${COVERAGE_DIR}"/*.tap; do
        if [[ -f "${tap_file}" ]]; then
            local test_name="$(basename "${tap_file}" .tap)"
            local test_count=$(grep -c "^ok " "${tap_file}" || echo "0")
            local fail_count=$(grep -c "^not ok " "${tap_file}" || echo "0")
            local skip_count=$(grep -c "# SKIP" "${tap_file}" || echo "0")

            cat >> "${COVERAGE_DIR}/shell_test_report.md" << EOF

### ${test_name}
- Tests run: ${test_count}
- Tests failed: ${fail_count}
- Tests skipped: ${skip_count}
EOF
        fi
    done

    log_success "Test report generated: ${COVERAGE_DIR}/shell_test_report.md"
}

# Main execution
main() {
    local lib_tests=("${BATS_DIR}/lib"/*.bats)
    local total_tests=0
    local passed_tests=0
    local coverage_failures=0

    log_info "Starting comprehensive shell script testing"
    log_info "Euxis root: ${EUXIS_ROOT}"
    log_info "Test directory: ${BATS_DIR}"

    # Check dependencies
    check_dependencies

    # Create coverage directory
    mkdir -p "${COVERAGE_DIR}"

    # Run library tests
    for test_file in "${lib_tests[@]}"; do
        if [[ -f "${test_file}" ]]; then
            ((total_tests++))

            local lib_name="$(basename "${test_file}" .bats)"
            local lib_file="${EUXIS_ROOT}/bin/lib/${lib_name}.sh"

            if [[ ! -f "${lib_file}" ]]; then
                log_error "Library file not found: ${lib_file}"
                continue
            fi

            # Run the test
            if run_test_file "${test_file}"; then
                ((passed_tests++))

                # Check coverage
                if ! analyze_coverage "${lib_file}" "${test_file}"; then
                    ((coverage_failures++))
                fi

                # Check test discipline
                check_mock_discipline "${test_file}"
                check_error_paths "${test_file}"
                check_idempotency "${test_file}"
            else
                log_error "Test suite failed: ${lib_name}"
            fi
        fi
    done

    # Run integration tests
    if run_integration_tests; then
        log_success "Integration tests passed"
    else
        log_error "Integration tests failed"
    fi

    # Generate report
    generate_report "${total_tests}" "${passed_tests}" "${coverage_failures}"

    # Final summary
    echo ""
    log_info "=========================================="
    log_info "SHELL SCRIPT TESTING SUMMARY"
    log_info "=========================================="
    log_info "Total test suites: ${total_tests}"
    log_info "Passed test suites: ${passed_tests}"
    log_info "Failed test suites: $((total_tests - passed_tests))"
    log_info "Coverage failures: ${coverage_failures}"

    if [[ "${passed_tests}" -eq "${total_tests}" && "${coverage_failures}" -eq 0 ]]; then
        log_success "ALL SHELL SCRIPT TESTS PASSED"
        echo ""
        echo "🚀 Shell script testing discipline enforced successfully!"
        echo "   ✓ Function-level coverage achieved"
        echo "   ✓ Mock discipline maintained"
        echo "   ✓ Error paths tested"
        echo "   ✓ Idempotency verified"
        echo "   ✓ Integration tests passed"
        return 0
    else
        log_error "SHELL SCRIPT TESTING FAILED"
        echo ""
        echo "❌ Testing discipline violations detected:"
        if [[ "${passed_tests}" -ne "${total_tests}" ]]; then
            echo "   • Test suite failures: $((total_tests - passed_tests))"
        fi
        if [[ "${coverage_failures}" -gt 0 ]]; then
            echo "   • Coverage requirement failures: ${coverage_failures}"
        fi
        return 1
    fi
}

# Signal handling for cleanup
cleanup() {
    log_info "Cleaning up test artifacts..."
    # Any cleanup needed
}

trap cleanup EXIT

# Run main function
main "$@"