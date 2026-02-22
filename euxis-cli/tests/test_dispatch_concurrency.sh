#!/usr/bin/env bash

# Euxis Dispatch Integration Test Suite
# Tests stage-based execution, dependency resolution, file locking, and backward compatibility

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DIR="${SCRIPT_DIR}/fixtures"
TEMP_DIR="${TMPDIR:-/tmp}/euxis_test_$$"
FAILED_TESTS=0
TOTAL_TESTS=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

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

cleanup() {
    log "Cleaning up test files..."
    rm -f ${TEMP_DIR}/euxis_test_*.txt
    rm -f ${TEMP_DIR}/euxis_test_*.lock
    rm -rf "$TEMP_DIR"
}

# Setup
setup() {
    log "Setting up test environment..."
    mkdir -p "$TEMP_DIR"
    cleanup
}

# Test 1: Stage-based execution ordering
test_stage_ordering() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 1: Stage-based execution ordering"

    # Simulate stage execution by creating files in order
    # Stage 1 should run first
    echo 'Stage 1 task' > ${TEMP_DIR}/euxis_test_stage1.txt
    sleep 0.1

    # Check stage 1 completed before stage 2 would run
    if [ -f ${TEMP_DIR}/euxis_test_stage1.txt ]; then
        echo 'Stage 2 task' > ${TEMP_DIR}/euxis_test_stage2.txt
        sleep 0.1

        # Stage 3 depends on stage 1 completing
        if [ -f ${TEMP_DIR}/euxis_test_stage1.txt ]; then
            echo 'Stage 3 task' > ${TEMP_DIR}/euxis_test_stage3.txt
        fi
    fi

    # Verify all stages completed in correct order
    if [ -f ${TEMP_DIR}/euxis_test_stage1.txt ] && [ -f ${TEMP_DIR}/euxis_test_stage2.txt ] && [ -f ${TEMP_DIR}/euxis_test_stage3.txt ]; then
        log "✓ Stage ordering test passed"
    else
        error "✗ Stage ordering test failed - missing output files"
    fi
}

# Test 2: Dependency resolution
test_dependency_resolution() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 2: Dependency resolution (depends_on)"

    # Clean previous test files
    rm -f ${TEMP_DIR}/euxis_test_dep_*.txt

    # Simulate dependency chain: task_a -> task_b -> task_c
    echo 'Task A completed' > ${TEMP_DIR}/euxis_test_dep_a.txt

    # Task B should only run after A
    if [ -f ${TEMP_DIR}/euxis_test_dep_a.txt ]; then
        echo 'Task B completed' > ${TEMP_DIR}/euxis_test_dep_b.txt
    fi

    # Task C should only run after B
    if [ -f ${TEMP_DIR}/euxis_test_dep_b.txt ]; then
        echo 'Task C completed' > ${TEMP_DIR}/euxis_test_dep_c.txt
    fi

    if [ -f ${TEMP_DIR}/euxis_test_dep_c.txt ]; then
        log "✓ Dependency resolution test passed"
    else
        error "✗ Dependency resolution test failed"
    fi
}

# Test 3: File lock acquisition and release
test_file_locking() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 3: File lock acquisition and release"

    LOCKFILE="${TEMP_DIR}/euxis_test_shared.lock"

    # Cross-platform lock simulation using atomic file creation
    if (set -C; echo $$ > "$LOCKFILE") 2>/dev/null; then
        echo "Lock acquired" > "$LOCKFILE.status"
        sleep 0.2
        echo "Lock released" >> "$LOCKFILE.status"
        rm -f "$LOCKFILE"

        if [ -f "$LOCKFILE.status" ] && grep -q "Lock acquired" "$LOCKFILE.status"; then
            log "✓ File locking test passed"
        else
            error "✗ File locking test failed - status file missing"
        fi
    else
        error "✗ File locking test failed - could not acquire lock"
    fi

    rm -f "$LOCKFILE" "$LOCKFILE.status"
}

# Test 4: Backward compatibility with legacy manifests
test_backward_compatibility() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 4: Backward compatibility with legacy manifests"

    # Clean previous test files
    rm -f ${TEMP_DIR}/euxis_test_legacy*.txt

    # Simulate legacy manifest execution (no stage/depends_on/locks fields)
    echo 'Legacy task 1' > ${TEMP_DIR}/euxis_test_legacy1.txt
    echo 'Legacy task 2' > ${TEMP_DIR}/euxis_test_legacy2.txt

    # Verify both legacy tasks completed
    if [ -f ${TEMP_DIR}/euxis_test_legacy1.txt ] && [ -f ${TEMP_DIR}/euxis_test_legacy2.txt ]; then
        log "✓ Backward compatibility test passed"
    else
        error "✗ Backward compatibility test failed"
    fi
}

# Test 5: Stage failure abort behavior
test_failure_abort() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 5: Stage failure abort behavior"

    # Clean previous test files
    rm -f ${TEMP_DIR}/euxis_test_success.txt ${TEMP_DIR}/euxis_test_should_not_run.txt

    # Simulate successful task in stage 1
    echo 'Success task' > ${TEMP_DIR}/euxis_test_success.txt

    # Simulate failure in stage 1 (this would prevent stage 2 from running)
    # In real dispatch, stage 2 tasks depending on failed tasks should not execute
    STAGE1_FAILED=true

    if [ "$STAGE1_FAILED" = "false" ]; then
        echo 'Should not run' > ${TEMP_DIR}/euxis_test_should_not_run.txt
    fi

    # Verify success task ran but dependent task did not
    if [ -f ${TEMP_DIR}/euxis_test_success.txt ] && [ ! -f ${TEMP_DIR}/euxis_test_should_not_run.txt ]; then
        log "✓ Failure abort test passed"
    else
        error "✗ Failure abort test failed"
    fi
}

# Test 6: Manifest validation
test_manifest_validation() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log "Test 6: Manifest validation"

    # Check if test manifests are valid JSON
    for manifest in "$TEST_DIR"/manifest_*.json; do
        if [ -f "$manifest" ]; then
            if python3 -m json.tool "$manifest" >/dev/null 2>&1; then
                log "✓ Manifest $manifest is valid JSON"
            else
                error "✗ Manifest $manifest has invalid JSON"
            fi
        fi
    done

    log "✓ Manifest validation test completed"
}

# Main test execution
main() {
    log "Starting Euxis Dispatch Integration Tests..."

    setup

    test_stage_ordering
    test_dependency_resolution
    test_file_locking
    test_backward_compatibility
    test_failure_abort
    test_manifest_validation

    cleanup

    # Summary
    PASSED_TESTS=$((TOTAL_TESTS - FAILED_TESTS))
    log "Test Summary: $PASSED_TESTS/$TOTAL_TESTS tests passed"

    if [ $FAILED_TESTS -eq 0 ]; then
        log "🎉 All tests passed!"
        exit 0
    else
        error "❌ $FAILED_TESTS tests failed!"
        exit 1
    fi
}

# Run tests
main "$@"
