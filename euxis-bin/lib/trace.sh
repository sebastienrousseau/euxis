#!/usr/bin/env bash
[[ -n "${_EUXIS_LIB_TRACE:-}" ]] && return; _EUXIS_LIB_TRACE=1
#
# trace.sh - Black Box Recorder for Euxis operations
#
# Captures all agent interactions, council debates, and system events
# to .euxis/traces/ for replay and debugging.
#

EUXIS_HOME="${EUXIS_HOME:-$HOME/.euxis}"
TRACES_DIR="${EUXIS_HOME}/traces"

# Initialize trace directory
_trace_init() {
    mkdir -p "$TRACES_DIR"
}

# Generate a new trace ID
_trace_new_id() {
    echo "$(date +%s)-$$"
}

# Start a new trace session
# Usage: trace_start <type> <description>
# Types: council, squad, combo, playbook, agent, loop
trace_start() {
    local trace_type="$1"
    local description="$2"

    _trace_init

    TRACE_ID=$(_trace_new_id)
    TRACE_FILE="${TRACES_DIR}/${TRACE_ID}.json"
    TRACE_START_TIME=$(date -Iseconds)

    # Initialize trace file
    cat > "$TRACE_FILE" <<EOF
{
  "id": "$TRACE_ID",
  "type": "$trace_type",
  "description": "$description",
  "started_at": "$TRACE_START_TIME",
  "ended_at": null,
  "status": "running",
  "environment": {
    "euxis_version": "v0.0.3",
    "os": "$(uname -s)",
    "arch": "$(uname -m)",
    "shell": "$(basename "${SHELL:-unknown}")",
    "cwd": "$(pwd)"
  },
  "events": [
EOF

    TRACE_EVENT_COUNT=0
    export TRACE_ID TRACE_FILE TRACE_EVENT_COUNT

    echo "$TRACE_ID"
}

# Add an event to the current trace
# Usage: trace_event <agent> <action> <status> [message]
trace_event() {
    local agent="$1"
    local action="$2"
    local status="$3"
    local message="${4:-}"
    local timestamp=$(date -Iseconds)

    [[ -z "$TRACE_FILE" ]] && return 1
    [[ ! -f "$TRACE_FILE" ]] && return 1

    # Add comma if not first event
    if [[ $TRACE_EVENT_COUNT -gt 0 ]]; then
        echo "," >> "$TRACE_FILE"
    fi

    # Escape message for JSON
    local escaped_message
    escaped_message=$(printf '%s' "$message" | sed 's/\\/\\\\/g; s/"/\\"/g; s/\n/\\n/g; s/\t/\\t/g' | tr '\n' ' ')

    cat >> "$TRACE_FILE" <<EOF
    {
      "timestamp": "$timestamp",
      "agent": "$agent",
      "action": "$action",
      "status": "$status",
      "message": "$escaped_message"
    }
EOF

    TRACE_EVENT_COUNT=$((TRACE_EVENT_COUNT + 1))
    export TRACE_EVENT_COUNT
}

# Add agent output to trace
# Usage: trace_output <agent> <output>
trace_output() {
    local agent="$1"
    local output="$2"
    trace_event "$agent" "output" "data" "$output"
}

# Add error to trace
# Usage: trace_error <agent> <error_message>
trace_error() {
    local agent="$1"
    local error="$2"
    trace_event "$agent" "error" "failed" "$error"
}

# End the current trace
# Usage: trace_end <status>
# Status: success, failed, cancelled
trace_end() {
    local status="${1:-success}"
    local end_time=$(date -Iseconds)

    [[ -z "$TRACE_FILE" ]] && return 1
    [[ ! -f "$TRACE_FILE" ]] && return 1

    # Close the events array and complete the JSON
    cat >> "$TRACE_FILE" <<EOF

  ],
  "ended_at": "$end_time",
  "status": "$status",
  "event_count": $TRACE_EVENT_COUNT
}
EOF

    # Return the trace ID for reference
    echo "$TRACE_ID"
}

# List recent traces
# Usage: trace_list [limit]
trace_list() {
    local limit="${1:-10}"

    _trace_init

    ls -t "$TRACES_DIR"/*.json 2>/dev/null | head -n "$limit" | while read -r file; do
        local id=$(basename "$file" .json)
        local type=$(jq -r '.type // "unknown"' "$file" 2>/dev/null)
        local status=$(jq -r '.status // "unknown"' "$file" 2>/dev/null)
        local desc=$(jq -r '.description // ""' "$file" 2>/dev/null | head -c 50)
        local started=$(jq -r '.started_at // ""' "$file" 2>/dev/null)

        printf "%-20s %-10s %-10s %s\n" "$id" "$type" "$status" "$desc"
    done
}

# Get trace file path
# Usage: trace_get_file <trace_id>
trace_get_file() {
    local trace_id="$1"
    local file="${TRACES_DIR}/${trace_id}.json"

    if [[ -f "$file" ]]; then
        echo "$file"
        return 0
    else
        return 1
    fi
}

# Prune old traces (keep last N days)
# Usage: trace_prune [days]
trace_prune() {
    local days="${1:-30}"

    _trace_init

    find "$TRACES_DIR" -name "*.json" -type f -mtime +"$days" -delete 2>/dev/null
    echo "Pruned traces older than $days days"
}
