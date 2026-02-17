#!/usr/bin/env bash
#
# Euxis Library Unit Test Runner
# Usage: bash tests/lib/run_tests.sh
#

set -uo pipefail

EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"
TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PASS=0
FAIL=0
TOTAL=0

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

assert_eq() {
    local label="$1" expected="$2" actual="$3"
    ((TOTAL++))
    if [[ "${expected}" == "${actual}" ]]; then
        echo -e "  ${GREEN}PASS${NC}: ${label}"
        ((PASS++))
    else
        echo -e "  ${RED}FAIL${NC}: ${label}"
        echo "    Expected: '${expected}'"
        echo "    Actual:   '${actual}'"
        ((FAIL++))
    fi
}

assert_contains() {
    local label="$1" needle="$2" haystack="$3"
    ((TOTAL++))
    if echo "${haystack}" | grep -q "${needle}"; then
        echo -e "  ${GREEN}PASS${NC}: ${label}"
        ((PASS++))
    else
        echo -e "  ${RED}FAIL${NC}: ${label}"
        echo "    Expected to contain: '${needle}'"
        echo "    In: '${haystack}'"
        ((FAIL++))
    fi
}

assert_file_exists() {
    local label="$1" path="$2"
    ((TOTAL++))
    if [[ -f "${path}" ]]; then
        echo -e "  ${GREEN}PASS${NC}: ${label}"
        ((PASS++))
    else
        echo -e "  ${RED}FAIL${NC}: ${label} — file not found: ${path}"
        ((FAIL++))
    fi
}

assert_dir_exists() {
    local label="$1" path="$2"
    ((TOTAL++))
    if [[ -d "${path}" ]]; then
        echo -e "  ${GREEN}PASS${NC}: ${label}"
        ((PASS++))
    else
        echo -e "  ${RED}FAIL${NC}: ${label} — dir not found: ${path}"
        ((FAIL++))
    fi
}

echo "=== Euxis Library Unit Tests ==="
echo ""

# Run each test file
for test_file in "${TESTS_DIR}"/test_*.sh; do
    [[ -f "${test_file}" ]] || continue
    echo "--- $(basename "${test_file}") ---"
    source "${test_file}"
    echo ""
done

echo "================================="
echo "Results: ${PASS} passed, ${FAIL} failed, ${TOTAL} total"

if [[ ${FAIL} -gt 0 ]]; then
    exit 1
fi
exit 0
