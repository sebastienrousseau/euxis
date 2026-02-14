#!/usr/bin/env bash
# EUXIS PERFORMANCE BASELINE: Establish performance budgets and stress test critical paths
set -euo pipefail

EUXIS_HOME="${HOME}/.euxis"
BIN_DIR="${EUXIS_HOME}/bin"
RESULTS_DIR="/tmp/euxis-perf-$(date +%s)"
mkdir -p "$RESULTS_DIR"

echo "=================================================="
echo "  EUXIS PERFORMANCE BASELINE"
echo "  Platform: $(uname -s) ($(uname -m))"
echo "  Results: $RESULTS_DIR"
echo "=================================================="

# Performance measurement utilities
perf_timer_start() {
    if command -v perl &>/dev/null; then
        perl -MTime::HiRes=time -e 'printf "%.0f", time()*1e9'
    else
        date +%s000000000
    fi
}

perf_timer_elapsed_ms() {
    local start="$1"
    local end
    end=$(perf_timer_start)
    echo $(( (end - start) / 1000000 ))
}

# Test result tracking
RESULTS_CSV="$RESULTS_DIR/baseline_results.csv"
echo "metric,value_ms,p50_budget,p95_budget,p99_budget,status,timestamp" > "$RESULTS_CSV"

PASS=0
FAIL=0

record_metric() {
    local name="$1"
    local value="$2"
    local p50_budget="${3:-}"
    local p95_budget="${4:-}"
    local p99_budget="${5:-}"
    local status="PASS"

    if [[ -n "$p99_budget" ]] && (( value > p99_budget )); then
        status="FAIL_P99"
        ((FAIL++))
        echo "  ❌ $name: ${value}ms (exceeds p99 budget: ${p99_budget}ms)"
    elif [[ -n "$p95_budget" ]] && (( value > p95_budget )); then
        status="WARN_P95"
        echo "  ⚠️  $name: ${value}ms (exceeds p95 budget: ${p95_budget}ms)"
    elif [[ -n "$p50_budget" ]] && (( value > p50_budget )); then
        status="WARN_P50"
        echo "  ⚠️  $name: ${value}ms (exceeds p50 budget: ${p50_budget}ms)"
    else
        ((PASS++))
        echo "  ✅ $name: ${value}ms"
    fi

    echo "$name,$value,$p50_budget,$p95_budget,$p99_budget,$status,$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$RESULTS_CSV"
}

# ============================================================================
# BASELINE 1: Critical Path Latencies
# ============================================================================
echo ""
echo "[1/6] Critical Path Latencies..."

# Health check (the problematic 15s latency)
echo "  Testing health check..."
start_time=$(perf_timer_start)
"$BIN_DIR/euxis-health" --silent >/dev/null 2>&1 || true
elapsed=$(perf_timer_elapsed_ms "$start_time")
record_metric "health_check" "$elapsed" 500 2000 5000

# TUI cold start
echo "  Testing TUI startup..."
start_time=$(perf_timer_start)
timeout 15 python3 -c "
import sys, time
start = time.time()
try:
    from tui.app import EuxisApp
    app = EuxisApp()
    startup_ms = int((time.time() - start) * 1000)
    print(startup_ms)
except Exception as e:
    print(5000)  # Timeout value
    sys.exit(1)
" 2>/dev/null | { read tui_ms; echo $tui_ms; } || tui_ms=5000
record_metric "tui_startup" "$tui_ms" 800 1500 3000

# Shell library loading
echo "  Testing shell library loading..."
start_time=$(perf_timer_start)
bash -c "
source '$EUXIS_HOME/bin/lib/common.sh'
source '$EUXIS_HOME/bin/lib/providers.sh'
source '$EUXIS_HOME/bin/lib/agents.sh'
source '$EUXIS_HOME/bin/lib/memory.sh'
source '$EUXIS_HOME/bin/lib/session.sh'
" 2>/dev/null || true
elapsed=$(perf_timer_elapsed_ms "$start_time")
record_metric "shell_lib_loading" "$elapsed" 100 500 1000

# ============================================================================
# BASELINE 2: Agent Dispatch Performance
# ============================================================================
echo ""
echo "[2/6] Agent Dispatch Performance..."

