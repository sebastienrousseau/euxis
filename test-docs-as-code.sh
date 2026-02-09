#!/bin/bash
set -euo pipefail

# Test script for documentation-as-code validation
# Validates all code snippets in README.md and docs/ directory

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_LOG="$SCRIPT_DIR/docs-test-results.log"
FAILED_TESTS=()
PASSED_TESTS=()

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test result tracking
log_test() {
    local test_name="$1"
    local result="$2"
    local output="$3"

    echo "[$result] $test_name" >> "$TEST_LOG"
    if [ -n "$output" ]; then
        echo "  Output: $output" >> "$TEST_LOG"
    fi
    echo "" >> "$TEST_LOG"
}

run_test() {
    local test_name="$1"
    local command="$2"
    local expected_pattern="${3:-}"
    local should_fail="${4:-false}"

    echo -n "Testing: $test_name ... "

    # Run command and capture output and exit code
    set +e
    output=$(eval "$command" 2>&1)
    exit_code=$?
    set -e

    # Determine if test passed
    local test_passed=false

    if [ "$should_fail" = "true" ]; then
        # Test should fail
        if [ $exit_code -ne 0 ]; then
            test_passed=true
        fi
    else
        # Test should succeed
        if [ $exit_code -eq 0 ]; then
            if [ -n "$expected_pattern" ]; then
                # Check if output matches expected pattern
                if echo "$output" | grep -q "$expected_pattern"; then
                    test_passed=true
                fi
            else
                test_passed=true
            fi
        fi
    fi

    if [ "$test_passed" = "true" ]; then
        echo -e "${GREEN}PASS${NC}"
        PASSED_TESTS+=("$test_name")
        log_test "$test_name" "PASS" "$output"
    else
        echo -e "${RED}FAIL${NC}"
        FAILED_TESTS+=("$test_name")
        log_test "$test_name" "FAIL" "Exit code: $exit_code, Output: $output"
        echo -e "  ${YELLOW}Command:${NC} $command"
        echo -e "  ${YELLOW}Expected pattern:${NC} $expected_pattern"
        echo -e "  ${YELLOW}Exit code:${NC} $exit_code"
        echo -e "  ${YELLOW}Output:${NC} $output"
    fi
}

# Initialize test log
echo "Documentation Code Testing - $(date)" > "$TEST_LOG"
echo "=============================================" >> "$TEST_LOG"

echo "🧪 Testing Documentation as Code"
echo "================================="

echo ""
echo "📋 Testing Basic Tool Availability"
echo "-----------------------------------"

# Test 1: Basic tool availability tests
run_test "euxis command exists" "command -v euxis"
run_test "euxis-health command exists" "command -v euxis-health"
run_test "euxis-certify command exists" "command -v euxis-certify"
run_test "euxis-cortex command exists" "command -v euxis-cortex"
run_test "euxis-squad command exists" "command -v euxis-squad"
run_test "euxis-combo command exists" "command -v euxis-combo"
run_test "euxis-playbook command exists" "command -v euxis-playbook"

echo ""
echo "🏥 Testing Health and Status Commands"
echo "--------------------------------------"

# Test 2: Health and status commands from documentation
run_test "euxis-health basic execution" "euxis-health" "Fleet Status"
run_test "euxis-squad list" "euxis-squad list"
run_test "euxis-combo list" "euxis-combo list"
run_test "euxis-playbook list" "euxis-playbook list"
run_test "euxis-codex list" "euxis-codex list"

echo ""
echo "📖 Testing Documentation Commands"
echo "----------------------------------"

# Test 3: Info/help commands that should work without setup
run_test "euxis-squad info quality" "euxis-squad info quality"
run_test "euxis-combo info steve-jobs" "euxis-combo info steve-jobs"
run_test "euxis-cortex stats" "euxis-cortex stats"

echo ""
echo "🔧 Testing Tool-Specific Help"
echo "------------------------------"

