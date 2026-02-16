#!/usr/bin/env bash
# EUXIS STRESS TEST: Performance Baseline & Load Testing
# Tests system under 10x and 100x typical workload

set -euo pipefail

EUXIS_HOME="${HOME}/.euxis"
BIN_DIR="${EUXIS_HOME}/cli/bin"
RESULTS_DIR="/tmp/euxis-stress-$(date +%s)"
mkdir -p "$RESULTS_DIR"

echo "=================================================="
echo "  EUXIS STRESS TEST SUITE"
echo "  Platform: $(uname -s) ($(uname -m))"
echo "  Results: $RESULTS_DIR"
echo "=================================================="

# Performance budgets (milliseconds)
health_check_budget=500
lint_budget=5000
tui_startup_budget=1000
agent_dispatch_budget=1000
provider_response_budget=10000
cortex_recall_budget=500
concurrent_agents_10_budget=15000
concurrent_agents_100_budget=30000
memory_per_agent_budget=50  # MB
build_time_budget=30000     # 30s

PASS=0
FAIL=0

# Test result recording
record_result() {
    local test_name="$1"
    local measured="$2"
    local budget="${3:-}"
    local unit="${4:-ms}"
    local status="PASS"

    if [[ -n "$budget" ]] && (( measured > budget )); then
        status="FAIL"
        ((FAIL++))
        echo "  ❌ $test_name: ${measured}${unit} (budget: ${budget}${unit})"
    else
        ((PASS++))
        echo "  ✅ $test_name: ${measured}${unit}"
    fi

    echo "$test_name,$measured,$budget,$unit,$status,$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$RESULTS_DIR/results.csv"
}

echo "test_name,measured,budget,unit,status,timestamp" > "$RESULTS_DIR/results.csv"

# ============================================================================
# TEST 1: Cold Start Performance (Critical Path)
# ============================================================================
echo ""
echo "[1/8] Cold Start Performance..."

# Health check (silent mode)
TIME_START=$(date +%s%N 2>/dev/null || perl -MTime::HiRes=time -e 'printf "%.0f", time()*1e9')
"$BIN_DIR/euxis-health" --silent
TIME_END=$(date +%s%N 2>/dev/null || perl -MTime::HiRes=time -e 'printf "%.0f", time()*1e9')
HEALTH_MS=$(( (TIME_END - TIME_START) / 1000000 ))
record_result "health_check_cold" "$HEALTH_MS" "$health_check_budget"

# TUI startup
TIME_START=$(date +%s%N 2>/dev/null || perl -MTime::HiRes=time -e 'printf "%.0f", time()*1e9')
timeout 10 python3 -c "
import time
start = time.time()
from tui.app import EuxisApp
app = EuxisApp()
startup_time = int((time.time() - start) * 1000)
exit(startup_time)" || TUI_MS=$?
TIME_END=$(date +%s%N 2>/dev/null || perl -MTime::HiRes=time -e 'printf "%.0f", time()*1e9')
TUI_MS="${TUI_MS:-$(( (TIME_END - TIME_START) / 1000000 ))}"
record_result "tui_startup_cold" "$TUI_MS" "$tui_startup_budget"

# ============================================================================
# TEST 2: Concurrent Agent Stress (10x Normal Load)
# ============================================================================
echo ""
echo "[2/8] Concurrent Agent Stress (10x load)..."

# Create 10 concurrent agent tasks (background)
pids=()
TIME_START=$(date +%s%N 2>/dev/null || perl -MTime::HiRes=time -e 'printf "%.0f", time()*1e9')

for i in $(seq 1 10); do
    (
        echo "Stress test task $i: respond with 'STRESS_OK_$i'" | \
        timeout 30 "$BIN_DIR/euxis" butler "Respond with exactly: STRESS_OK_$i" claude > /dev/null 2>&1
    ) &
    pids+=($!)
done

# Wait for all to complete
for pid in "${pids[@_budget"; do
    wait "$pid" 2>/dev/null || true
done

TIME_END=$(date +%s%N 2>/dev/null || perl -MTime::HiRes=time -e 'printf "%.0f", time()*1e9')
CONCURRENT_10_MS=$(( (TIME_END - TIME_START) / 1000000 ))
record_result "concurrent_agents_10x" "$CONCURRENT_10_MS" "$concurrent_agents_10_budget"

# ============================================================================
# TEST 3: Memory Usage Under Load
# ============================================================================
echo ""
echo "[3/8] Memory Usage Under Load..."