# Simple agent dispatch
echo "  Testing simple agent dispatch..."
start_time=$(perf_timer_start)
echo "test" | timeout 30 "$BIN_DIR/euxis" butler "Echo: OK" claude >/dev/null 2>&1 && dispatch_success=true || dispatch_success=false
elapsed=$(perf_timer_elapsed_ms "$start_time")

if [[ "$dispatch_success" == "true" ]]; then
    record_metric "agent_dispatch_simple" "$elapsed" 3000 8000 15000
else
    record_metric "agent_dispatch_simple" "30000" 3000 8000 15000  # Timeout
    echo "  ❌ Agent dispatch failed or timed out"
fi

# ============================================================================
# BASELINE 3: Concurrent Load Testing (Scaled)
# ============================================================================
echo ""
echo "[3/6] Concurrent Load Testing..."

# 5 concurrent health checks (manageable load)
echo "  Testing 5x concurrent health checks..."
start_time=$(perf_timer_start)
pids=()
for i in {1..5}; do
    ("$BIN_DIR/euxis-health" --silent >/dev/null 2>&1) &
    pids+=($!)
done

for pid in "${pids[@]}"; do
    wait "$pid" 2>/dev/null || true
done
elapsed=$(perf_timer_elapsed_ms "$start_time")
record_metric "concurrent_health_5x" "$elapsed" 2000 5000 10000

# ============================================================================
# BASELINE 4: Memory Usage Profiling
# ============================================================================
echo ""
echo "[4/6] Memory Usage Profiling..."

if command -v ps &>/dev/null; then
    echo "  Measuring baseline memory usage..."

    # Baseline memory (shell process)
    baseline_rss=$(ps -o rss= -p $$ 2>/dev/null | tr -d ' ' || echo 0)
    baseline_mb=$((baseline_rss / 1024))
    record_metric "baseline_memory" "$baseline_mb" 10 25 50

    # Memory during TUI load
    echo "  Measuring TUI memory usage..."
    (
        python3 -c "
from tui.app import EuxisApp
import time, os
app = EuxisApp()
time.sleep(2)  # Allow memory to settle
print('TUI_LOADED')
time.sleep(3)
" >/dev/null 2>&1
    ) &
    tui_pid=$!

    sleep 2
    tui_rss=$(ps -o rss= -p "$tui_pid" 2>/dev/null | tr -d ' ' || echo 0)
    tui_mb=$((tui_rss / 1024))
    kill "$tui_pid" 2>/dev/null || true
    wait "$tui_pid" 2>/dev/null || true

    record_metric "tui_memory_usage" "$tui_mb" 30 60 100
fi

# ============================================================================
# BASELINE 5: Build & Asset Size Analysis
# ============================================================================
echo ""
echo "[5/6] Build & Asset Size Analysis..."

# Lint performance (closest to build time)
echo "  Measuring lint performance..."
start_time=$(perf_timer_start)
"$BIN_DIR/euxis-lint" >/dev/null 2>&1 || true
elapsed=$(perf_timer_elapsed_ms "$start_time")
record_metric "lint_time" "$elapsed" 2000 5000 10000

# Binary size analysis
echo "  Analyzing binary sizes..."
total_size=0
binary_count=0
largest_binary=0
largest_name=""