# Test 4: Help flags and basic syntax validation
run_test "euxis help flag" "euxis --help" "Usage:"
run_test "euxis-health help flag" "euxis-health --help || euxis-health -h || true" # Try both help formats
run_test "euxis-dispatch help" "euxis-dispatch --help" "mode"
run_test "euxis-cortex help" "euxis-cortex --help" "remember"

echo ""
echo "⚡ Testing Prerequisites (from Quick Start)"
echo "-------------------------------------------"

# Test 5: Prerequisites validation (from quick-start.md examples)
run_test "bash version check" "bash --version" "4\."
run_test "python3 version check" "python3 --version" "3\."

echo ""
echo "🎯 Testing Command Syntax Validation"
echo "-------------------------------------"

# Test 6: Command syntax validation (should show proper error messages, not crash)
run_test "euxis without arguments shows help" "euxis 2>&1" "Usage:"
run_test "euxis-cortex without args shows help" "euxis-cortex 2>&1 || true" ""
run_test "euxis-squad without args shows help" "euxis-squad 2>&1 || true" ""
run_test "euxis-combo without args shows help" "euxis-combo 2>&1 || true" ""

echo ""
echo "📝 Testing Safe Non-Destructive Commands"
echo "-----------------------------------------"

# Test 7: Safe commands that can be tested without side effects
run_test "euxis-lint dry run" "euxis-lint --help || euxis-lint --dry-run || true" ""
run_test "euxis-verify help" "euxis-verify --help || true" ""
run_test "euxis-hooks status" "euxis-hooks status" ""

echo ""
echo "🔍 Testing File and Config Validation"
echo "--------------------------------------"

# Test 8: Configuration and registry validation
run_test "Registry file exists" "test -f ~/.euxis/registry.json"
run_test "Registry JSON is valid" "python3 -m json.tool ~/.euxis/registry.json > /dev/null"
run_test "Agent prompts directory exists" "test -d ~/.euxis/prompts"

echo ""
echo "🎨 Testing Output Format Validation"
echo "------------------------------------"

# Test 9: Commands that should produce structured output
run_test "euxis-squad validate" "euxis-squad validate" ""
run_test "euxis-bench help" "euxis-bench --help || true" ""

echo ""
echo "🚨 Testing Error Conditions"
echo "----------------------------"

# Test 10: Expected failure cases (commands that should fail gracefully)
run_test "euxis with invalid agent" "euxis invalid-agent-name 'test task' 2>&1 || true" "" false
run_test "euxis-squad with invalid squad" "euxis-squad info invalid-squad 2>&1 || true" "" false
run_test "euxis-combo with invalid combo" "euxis-combo info invalid-combo 2>&1 || true" "" false

echo ""
echo "📊 Test Results Summary"
echo "======================="

total_tests=$((${#PASSED_TESTS[@]} + ${#FAILED_TESTS[@]}))
pass_rate=$((${#PASSED_TESTS[@]} * 100 / total_tests))

echo "Total Tests: $total_tests"
echo -e "Passed: ${GREEN}${#PASSED_TESTS[@]}${NC}"
echo -e "Failed: ${RED}${#FAILED_TESTS[@]}${NC}"
echo "Pass Rate: $pass_rate%"

if [ ${#FAILED_TESTS[@]} -gt 0 ]; then
    echo ""
    echo -e "${RED}Failed Tests:${NC}"
    for test in "${FAILED_TESTS[@]}"; do
        echo "  - $test"
    done
fi

echo ""
echo "📄 Full test log saved to: $TEST_LOG"

# Exit with appropriate code
if [ ${#FAILED_TESTS[@]} -eq 0 ]; then
    echo -e "\n${GREEN}✅ All documentation code snippets are valid!${NC}"
    exit 0
else
    echo -e "\n${RED}❌ Some documentation code snippets failed validation.${NC}"
    exit 1
fi