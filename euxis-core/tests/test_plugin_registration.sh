#!/usr/bin/env bash

# Euxis Plugin Registration Test Suite
# Tests register_agent_plugin, unregister_agent_plugin, and list_plugins functions

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
TEST_TEMP_DIR="${TMPDIR:-/tmp}/euxis_plugin_test_$$"
FAILED_TESTS=0
TOTAL_TESTS=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test logging functions
log() {
    echo -e "${GREEN}[TEST]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
    FAILED_TESTS=$((FAILED_TESTS + 1))
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

# Mock logging functions to capture output
log_info() {
    echo "[INFO] $1" >> "${TEST_TEMP_DIR}/test_output.log"
}

log_error() {
    echo "[ERROR] $1" >> "${TEST_TEMP_DIR}/test_output.log"
    return 1
}

log_warn() {
    echo "[WARN] $1" >> "${TEST_TEMP_DIR}/test_output.log"
}

# Setup test environment
setup() {
    log "Setting up test environment..."

    # Create test temp directory
    mkdir -p "${TEST_TEMP_DIR}"

    # Mock EUXIS_HOME for testing
    export EUXIS_HOME="${TEST_TEMP_DIR}/.euxis"
    export PROMPTS_DIR="${EUXIS_HOME}/prompts"
    export EUXIS_PLUGINS_DIR="${EUXIS_HOME}/euxis-core/config/plugins"

    # Create directory structure
    mkdir -p "${PROMPTS_DIR}/core"
    mkdir -p "${PROMPTS_DIR}/fleet"
    mkdir -p "${EUXIS_PLUGINS_DIR}"

    # Create test log file
    touch "${TEST_TEMP_DIR}/test_output.log"

    # Source the agents library with mocked logging
    source "${PROJECT_ROOT}/core/lib/agents.sh"
}

# Cleanup test environment
cleanup() {
    log "Cleaning up test files..."
    rm -rf "${TEST_TEMP_DIR}"
    unset EUXIS_HOME PROMPTS_DIR EUXIS_PLUGINS_DIR
}

# Test helper: Create valid manifest file
create_valid_manifest() {
    local manifest_file="$1"
    local agent_id="${2:-test-plugin}"
    local prompt_file="${3:-${TEST_TEMP_DIR}/test_prompt.txt}"
    local tier="${4:-standard}"

    # Create the prompt file
    cat > "${prompt_file}" << 'EOF'
---
agent_id: test-plugin
role: "Test plugin for unit testing"
version: "1.0.0"
tags: [test, plugin]
---

# Test Plugin

Test plugin for unit testing the plugin registration system.
EOF

    # Create the manifest
    cat > "${manifest_file}" << EOF
{
    "agent_id": "${agent_id}",
    "role": "Test plugin for unit testing",
    "prompt_file": "${prompt_file}",
    "tier": "${tier}",
    "tags": ["test", "plugin"]
}
EOF
}

# Test helper: Create invalid manifest file
create_invalid_manifest() {
    local manifest_file="$1"
    local type="${2:-missing_field}"

    case "${type}" in
        "invalid_json")
            echo '{"agent_id": "test-plugin", invalid json' > "${manifest_file}"
            ;;
        "missing_agent_id")
            echo '{"role": "Test plugin", "prompt_file": "/tmp/test.txt"}' > "${manifest_file}"
            ;;
        "null_agent_id")
            echo '{"agent_id": null, "role": "Test plugin", "prompt_file": "/tmp/test.txt"}' > "${manifest_file}"
            ;;
        "missing_prompt_file")
            echo '{"agent_id": "test-plugin", "role": "Test plugin"}' > "${manifest_file}"
            ;;
        "nonexistent_prompt_file")
            echo '{"agent_id": "test-plugin", "role": "Test plugin", "prompt_file": "/nonexistent/file.txt"}' > "${manifest_file}"
            ;;
        *)
            echo '{"agent_id": "test-plugin"}' > "${manifest_file}"
            ;;
    esac
}

