#!/usr/bin/env bash
## ============================================================================
## Euxis Library: Common Utilities
## ============================================================================
##
## Provide logging, PII sanitization, and performance tracking for all agents.
##
## This is the foundational library sourced by every Euxis component. It
## establishes consistent logging patterns, security-by-default sanitization,
## and performance instrumentation across the fleet.
##
## ARCHITECTURE:
##
##   ┌─────────────────┐
##   │  Any Euxis      │
##   │  Component      │
##   └────────┬────────┘
##            │ source
##            ▼
##   ┌─────────────────┐     ┌─────────────────┐
##   │   common.sh     │ ──► │     stderr      │  Logs (sanitized)
##   │   (this lib)    │     └─────────────────┘
##   └─────────────────┘
##            │
##            ▼
##   ┌─────────────────┐
##   │  PII Redaction  │  Email, API keys, tokens, IPs
##   └─────────────────┘
##
## LOGGING FUNCTIONS:
##
##   | Function    | Prefix           | Visibility | Use Case              |
##   |-------------|------------------|------------|-----------------------|
##   | log_info    | [euxis]          | Always     | Standard messages     |
##   | log_error   | [euxis] ERROR:   | Always     | Recoverable errors    |
##   | log_debug   | [euxis] DEBUG:   | EUXIS_DEBUG| Development/debug     |
##   | log_warn    | [euxis] WARN:    | Always     | Non-fatal warnings    |
##
## ENVIRONMENT VARIABLES:
##
##   | Variable          | Default | Description                         |
##   |-------------------|---------|-------------------------------------|
##   | EUXIS_HOME        | ~/.euxis| Installation directory              |
##   | EUXIS_DEBUG       | 0       | Enable debug logging (0|1)          |
##   | EUXIS_LOG_SANITIZE| 1       | Enable PII redaction (0|1)          |
##
## SECURITY:
##   - All log output passes through _sanitize_pii() by default
##   - Redacts: email addresses, API keys (sk-*, AKIA*), bearer tokens, IPs
##   - Disable sanitization only for debugging: EUXIS_LOG_SANITIZE=0
##
## DEPENDENCIES:
##   - bash 4.0+
##   - sed (for regex-based redaction)
##
## IDEMPOTENCY:
##   - Safe to source multiple times (include guard protects re-entry)
##
## USAGE:
##   source "${EUXIS_HOME}/euxis-core/lib/common.sh"
##   log_info "Deploying fleet"
##   log_error "Agent failed:" "$agent_name"
##   log_debug "Verbose:" "$detailed_info"
##
## ============================================================================

# Include guard — prevents duplicate sourcing
[[ -n "${_EUXIS_LIB_COMMON:-}" ]] && return; _EUXIS_LIB_COMMON=1

set -euo pipefail

EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"

# ============================================================================
# PII Sanitization (applied to all log output)
# ============================================================================
#
# Patterns redacted:
#   - Email:  user@domain.com         → [REDACTED_EMAIL]
#   - API:    sk-*, key-*, AKIA*      → [REDACTED_KEY]
#   - Token:  Bearer eyJ...           → Bearer [REDACTED_TOKEN]
#   - IPv4:   192.168.1.1             → [REDACTED_IP] (localhost preserved)

## _sanitize_pii — Redact sensitive patterns from log messages.
#
# DESCRIPTION:
#     Strips email addresses, API keys, bearer tokens, IP addresses,
#     and other PII from log messages before output. Enabled by default;
#     disable with EUXIS_LOG_SANITIZE=0 for debugging.
#
# ARGUMENTS:
#     $* (string)    Raw log message
#
# OUTPUTS:
#     stdout         Sanitized log message
_sanitize_pii() {
    local msg="$*"
    if [[ "${EUXIS_LOG_SANITIZE:-1}" == "0" ]]; then
        printf '%s' "$msg"
        return
    fi
    # Redact email addresses
    msg=$(printf '%s' "$msg" | sed -E 's/[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}/[REDACTED_EMAIL]/g')
    # Redact API keys (sk-*, key-*, AKIA*, etc.)
    msg=$(printf '%s' "$msg" | sed -E 's/(sk-|key-|AKIA)[a-zA-Z0-9]{16,}/[REDACTED_KEY]/g')
    # Redact bearer tokens
    msg=$(printf '%s' "$msg" | sed -E 's/[Bb]earer [a-zA-Z0-9._-]{20,}/Bearer [REDACTED_TOKEN]/g')
    # Redact IPv4 addresses (except localhost)
    msg=$(printf '%s' "$msg" | sed -E 's/([0-9]{1,3}\.){3}[0-9]{1,3}/[REDACTED_IP]/g')
    # Restore localhost
    msg=$(printf '%s' "$msg" | sed 's/\[REDACTED_IP\]:/<localhost>:/g; s/\[REDACTED_IP\]/<localhost>/g')
    printf '%s' "$msg"
}