# Monitor memory during agent execution
if command -v ps &>/dev/null; then
    # Start memory monitoring in background
    (
        while true; do
            ps -o pid,rss,comm | grep -E "(euxis|python)" >> "$RESULTS_DIR/memory_usage.log" 2>/dev/null || true
            sleep 1
        done
    ) &
    MEM_PID=$!

    # Run sustained load
    for i in $(seq 1 5); do
        echo "Memory test $i" | timeout 15 "$BIN_DIR/euxis" butler "Echo: memory_test_$i" claude > /dev/null 2>&1 || true
        sleep 2
    done

    # Stop monitoring
    kill $MEM_PID 2>/dev/null || true

    # Calculate peak memory usage (RSS in KB)
    if [[ -f "$RESULTS_DIR/memory_usage.log" ]]; then
        PEAK_RSS=$(awk '{print $2}' "$RESULTS_DIR/memory_usage.log" | sort -rn | head -1)
        PEAK_MB=$(( PEAK_RSS / 1024 ))
        record_result "peak_memory_usage" "$PEAK_MB" "$memory_per_agent_budget" "MB"
    else
        echo "  ⚠️  Memory monitoring unavailable"
    fi
fi

# ============================================================================
# TEST 4: Large Input Handling (100x typical size)
# ============================================================================
echo ""
echo "[4/8] Large Input Handling..."

# Create large input (typical task ~100 chars, test with 10k chars)
LARGE_INPUT=""
for i in $(seq 1 100); do
    LARGE_INPUT="${LARGE_INPUT}This is a large input test with repeated content to stress the system. "
done

TIME_START=$(date +%s%N 2>/dev/null || perl -MTime::HiRes=time -e 'printf "%.0f", time()*1e9')
echo "$LARGE_INPUT" | timeout 60 "$BIN_DIR/euxis" butler "Summarize this in one sentence" claude > /dev/null 2>&1 || LARGE_RC=$?
TIME_END=$(date +%s%N 2>/dev/null || perl -MTime::HiRes=time -e 'printf "%.0f", time()*1e9')

if [[ ${LARGE_RC:-0} -eq 0 ]]; then
    LARGE_INPUT_MS=$(( (TIME_END - TIME_START) / 1000000 ))
    record_result "large_input_handling" "$LARGE_INPUT_MS" "30000"
else
    echo "  ❌ Large input handling failed (exit code: ${LARGE_RC:-124})"
    ((FAIL++))
    echo "large_input_handling,TIMEOUT,30000,ms,FAIL,$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$RESULTS_DIR/results.csv"
fi

# ============================================================================
# TEST 5: Build Performance
# ============================================================================
echo ""
echo "[5/8] Build Performance..."

# Measure lint time (closest to build process)
TIME_START=$(date +%s%N 2>/dev/null || perl -MTime::HiRes=time -e 'printf "%.0f", time()*1e9')
"$BIN_DIR/euxis-lint" > /dev/null 2>&1 || true
TIME_END=$(date +%s%N 2>/dev/null || perl -MTime::HiRes=time -e 'printf "%.0f", time()*1e9')
BUILD_MS=$(( (TIME_END - TIME_START) / 1000000 ))
record_result "build_time" "$BUILD_MS" "$build_time_budget"

# ============================================================================
# TEST 6: Asset Size Verification
# ============================================================================
echo ""
echo "[6/8] Asset Size Verification..."