# Test helper: Check if plugin is registered
is_plugin_registered() {
    local agent_id="$1"
    [[ -f "${PROMPTS_DIR}/fleet/${agent_id}.txt" ]] && [[ -f "${EUXIS_PLUGINS_DIR}/${agent_id}.json" ]]
}

# Test helper: Get test output
get_test_output() {
    cat "${TEST_TEMP_DIR}/test_output.log" 2>/dev/null || echo ""
}

# Test helper: Clear test output
clear_test_output() {
    > "${TEST_TEMP_DIR}/test_output.log"
}

# ============================================================================
# Test Cases: register_agent_plugin
# ============================================================================

test_register_plugin_success() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 1: Register plugin with valid manifest (SUCCESS)"

    local manifest="${TEST_TEMP_DIR}/valid_manifest.json"
    create_valid_manifest "${manifest}" "test-success" "${TEST_TEMP_DIR}/test_prompt.txt"
    clear_test_output

    # Test registration
    if register_agent_plugin "${manifest}"; then
        # Verify symlink created
        if [[ -L "${PROMPTS_DIR}/fleet/test-success.txt" ]]; then
            # Verify metadata copied
            if [[ -f "${EUXIS_PLUGINS_DIR}/test-success.json" ]]; then
                # Verify log output
                if grep -q "\[INFO\] Registered plugin agent: test-success" "${TEST_TEMP_DIR}/test_output.log"; then
                    log "✓ Plugin registered successfully with all artifacts created"
                else
                    error "Log message not found"
                fi
            else
                error "Plugin metadata file not created"
            fi
        else
            error "Plugin symlink not created"
        fi
    else
        error "Plugin registration failed unexpectedly"
    fi
}

test_register_plugin_missing_file() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 2: Register plugin with missing manifest file (FAILURE)"

    clear_test_output

    # Test with nonexistent file
    if ! register_agent_plugin "/nonexistent/manifest.json"; then
        # Verify error message
        if grep -q "\[ERROR\] Plugin manifest not found: /nonexistent/manifest.json" "${TEST_TEMP_DIR}/test_output.log"; then
            log "✓ Correctly failed with missing file error"
        else
            error "Expected error message not found"
        fi
    else
        error "Registration should have failed with missing file"
    fi
}

test_register_plugin_missing_jq() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 3: Register plugin without jq command (FAILURE)"

    local manifest="${TEST_TEMP_DIR}/valid_manifest.json"
    create_valid_manifest "${manifest}"
    clear_test_output

    # Mock missing jq by overriding command
    local original_path="$PATH"
    export PATH="/nonexistent"

    if ! register_agent_plugin "${manifest}"; then
        # Restore PATH first before using grep
        export PATH="${original_path}"

        # Verify error message
        if grep -q "\[ERROR\] jq is required for plugin registration" "${TEST_TEMP_DIR}/test_output.log"; then
            log "✓ Correctly failed with missing jq error"
        else
            error "Expected jq error message not found"
        fi
    else
        # Restore PATH
        export PATH="${original_path}"
        error "Registration should have failed without jq"
    fi
}

test_register_plugin_invalid_json() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 4: Register plugin with invalid JSON manifest (FAILURE)"

    local manifest="${TEST_TEMP_DIR}/invalid_manifest.json"
    create_invalid_manifest "${manifest}" "invalid_json"
    clear_test_output

    # Test should fail due to jq parsing error
    if ! register_agent_plugin "${manifest}" 2>/dev/null; then
        log "✓ Correctly failed with invalid JSON"
    else
        error "Registration should have failed with invalid JSON"
    fi
}

test_register_plugin_missing_agent_id() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 5: Register plugin with missing agent_id (FAILURE)"

    local manifest="${TEST_TEMP_DIR}/no_agent_id.json"
    create_invalid_manifest "${manifest}" "missing_agent_id"
    clear_test_output

    if ! register_agent_plugin "${manifest}"; then
        if grep -q "\[ERROR\] Plugin manifest missing 'agent_id'" "${TEST_TEMP_DIR}/test_output.log"; then
            log "✓ Correctly failed with missing agent_id error"
        else
            error "Expected agent_id error message not found"
        fi
    else
        error "Registration should have failed with missing agent_id"
    fi
}