# ============================================================================
# Logging Functions
# ============================================================================

# log_info - Log informational message to stderr
#
# DESCRIPTION:
#     Outputs informational messages with [euxis] prefix to stderr.
#     Used for standard operational messages that users should see.
#
# ARGUMENTS:
#     $* (string)    Message components to log
#
# OUTPUTS:
#     stderr         Formatted message with [euxis] prefix
#
# EXAMPLES:
#     log_info "Starting deployment"
#     log_info "Processed" $count "files"
log_info() {
    echo "[euxis] $(_sanitize_pii "$*")" >&2
}

# log_error - Log error message to stderr
#
# DESCRIPTION:
#     Outputs error messages with [euxis] ERROR: prefix to stderr.
#     Used for recoverable errors that need user attention.
#
# ARGUMENTS:
#     $* (string)    Error message components
#
# OUTPUTS:
#     stderr         Formatted error message
#
# EXAMPLES:
#     log_error "Failed to connect to provider"
#     log_error "Invalid agent:" "$agent_name"
log_error() {
    echo "[euxis] ERROR: $(_sanitize_pii "$*")" >&2
}

# log_debug - Log debug message to stderr (conditional)
#
# DESCRIPTION:
#     Outputs debug messages only when EUXIS_DEBUG=1. Used for
#     detailed diagnostic information during development and troubleshooting.
#
# ARGUMENTS:
#     $* (string)    Debug message components
#
# OUTPUTS:
#     stderr         Formatted debug message (when enabled)
#
# ENVIRONMENT:
#     EUXIS_DEBUG    Enable debug output (0|1, default: 0)
#
# EXAMPLES:
#     log_debug "Processing file" "$filename"
#     EUXIS_DEBUG=1 script.sh  # Enable debug output
log_debug() {
    [[ "${EUXIS_DEBUG:-0}" == "1" ]] && echo "[euxis] DEBUG: $(_sanitize_pii "$*")" >&2 || true
}

# log_warn - Log warning message to stderr
#
# DESCRIPTION:
#     Outputs warning messages with [euxis] WARN: prefix to stderr.
#     Used for non-fatal issues that may require attention.
#
# ARGUMENTS:
#     $* (string)    Warning message components
#
# OUTPUTS:
#     stderr         Formatted warning message
#
# EXAMPLES:
#     log_warn "Performance budget exceeded"
#     log_warn "Deprecated feature used:" "$feature"
log_warn() {
    echo "[euxis] WARN: $(_sanitize_pii "$*")" >&2
}

# ============================================================================
# Performance Instrumentation (latency tracking)
# ============================================================================

# ============================================================================
# Performance Instrumentation
# ============================================================================

# _perf_enabled - Check if performance tracking is enabled
#
# DESCRIPTION:
#     Determines if performance instrumentation should run. Can be disabled
#     to eliminate overhead in production environments.
#
# RETURNS:
#     0              Performance tracking enabled
#     1              Performance tracking disabled
#
# ENVIRONMENT:
#     EUXIS_PERF_DISABLE    Disable performance tracking (0|1, default: 0)
#
# EXAMPLES:
#     if _perf_enabled; then
#         # Capture timing
#     fi
_perf_enabled() {
    [[ "${EUXIS_PERF_DISABLE:-0}" != "1" ]]
}

