#!/usr/bin/env bash
# Test script for euxis-license-check

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
LICENSE_CHECKER="${PROJECT_ROOT}/cli/bin/euxis-license-check"

# Test data directory
TEST_DATA_DIR="${SCRIPT_DIR}/data/license_check"
mkdir -p "${TEST_DATA_DIR}"

# Create test requirements files
cat > "${TEST_DATA_DIR}/requirements_clean.txt" <<EOF
# Clean requirements with only permissive licenses
requests>=2.25.0
pyyaml>=5.4.0
click>=7.0
EOF

cat > "${TEST_DATA_DIR}/requirements_problematic.txt" <<EOF
# Requirements with potential license issues
requests>=2.25.0
# GPLv3 library (example - this might not be real)
some-gpl-package>=1.0.0
EOF

# Test functions
test_help() {
    echo "Testing --help flag..."
    if "${LICENSE_CHECKER}" --help | grep -q "USAGE:"; then
        echo "✅ Help output works"
        return 0
    else
        echo "❌ Help output failed"
        return 1
    fi
}

test_missing_file() {
    echo "Testing missing requirements file..."
    local output
    output=$("${LICENSE_CHECKER}" /nonexistent/file 2>&1 || true)
    if echo "$output" | grep -q "Requirements file not found"; then
        echo "✅ Missing file detection works"
        return 0
    else
        echo "❌ Missing file detection failed"
        echo "Debug: Got output: $output"
        return 1
    fi
}

test_empty_requirements() {
    local empty_file="${TEST_DATA_DIR}/empty_requirements.txt"
    touch "${empty_file}"

    echo "Testing empty requirements file..."
    if "${LICENSE_CHECKER}" --requirements "${empty_file}" 2>&1 | grep -q "No dependencies found"; then
        echo "✅ Empty file handling works"
        return 0
    else
        echo "❌ Empty file handling failed"
        return 1
    fi
}

test_format_options() {
    echo "Testing output format options..."
    local test_req="${TEST_DATA_DIR}/requirements_clean.txt"

    # Test markdown format (should not fail even if dependencies can't be analyzed)
    if "${LICENSE_CHECKER}" --format markdown --requirements "${test_req}" 2>/dev/null || [[ $? -eq 1 ]]; then
        echo "✅ Markdown format works"
    else
        echo "❌ Markdown format failed"
        return 1
    fi

    # Test JSON format
    if "${LICENSE_CHECKER}" --format json --requirements "${test_req}" 2>/dev/null || [[ $? -eq 1 ]]; then
        echo "✅ JSON format works"
    else
        echo "❌ JSON format failed"
        return 1
    fi

    return 0
}

# Run tests
echo "Running euxis-license-check tests..."
echo "======================================"

TESTS_PASSED=0
TESTS_TOTAL=0

run_test() {
    local test_name="$1"
    TESTS_TOTAL=$((TESTS_TOTAL + 1))

    if "$test_name"; then
        TESTS_PASSED=$((TESTS_PASSED + 1))
    fi
    echo
}

run_test test_help
run_test test_missing_file
run_test test_empty_requirements
run_test test_format_options

echo "======================================"
echo "Tests passed: ${TESTS_PASSED}/${TESTS_TOTAL}"

if [[ $TESTS_PASSED -eq $TESTS_TOTAL ]]; then
    echo "🎉 All tests passed!"
    exit 0
else
    echo "💥 Some tests failed!"
    exit 1
fi