for binary in "$BIN_DIR"/*; do
    if [[ -f "$binary" && -x "$binary" ]]; then
        size=$(stat -f %z "$binary" 2>/dev/null || stat -c %s "$binary" 2>/dev/null || echo 0)
        total_size=$((total_size + size))
        binary_count=$((binary_count + 1))

        if (( size > largest_binary )); then
            largest_binary=$size
            largest_name=$(basename "$binary")
        fi
    fi
done

total_mb=$((total_size / 1024 / 1024))
largest_kb=$((largest_binary / 1024))

record_metric "total_binary_size_mb" "$total_mb" 20 50 100
record_metric "largest_binary_kb" "$largest_kb" 100 500 1000
record_metric "binary_count" "$binary_count" 50 80 120

echo "  Largest binary: $largest_name (${largest_kb}KB)"

# ============================================================================
# BASELINE 6: System Resource Limits
# ============================================================================
echo ""
echo "[6/6] System Resource Limits..."

# File descriptor usage
if command -v lsof &>/dev/null; then
    fd_count=$(lsof -p $$ 2>/dev/null | wc -l | tr -d ' ' || echo 0)
    record_metric "file_descriptors" "$fd_count" 20 50 100
fi

# Process count during operation
process_count=$(ps -u "$USER" | wc -l | tr -d ' ')
record_metric "user_processes" "$process_count" 50 100 200

# ============================================================================
# PERFORMANCE BUDGET ESTABLISHMENT
# ============================================================================
echo ""
echo "=================================================="
echo "  PERFORMANCE BUDGET SUMMARY"
echo "=================================================="

cat > "$RESULTS_DIR/performance_budgets.md" << EOF
# Euxis Performance Budgets (Established $(date -u +%Y-%m-%dT%H:%M:%SZ))

## Critical Path Latencies

| Metric | P50 Budget | P95 Budget | P99 Budget | Purpose |
|--------|------------|------------|------------|---------|
| Health Check | 500ms | 2000ms | 5000ms | Fast boot validation |
| TUI Startup | 800ms | 1500ms | 3000ms | User interface responsiveness |
| Shell Lib Loading | 100ms | 500ms | 1000ms | CLI startup time |
| Agent Dispatch | 3000ms | 8000ms | 15000ms | Single agent task |
| Concurrent Health 5x | 2000ms | 5000ms | 10000ms | Load testing |

## Memory Budgets

| Metric | P50 Budget | P95 Budget | P99 Budget | Purpose |
|--------|------------|------------|------------|---------|
| Baseline Memory | 10MB | 25MB | 50MB | Shell process memory |
| TUI Memory | 30MB | 60MB | 100MB | Python TUI memory |

## Asset Size Budgets

| Metric | P50 Budget | P95 Budget | P99 Budget | Purpose |
|--------|------------|------------|------------|---------|
| Total Binary Size | 20MB | 50MB | 100MB | Distribution size |
| Largest Binary | 100KB | 500KB | 1000KB | Individual binary size |
| Binary Count | 50 | 80 | 120 | Executable count |

## Operational Budgets

| Metric | P50 Budget | P95 Budget | P99 Budget | Purpose |
|--------|------------|------------|------------|---------|
| Lint Time | 2000ms | 5000ms | 10000ms | CI/CD pipeline |
| File Descriptors | 20 | 50 | 100 | Resource usage |
| User Processes | 50 | 100 | 200 | System load |

## Recommendations

1. **Health Check Optimization**: Current latency exceeds budget - investigate provider checks
2. **Memory Monitoring**: Establish continuous monitoring for memory leaks
3. **Asset Size Control**: Monitor binary bloat as system grows
4. **Concurrency Limits**: Test higher concurrent loads gradually
5. **Performance CI**: Integrate these benchmarks into CI pipeline

EOF

echo ""
echo "  Performance Budget Summary:"
echo "    Passed: $PASS metrics"
echo "    Failed: $FAIL metrics"
echo "    Results: $RESULTS_CSV"
echo "    Report: $RESULTS_DIR/performance_budgets.md"

echo ""
echo "=================================================="

# Create a simple benchmark command for regular testing
cat > "$RESULTS_DIR/quick_benchmark.sh" << 'EOF'
#!/usr/bin/env bash
# Quick performance check for regular use
echo "Quick Euxis Performance Check..."

start=$(date +%s%N 2>/dev/null || perl -MTime::HiRes=time -e 'printf "%.0f", time()*1e9')
$HOME/.euxis/bin/euxis-health --silent >/dev/null 2>&1
end=$(date +%s%N 2>/dev/null || perl -MTime::HiRes=time -e 'printf "%.0f", time()*1e9')
health_ms=$(( (end - start) / 1000000 ))

if (( health_ms > 5000 )); then
    echo "❌ Health check: ${health_ms}ms (FAIL - exceeds 5s budget)"
    exit 1
elif (( health_ms > 2000 )); then
    echo "⚠️  Health check: ${health_ms}ms (WARN - exceeds 2s budget)"
else
    echo "✅ Health check: ${health_ms}ms (PASS)"
fi
EOF

chmod +x "$RESULTS_DIR/quick_benchmark.sh"
echo "  Quick benchmark: $RESULTS_DIR/quick_benchmark.sh"

[[ $FAIL -eq 0 ]]