# _euxis_now_ns - Get current time in nanoseconds
#
# DESCRIPTION:
#     Returns current time in nanoseconds with fallback for systems
#     that don't support nanosecond precision. Optimizes for macOS
#     compatibility by using perl when available.
#
# RETURNS:
#     0              Always succeeds
#
# OUTPUTS:
#     stdout         Current time in nanoseconds (string)
#
# SIDE EFFECTS:
#     Returns "0" when performance tracking is disabled
#
# EXAMPLES:
#     local start_time
#     start_time=$(_euxis_now_ns)
_euxis_now_ns() {
    _perf_enabled || { echo "0"; return; }

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

# _perf_start - Start a performance timer
#
# DESCRIPTION:
#     Captures the current time for latency measurement. Returns a timestamp
#     that can be passed to _perf_elapsed_ms() to calculate duration.
#
# RETURNS:
#     0              Always succeeds
#
# OUTPUTS:
#     stdout         Timestamp in nanoseconds for later comparison
#
# EXAMPLES:
#     local timer
#     timer=$(_perf_start)
#     # ... do work ...
#     local elapsed
#     elapsed=$(_perf_elapsed_ms "$timer")
_perf_start() {
    _perf_enabled || { echo "0"; return; }
    _euxis_now_ns
}

# _perf_elapsed_ms - Calculate elapsed time in milliseconds
#
# DESCRIPTION:
#     Calculates the time elapsed since a timer started, returning
#     the result in milliseconds for human-readable reporting.
#
# ARGUMENTS:
#     $1 (string)    Start timestamp from _perf_start()
#
# RETURNS:
#     0              Always succeeds
#
# OUTPUTS:
#     stdout         Elapsed time in milliseconds (integer)
#
# EXAMPLES:
#     local timer elapsed
#     timer=$(_perf_start)
#     sleep 1
#     elapsed=$(_perf_elapsed_ms "$timer")
#     echo "Operation took ${elapsed}ms"
_perf_elapsed_ms() {
    _perf_enabled || { echo "0"; return; }
    local start="$1"
    local end
    end=$(_euxis_now_ns)
    echo $(( (end - start) / 1000000 ))
}

# Default performance budgets (milliseconds)
declare -A EUXIS_PERF_BUDGETS=(
    ["llm_call"]=5000
    ["file_read"]=100
    ["file_write"]=200
    ["agent_dispatch"]=1000
    ["cortex_recall"]=500
)

# performance_budget - Set or get performance budget for operations
#
# DESCRIPTION:
#     Manages performance budgets for different operations. Can set new
#     budgets or retrieve existing ones. Used for latency monitoring.
#
# ARGUMENTS:
#     $1 (string)    Operation name
#     $2 (int)       New budget in milliseconds (optional)
#
# RETURNS:
#     0              Always succeeds
#
# OUTPUTS:
#     stdout         Current budget for operation (when getting)
#
# SIDE EFFECTS:
#     Modifies EUXIS_PERF_BUDGETS associative array
#     Logs debug message when setting budget
#
# EXAMPLES:
#     # Set budget
#     performance_budget "llm_call" 3000
#
#     # Get budget
#     budget=$(performance_budget "file_read")
performance_budget() {
    local operation="$1"
    local budget="${2:-}"
    # Associative array subscript access triggers set -u for non-existent keys
    local _prev_u=false; [[ -o nounset ]] && _prev_u=true; set +u

    if [[ -n "$budget" ]]; then
        EUXIS_PERF_BUDGETS["$operation"]="$budget"
        log_debug "PERF: Set budget for ${operation} to ${budget}ms"
    else
        echo "${EUXIS_PERF_BUDGETS[$operation]:-1000}"
    fi

    $_prev_u && set -u || true
}

# _perf_check_budget - Verify operation completed within budget
#
# DESCRIPTION:
#     Checks if an operation's elapsed time is within the configured
#     performance budget. Logs warnings for budget violations.
#
# ARGUMENTS:
#     $1 (int)       Elapsed time in milliseconds
#     $2 (int)       Budget in milliseconds (optional)
#     $3 (string)    Operation name (optional, default: "unknown")
#
# RETURNS:
#     0              Within budget
#     1              Budget exceeded
#
# SIDE EFFECTS:
#     Logs warning when budget is exceeded
#     Logs debug message when within budget
#
# EXAMPLES:
#     timer=$(_perf_start)
#     # ... operation ...
#     elapsed=$(_perf_elapsed_ms "$timer")
#     _perf_check_budget "$elapsed" "" "file_operation"
_perf_check_budget() {
    _perf_enabled || return 0

    local elapsed="$1"
    local budget="$2"
    local operation="${3:-unknown}"

    # Use configured budget if not specified
    if [[ -z "$budget" ]]; then
        budget=$(performance_budget "$operation")
    fi

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
EUXIS_PERF_LOG="${EUXIS_HOME}/euxis-runtime/data/perf/metrics.jsonl"

# _perf_record - Record performance metric to JSONL log
#
# DESCRIPTION:
#     Appends a performance metric record to the system-wide performance
#     log in JSONL format. Used for monitoring and analysis of fleet
#     coordination overhead.
#
# ARGUMENTS:
#     $1 (string)    Operation name
#     $2 (int)       Elapsed time in milliseconds
#     $3 (string)    Agent name (optional, default: "system")
#     $4 (string)    Status (optional, default: "ok")
#
# RETURNS:
#     0              Always succeeds
#
# SIDE EFFECTS:
#     Creates performance directory if it doesn't exist
#     Appends JSONL record to metrics file
#
# EXAMPLES:
#     _perf_record "llm_call" 2500 "architect" "success"
#     _perf_record "file_read" 150 "system" "ok"
_perf_record() {
    _perf_enabled || return 0

    local operation="$1"
    local elapsed_ms="$2"
    local agent="${3:-system}"
    local status="${4:-ok}"

    local perf_dir="${EUXIS_HOME}/euxis-runtime/data/perf"
    [[ -d "${perf_dir}" ]] || mkdir -p "${perf_dir}"

    # Sanitize agent name and status for metrics (anonymize when enabled)
    local safe_agent="${agent}"
    local safe_status="${status}"
    if [[ "${EUXIS_PERF_ANONYMIZE:-0}" == "1" ]]; then
        safe_agent="agent-$(printf '%s' "${agent}" | cksum | cut -d' ' -f1)"
        safe_status="ok"
    fi

    if command -v jq &>/dev/null; then
        jq -n \
            --arg ts "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
            --arg op "${operation}" \
            --arg agent "${safe_agent}" \
            --argjson ms "${elapsed_ms}" \
            --arg status "${safe_status}" \
            '{ts:$ts,op:$op,agent:$agent,ms:$ms,status:$status}' \
            >> "${EUXIS_PERF_LOG}" 2>/dev/null || true
    else
        printf '{"ts":"%s","op":"%s","agent":"%s","ms":%s,"status":"%s"}\n' \
            "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
            "${operation}" \
            "${safe_agent}" \
            "${elapsed_ms}" \
            "${safe_status}" \
            >> "${EUXIS_PERF_LOG}" 2>/dev/null || true
    fi
}

# ============================================================================
# UI Elements
# ============================================================================

# Global spinner process ID
_spinner_pid=""

# start_spinner - Display animated spinner for long operations
#
# DESCRIPTION:
#     Starts an animated spinner in the background to provide visual
#     feedback during long-running operations like LLM calls.
#
# ARGUMENTS:
#     $1 (string)    Status message (optional, default: "Thinking...")
#
# RETURNS:
#     0              Always succeeds
#
# SIDE EFFECTS:
#     Starts background process stored in _spinner_pid
#     Outputs spinning animation to stderr
#
# EXAMPLES:
#     start_spinner "Processing..."
#     # ... long operation ...
#     stop_spinner
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

# stop_spinner - Stop animated spinner and clear line
#
# DESCRIPTION:
#     Stops the background spinner process and clears the spinner
#     line from the terminal. Should be called after start_spinner.
#
# RETURNS:
#     0              Always succeeds
#
# SIDE EFFECTS:
#     Kills background spinner process
#     Clears current terminal line
#     Resets _spinner_pid to empty
#
# EXAMPLES:
#     start_spinner "Loading..."
#     sleep 2
#     stop_spinner
#     echo "Done!"
stop_spinner() {
    if [[ -n "${_spinner_pid}" ]]; then
        kill "${_spinner_pid}" 2>/dev/null
        wait "${_spinner_pid}" 2>/dev/null || true
        printf '\r\033[K' >&2
        _spinner_pid=""
    fi
}