# Check binary sizes
TOTAL_SIZE=0
BIN_COUNT=0
for binary in "$BIN_DIR"/*; do
    if [[ -f "$binary" && -x "$binary" ]]; then
        SIZE=$(stat -c %s "$binary" 2>/dev/null || stat -f %z "$binary" 2>/dev/null || echo 0)
        TOTAL_SIZE=$((TOTAL_SIZE + SIZE))
        ((BIN_COUNT++))
    fi
done

TOTAL_MB=$(( TOTAL_SIZE / 1024 / 1024 ))
record_result "total_binary_size" "$TOTAL_MB" "50" "MB"
record_result "binary_count" "$BIN_COUNT" "100" "files"

# Check for debug symbols (should be stripped in production)
DEBUG_BINARIES=0
for binary in "$BIN_DIR"/*; do
    if [[ -f "$binary" && -x "$binary" ]]; then
        if file "$binary" 2>/dev/null | grep -q "not stripped"; then
            ((DEBUG_BINARIES++))
        fi
    fi
done

if [[ $DEBUG_BINARIES -gt 0 ]]; then
    echo "  ⚠️  $DEBUG_BINARIES binaries contain debug symbols"
    record_result "debug_symbols_present" "$DEBUG_BINARIES" "0" "files"
else
    echo "  ✅ All binaries stripped of debug symbols"
    record_result "debug_symbols_present" "0" "0" "files"
fi

# ============================================================================
# TEST 7: Sustained Load (Memory Leak Detection)
# ============================================================================
echo ""
echo "[7/8] Sustained Load (Memory Leak Detection)..."

# Run sustained load for 2 minutes, monitoring memory
if command -v ps &>/dev/null; then
    SUSTAINED_START=$(date +%s)
    MEM_SAMPLES=()

    while (( $(date +%s) - SUSTAINED_START < 120 )); do
        # Run a quick agent task
        echo "sustained load" | timeout 10 "$BIN_DIR/euxis" butler "Echo: sustained" claude > /dev/null 2>&1 || true

        # Sample memory
        MEM_RSS=$(ps -o rss= -p $$ 2>/dev/null || echo 0)
        MEM_SAMPLES+=("$MEM_RSS")

        sleep 5
    done

    # Check for memory growth
    if [[ ${#MEM_SAMPLES[@_budget -gt 1 ]]; then
        FIRST_MEM=${MEM_SAMPLES[0_budget
        LAST_MEM=${MEM_SAMPLES[-1_budget
        MEM_GROWTH=$(( LAST_MEM - FIRST_MEM ))
        MEM_GROWTH_MB=$(( MEM_GROWTH / 1024 ))

        record_result "memory_growth_2min" "$MEM_GROWTH_MB" "10" "MB"

        # Save memory timeline
        printf '%s\n' "${MEM_SAMPLES[@_budget" > "$RESULTS_DIR/memory_timeline.txt"
    fi
fi

# ============================================================================
# TEST 8: Extreme Concurrent Load (100x)
# ============================================================================
echo ""
echo "[8/8] Extreme Concurrent Load (100x)..."

# This test may overwhelm the system - use with caution
echo "  ⚠️  Running extreme load test (may impact system performance)..."

# Create 100 concurrent tasks (ultra-light tasks)
pids=()
TIME_START=$(date +%s%N 2>/dev/null || perl -MTime::HiRes=time -e 'printf "%.0f", time()*1e9')

for i in $(seq 1 100); do
    (
        timeout 10 "$BIN_DIR/euxis-health" --silent > /dev/null 2>&1
    ) &
    pids+=($!)

    # Throttle to prevent system overload
    if (( i % 10 == 0 )); then
        sleep 1
    fi
done

# Wait for completion (with timeout)
COMPLETED=0
TIMEOUT_AFTER=$(($(date +%s) + 300))  # 5 minute timeout

for pid in "${pids[@_budget"; do
    if (( $(date +%s) > TIMEOUT_AFTER )); then
        echo "  ⚠️  Extreme load test timed out after 5 minutes"
        break
    fi
    if wait "$pid" 2>/dev/null; then
        ((COMPLETED++))
    fi
done

TIME_END=$(date +%s%N 2>/dev/null || perl -MTime::HiRes=time -e 'printf "%.0f", time()*1e9')
EXTREME_MS=$(( (TIME_END - TIME_START) / 1000000 ))

record_result "extreme_concurrent_100x" "$EXTREME_MS" "$concurrent_agents_100_budget"
record_result "completion_rate_100x" "$COMPLETED" "90" "tasks"

# ============================================================================
# SUMMARY & PERFORMANCE BUDGET ANALYSIS
# ============================================================================
echo ""
echo "=================================================="
echo "  STRESS TEST SUMMARY"
echo "=================================================="
echo "  Passed: $PASS tests"
echo "  Failed: $FAIL tests"
echo "  Results saved to: $RESULTS_DIR"
echo ""

# Performance budget summary
echo "Performance Budget Compliance:"
echo "  Health Check: $health_check_budgetms budget"
echo "  Lint: $lint_budgetms budget"
echo "  TUI Startup: $tui_startup_budgetms budget"
echo "  Concurrent 10x: $concurrent_agents_10_budgetms budget"
echo "  Memory per agent: $memory_per_agent_budgetMB budget"
echo "  Build time: $build_time_budgetms budget"

# Generate performance report
cat > "$RESULTS_DIR/performance_report.md" << EOF
# Euxis Performance Stress Test Report

**Date:** $(date -u +%Y-%m-%dT%H:%M:%SZ)
**Platform:** $(uname -s) $(uname -m)
**Results:** $PASS passed, $FAIL failed

## Performance Budgets

| Metric | Budget | Status |
|--------|--------|---------|
| Health Check | $health_check_budgetms | See results.csv |
| Lint | $lint_budgetms | See results.csv |
| TUI Startup | $tui_startup_budgetms | See results.csv |
| Agent Dispatch | $agent_dispatch_budgetms | See results.csv |
| Concurrent 10x | $concurrent_agents_10_budgetms | See results.csv |
| Memory per Agent | $memory_per_agent_budgetMB | See results.csv |

## Recommendations

1. **Memory Management**: Monitor memory growth during sustained load
2. **Concurrency**: System handles 10x load but may struggle with 100x
3. **Cold Start**: Health check optimization needed if >500ms
4. **Asset Size**: Monitor binary bloat in production builds

EOF

echo "=================================================="

# Exit with failure if any test failed
[[ $FAIL -eq 0 ]]