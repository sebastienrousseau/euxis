#!/usr/bin/env bash
# Euxis Library: Common utilities (logging, spinner, paths)
[[ -n "${_EUXIS_LIB_COMMON:-}" ]] && return; _EUXIS_LIB_COMMON=1

EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"

log_info() {
    echo "[euxis] $*" >&2
}

log_error() {
    echo "[euxis] ERROR: $*" >&2
}

log_debug() {
    [[ "${EUXIS_DEBUG:-0}" == "1" ]] && echo "[euxis] DEBUG: $*" >&2 || true
}

log_warn() {
    echo "[euxis] WARN: $*" >&2
}

# ============================================================================
# Performance Instrumentation (latency tracking)
# ============================================================================

# Nanosecond timer (falls back to seconds if %N unavailable)
_euxis_now_ns() {
    local ns
    ns=$(date +%s%N 2>/dev/null)
    if [[ "${ns}" == *N ]]; then
        # %N not supported (e.g., macOS) — fall back to ms via perl or seconds
        if command -v perl &>/dev/null; then
            perl -MTime::HiRes=time -e 'printf "%.0f", time()*1e9'
        else
            echo "$(date +%s)000000000"
        fi
    else
        printf '%s' "${ns}"
    fi
}

# Start a latency timer. Usage: local t; t=$(_perf_start)
_perf_start() { _euxis_now_ns; }

# End a latency timer and return elapsed ms. Usage: _perf_elapsed "$t"
_perf_elapsed_ms() {
    local start="$1"
    local end
    end=$(_euxis_now_ns)
    echo $(( (end - start) / 1000000 ))
}

# Latency budget enforcement. Returns 0 if within budget, 1 if exceeded.
# Usage: _perf_check_budget "$elapsed_ms" "$budget_ms" "operation_name"
_perf_check_budget() {
    local elapsed="$1"
    local budget="$2"
    local operation="${3:-unknown}"
    if (( elapsed > budget )); then
        log_warn "LATENCY BUDGET EXCEEDED: ${operation} took ${elapsed}ms (budget: ${budget}ms)"
        return 1
    fi
    log_debug "PERF: ${operation} completed in ${elapsed}ms (budget: ${budget}ms)"
    return 0
}

# ============================================================================
# Coordination Overhead Tracking
# ============================================================================

# Global coordination metrics file (per-session)
EUXIS_PERF_LOG="${EUXIS_HOME}/perf/metrics.jsonl"

# Record a performance metric as JSONL
_perf_record() {
    local operation="$1"
    local elapsed_ms="$2"
    local agent="${3:-system}"
    local status="${4:-ok}"

    local perf_dir="${EUXIS_HOME}/perf"
    [[ -d "${perf_dir}" ]] || mkdir -p "${perf_dir}"

    printf '{"ts":"%s","op":"%s","agent":"%s","ms":%s,"status":"%s"}\n' \
        "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
        "${operation}" \
        "${agent}" \
        "${elapsed_ms}" \
        "${status}" \
        >> "${EUXIS_PERF_LOG}"
}

# Spinner for LLM wait time (runs in background)
_spinner_pid=""

start_spinner() {
    local msg="${1:-Thinking...}"
    (
        local chars='⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏'
        local i=0
        while true; do
            printf '\r[euxis] %s %s' "${chars:i%10:1}" "${msg}" >&2
            i=$((i + 1))
            sleep 0.1
        done
    ) &
    _spinner_pid=$!
}

stop_spinner() {
    if [[ -n "${_spinner_pid}" ]]; then
        kill "${_spinner_pid}" 2>/dev/null
        wait "${_spinner_pid}" 2>/dev/null || true
        printf '\r\033[K' >&2
        _spinner_pid=""
    fi
}