test_register_plugin_null_agent_id() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 6: Register plugin with null agent_id (FAILURE)"

    local manifest="${TEST_TEMP_DIR}/null_agent_id.json"
    create_invalid_manifest "${manifest}" "null_agent_id"
    clear_test_output

    if ! register_agent_plugin "${manifest}"; then
        if grep -q "\[ERROR\] Plugin manifest missing 'agent_id'" "${TEST_TEMP_DIR}/test_output.log"; then
            log "✓ Correctly failed with null agent_id error"
        else
            error "Expected agent_id error message not found"
        fi
    else
        error "Registration should have failed with null agent_id"
    fi
}

test_register_plugin_missing_prompt_file() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 7: Register plugin with missing prompt_file (FAILURE)"

    local manifest="${TEST_TEMP_DIR}/no_prompt_file.json"
    create_invalid_manifest "${manifest}" "missing_prompt_file"
    clear_test_output

    if ! register_agent_plugin "${manifest}"; then
        if grep -q "\[ERROR\] Plugin manifest 'prompt_file' not found:" "${TEST_TEMP_DIR}/test_output.log"; then
            log "✓ Correctly failed with missing prompt_file error"
        else
            error "Expected prompt_file error message not found"
        fi
    else
        error "Registration should have failed with missing prompt_file"
    fi
}

test_register_plugin_nonexistent_prompt_file() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 8: Register plugin with nonexistent prompt_file (FAILURE)"

    local manifest="${TEST_TEMP_DIR}/bad_prompt_file.json"
    create_invalid_manifest "${manifest}" "nonexistent_prompt_file"
    clear_test_output

    if ! register_agent_plugin "${manifest}"; then
        if grep -q "\[ERROR\] Plugin manifest 'prompt_file' not found: /nonexistent/file.txt" "${TEST_TEMP_DIR}/test_output.log"; then
            log "✓ Correctly failed with nonexistent prompt_file error"
        else
            error "Expected prompt_file error message not found"
        fi
    else
        error "Registration should have failed with nonexistent prompt_file"
    fi
}

test_register_plugin_tier_defaults() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 9: Register plugin with default tier (SUCCESS)"

    local manifest="${TEST_TEMP_DIR}/no_tier_manifest.json"
    local prompt_file="${TEST_TEMP_DIR}/test_prompt_tier.txt"

    # Create manifest without tier field
    cat > "${prompt_file}" << 'EOF'
---
agent_id: test-tier
role: "Test tier defaults"
---
# Test Plugin
EOF

    cat > "${manifest}" << EOF
{
    "agent_id": "test-tier",
    "role": "Test tier defaults",
    "prompt_file": "${prompt_file}"
}
EOF

    clear_test_output

    if register_agent_plugin "${manifest}"; then
        if grep -q "\[INFO\] Registered plugin agent: test-tier (tier: standard)" "${TEST_TEMP_DIR}/test_output.log"; then
            log "✓ Plugin registered with default tier 'standard'"
        else
            error "Expected default tier message not found"
        fi
    else
        error "Plugin registration failed unexpectedly"
    fi
}

# ============================================================================
# Test Cases: unregister_agent_plugin
# ============================================================================

test_unregister_plugin_existing() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 10: Unregister existing plugin (SUCCESS)"

    # First register a plugin
    local manifest="${TEST_TEMP_DIR}/unregister_test.json"
    create_valid_manifest "${manifest}" "unregister-test"
    register_agent_plugin "${manifest}" >/dev/null

    # Verify it's registered
    if ! is_plugin_registered "unregister-test"; then
        error "Plugin not registered for unregister test"
        return
    fi

    clear_test_output

    # Test unregistration
    if unregister_agent_plugin "unregister-test"; then
        # Verify files removed
        if [[ ! -f "${PROMPTS_DIR}/fleet/unregister-test.txt" ]] && [[ ! -f "${EUXIS_PLUGINS_DIR}/unregister-test.json" ]]; then
            # Verify log message
            if grep -q "\[INFO\] Unregistered plugin agent: unregister-test" "${TEST_TEMP_DIR}/test_output.log"; then
                log "✓ Plugin unregistered successfully"
            else
                error "Expected unregister log message not found"
            fi
        else
            error "Plugin files not removed after unregistration"
        fi
    else
        error "Plugin unregistration failed"
    fi
}

test_unregister_plugin_nonexistent() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 11: Unregister nonexistent plugin (SUCCESS - idempotent)"

    clear_test_output

    # Test unregistering non-existent plugin (should be idempotent)
    if unregister_agent_plugin "nonexistent-plugin"; then
        if grep -q "\[INFO\] Unregistered plugin agent: nonexistent-plugin" "${TEST_TEMP_DIR}/test_output.log"; then
            log "✓ Unregister is idempotent for nonexistent plugins"
        else
            error "Expected unregister log message not found"
        fi
    else
        error "Unregister should succeed even for nonexistent plugins"
    fi
}

# ============================================================================
# Test Cases: list_plugins
# ============================================================================

test_list_plugins_empty() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 12: List plugins when none registered (EMPTY)"

    # Ensure plugins directory is empty
    rm -f "${EUXIS_PLUGINS_DIR}"/*.json 2>/dev/null || true

    local output
    output=$(list_plugins)

    if [[ -z "${output}" ]]; then
        log "✓ List plugins returns empty when no plugins registered"
    else
        error "Expected empty output, got: ${output}"
    fi
}

test_list_plugins_single() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 13: List plugins with single plugin (SUCCESS)"

    # Register a plugin
    local manifest="${TEST_TEMP_DIR}/list_test.json"
    create_valid_manifest "${manifest}" "list-test-plugin"
    register_agent_plugin "${manifest}" >/dev/null

    local output
    output=$(list_plugins)

    if echo "${output}" | grep -q "list-test-plugin (plugin)"; then
        log "✓ Single plugin listed correctly"
    else
        error "Expected plugin not found in list output: ${output}"
    fi
}

test_list_plugins_multiple() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 14: List plugins with multiple plugins (SUCCESS)"

    # Register multiple plugins
    local manifest1="${TEST_TEMP_DIR}/multi1.json"
    local manifest2="${TEST_TEMP_DIR}/multi2.json"

    create_valid_manifest "${manifest1}" "multi-plugin-1"
    create_valid_manifest "${manifest2}" "multi-plugin-2"

    register_agent_plugin "${manifest1}" >/dev/null
    register_agent_plugin "${manifest2}" >/dev/null

    local output
    output=$(list_plugins)

    if echo "${output}" | grep -q "multi-plugin-1 (plugin)" && echo "${output}" | grep -q "multi-plugin-2 (plugin)"; then
        log "✓ Multiple plugins listed correctly"
    else
        error "Expected plugins not found in list output: ${output}"
    fi
}

test_list_plugins_no_directory() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 15: List plugins when plugins directory doesn't exist (EMPTY)"

    # Remove plugins directory
    rm -rf "${EUXIS_PLUGINS_DIR}"

    local output
    output=$(list_plugins)

    if [[ -z "${output}" ]]; then
        log "✓ List plugins handles missing directory gracefully"
    else
        error "Expected empty output when directory missing, got: ${output}"
    fi

    # Recreate for other tests
    mkdir -p "${EUXIS_PLUGINS_DIR}"
}

# ============================================================================
# Edge Cases and Integration Tests
# ============================================================================

test_plugin_overwrite_existing() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 16: Register plugin over existing plugin (SUCCESS - overwrite)"

    # Register first plugin
    local manifest1="${TEST_TEMP_DIR}/overwrite1.json"
    create_valid_manifest "${manifest1}" "overwrite-test" "${TEST_TEMP_DIR}/prompt1.txt"
    register_agent_plugin "${manifest1}" >/dev/null

    # Register second plugin with same ID
    local manifest2="${TEST_TEMP_DIR}/overwrite2.json"
    create_valid_manifest "${manifest2}" "overwrite-test" "${TEST_TEMP_DIR}/prompt2.txt"
    clear_test_output

    if register_agent_plugin "${manifest2}"; then
        # Verify the new plugin replaced the old one
        local target_link
        target_link=$(readlink "${PROMPTS_DIR}/fleet/overwrite-test.txt")
        if [[ "${target_link}" == "${TEST_TEMP_DIR}/prompt2.txt" ]]; then
            log "✓ Plugin overwrite successful"
        else
            error "Plugin symlink not updated correctly"
        fi
    else
        error "Plugin overwrite failed"
    fi
}

test_register_unregister_cycle() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 17: Register-unregister-register cycle (SUCCESS)"

    local manifest="${TEST_TEMP_DIR}/cycle_test.json"
    create_valid_manifest "${manifest}" "cycle-test"

    # Register -> Unregister -> Register
    if register_agent_plugin "${manifest}" >/dev/null &&
       unregister_agent_plugin "cycle-test" >/dev/null &&
       register_agent_plugin "${manifest}" >/dev/null; then

        if is_plugin_registered "cycle-test"; then
            log "✓ Register-unregister-register cycle successful"
        else
            error "Plugin not properly re-registered after cycle"
        fi
    else
        error "Register-unregister-register cycle failed"
    fi
}

test_symlink_handling() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 18: Symlink creation and handling (SUCCESS)"

    local manifest="${TEST_TEMP_DIR}/symlink_test.json"
    create_valid_manifest "${manifest}" "symlink-test"
    register_agent_plugin "${manifest}" >/dev/null

    local symlink_path="${PROMPTS_DIR}/fleet/symlink-test.txt"

    # Verify symlink created and points to correct file
    if [[ -L "${symlink_path}" ]]; then
        local target
        target=$(readlink "${symlink_path}")
        if [[ "${target}" == "${TEST_TEMP_DIR}/test_prompt.txt" ]]; then
            # Verify symlink content is readable
            if [[ -r "${symlink_path}" ]] && grep -q "Test plugin for unit testing" "${symlink_path}"; then
                log "✓ Symlink created correctly and is readable"
            else
                error "Symlink not readable or content incorrect"
            fi
        else
            error "Symlink target incorrect: expected ${TEST_TEMP_DIR}/test_prompt.txt, got ${target}"
        fi
    else
        error "Symlink not created or not a symlink"
    fi
}

# ============================================================================
# Test Execution and Reporting
# ============================================================================

# Run all tests
run_tests() {
    log "Starting Euxis Plugin Registration Test Suite"
    log "=============================================="

    setup

    # Success path tests
    test_register_plugin_success
    test_register_plugin_tier_defaults
    test_unregister_plugin_existing
    test_unregister_plugin_nonexistent
    test_list_plugins_empty
    test_list_plugins_single
    test_list_plugins_multiple
    test_list_plugins_no_directory

    # Failure path tests
    test_register_plugin_missing_file
    test_register_plugin_missing_jq
    test_register_plugin_invalid_json
    test_register_plugin_missing_agent_id
    test_register_plugin_null_agent_id
    test_register_plugin_missing_prompt_file
    test_register_plugin_nonexistent_prompt_file

    # Edge cases and integration tests
    test_plugin_overwrite_existing
    test_register_unregister_cycle
    test_symlink_handling

    cleanup
}

# Report results
report_results() {
    log "=============================================="
    log "Test Results Summary:"
    log "Total tests: ${TOTAL_TESTS}"
    log "Failed tests: ${FAILED_TESTS}"
    log "Passed tests: $((TOTAL_TESTS - FAILED_TESTS))"

    if [[ ${FAILED_TESTS} -eq 0 ]]; then
        log "✅ All tests passed!"
        exit 0
    else
        error "❌ ${FAILED_TESTS} test(s) failed"
        exit 1
    fi
}

# Main execution
main() {
    # Check dependencies
    if ! command -v jq &>/dev/null; then
        error "jq is required for these tests"
        exit 1
    fi

    run_tests
    report_results
}

# Execute if run directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi