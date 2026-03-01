#!/usr/bin/env bash
[[ -n "${_EUXIS_LIB_MESH:-}" ]] && return; _EUXIS_LIB_MESH=1
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com
#
# mesh.sh — Peer-to-peer agent communication protocol
#
# Implements A2A (Agent-to-Agent) communication for mesh dispatch mode:
#   - Discovery: Agents find peers via capability tags in registry
#   - State Sync: Shared state via .euxis/mesh/state.json (no context bloat)
#   - Hand-off: Message passing with deadlock detection
#
# Protocol: JSON-based "Agent Cards" compatible with A2A standard
#
# Usage:
#   source "${EUXIS_HOME}/euxis-bin/lib/mesh.sh"
#   mesh_init "agent-id"
#   mesh_discover "formatting"  # Find agent with capability
#   mesh_send "target-agent" "message"
#   mesh_receive  # Check for incoming messages

# Note: Avoiding set -euo pipefail as this is a library meant to be sourced
# The calling script should handle its own error settings

# ============================================================================
# Configuration
# ============================================================================

EUXIS_HOME="${EUXIS_HOME:-$HOME/.euxis}"
MESH_DIR="${EUXIS_HOME}/euxis-data/runtime/mesh"
MESH_STATE="${MESH_DIR}/state.json"
MESH_LOCK="${MESH_DIR}/.state.lock"
MESH_INBOX="${MESH_DIR}/inbox"
MESH_REGISTRY="${EUXIS_HOME}/euxis-data/agents/registry.json"

# Timeouts
MESH_SEND_TIMEOUT="${MESH_SEND_TIMEOUT:-30}"      # Seconds to wait for delivery
MESH_RECEIVE_TIMEOUT="${MESH_RECEIVE_TIMEOUT:-5}" # Seconds to poll for messages
MESH_DEADLOCK_TIMEOUT="${MESH_DEADLOCK_TIMEOUT:-60}" # Seconds before deadlock detection

# Current agent identity (set via mesh_init)
_MESH_AGENT_ID=""
_MESH_SESSION_ID=""

# Portable UTC timestamp with millisecond precision (without sourcing common.sh)
_mesh_ts_utc_ms() {
    if command -v gdate &>/dev/null; then
        gdate -u +"%Y-%m-%dT%H:%M:%S.%3NZ"
        return
    fi
    if command -v python3 &>/dev/null; then
        python3 -c 'from datetime import datetime, timezone; print(datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z")'
        return
    fi
    date -u +"%Y-%m-%dT%H:%M:%SZ"
}

# Portable monotonic-ish message id suffix for filenames
_mesh_msg_id() {
    if command -v python3 &>/dev/null; then
        python3 -c 'import time; print(time.time_ns())'
        return
    fi
    if command -v perl &>/dev/null; then
        perl -MTime::HiRes=time -e 'printf "%.0f", time()*1e9'
        return
    fi
    date +%s
}

# ============================================================================
# Locking (for concurrent safety)
# ============================================================================

# Internal: perform locked write operation
# Usage: _mesh_locked_write "jq_command" "args..."
# This is the core pattern: read state, transform, write atomically under lock
_mesh_locked_write() {
    local jq_filter="$1"
    shift
    local jq_args=("$@")

    # Create lock file if needed
    mkdir -p "$(dirname "$MESH_LOCK")"

    if command -v flock &>/dev/null; then
        # flock available - use subshell pattern for clean fd handling
        (
            flock -w 5 200 || exit 1

            # Ensure state file exists
            if [[ ! -f "$MESH_STATE" ]]; then
                cat > "$MESH_STATE" <<'INITEOF'
{
  "version": "1.0",
  "created": "",
  "agents": {},
  "shared": {},
  "locks": {}
}
INITEOF
            fi

            local tmp="${MESH_STATE}.tmp.$$"
            if jq "${jq_args[@]}" "$jq_filter" "$MESH_STATE" > "$tmp" 2>/dev/null; then
                mv "$tmp" "$MESH_STATE"
            else
                rm -f "$tmp" 2>/dev/null
                exit 1
            fi
        ) 200>"$MESH_LOCK"
    else
        # Fallback for systems without flock (macOS without coreutils)
        # Use mkdir as atomic lock
        local lock_dir="${MESH_LOCK}.d"
        local waited=0
        while ! mkdir "$lock_dir" 2>/dev/null; do
            sleep 0.1
            waited=$((waited + 1))
            if [[ $waited -gt 50 ]]; then
                return 1  # 5 second timeout
            fi
        done

        # Ensure state file exists
        if [[ ! -f "$MESH_STATE" ]]; then
            cat > "$MESH_STATE" <<'INITEOF'
{
  "version": "1.0",
  "created": "",
  "agents": {},
  "shared": {},
  "locks": {}
}
INITEOF
        fi

        local tmp="${MESH_STATE}.tmp.$$"
        local rc=0
        if jq "${jq_args[@]}" "$jq_filter" "$MESH_STATE" > "$tmp" 2>/dev/null; then
            mv "$tmp" "$MESH_STATE"
        else
            rm -f "$tmp" 2>/dev/null
            rc=1
        fi

        rmdir "$lock_dir" 2>/dev/null || true
        return $rc
    fi
}

# ============================================================================
# Initialization
# ============================================================================

# Initialize mesh for an agent
# Usage: mesh_init "agent-id" ["session-id"]
mesh_init() {
    local agent_id="$1"
    local session_id="${2:-$(date +%Y%m%d-%H%M%S)-$$}"

    _MESH_AGENT_ID="$agent_id"
    _MESH_SESSION_ID="$session_id"

    # Create mesh directories
    mkdir -p "$MESH_DIR"
    mkdir -p "$MESH_INBOX/$agent_id"

    # Initialize state file if not exists
    if [[ ! -f "$MESH_STATE" ]]; then
        mesh_state_init
    fi

    # Register agent as online
    mesh_state_set "agents.$agent_id.status" "online"
    mesh_state_set "agents.$agent_id.session" "$session_id"
    mesh_state_set "agents.$agent_id.last_seen" "$(date -u +%Y-%m-%dT%H:%M:%SZ)"

    # Note: cleanup trap disabled to avoid conflicts with parent script traps
    # trap '_mesh_cleanup' EXIT
}

# Cleanup on exit
_mesh_cleanup() {
    if [[ -n "$_MESH_AGENT_ID" ]]; then
        mesh_state_set "agents.$_MESH_AGENT_ID.status" "offline" 2>/dev/null || true
    fi
}

# Register agent with capabilities (extends mesh_init)
# Usage: mesh_register "agent-id" "capability1,capability2,..." ["session-id"]
# This is the full bootstrap call that makes an agent discoverable
mesh_register() {
    local agent_id="$1"
    local capabilities="$2"
    local session_id="${3:-$(date +%Y%m%d-%H%M%S)-$$}"

    # Initialize base mesh state
    mesh_init "$agent_id" "$session_id"

    # Store capabilities as JSON array for discovery
    local caps_json
    if [[ -n "$capabilities" ]]; then
        # Convert comma-separated to JSON array: "a,b,c" → ["a","b","c"]
        caps_json=$(echo "$capabilities" | tr ',' '\n' | jq -R . | jq -s .)
    else
        caps_json="[]"
    fi

    # Register capabilities in mesh state
    mesh_state_set_json "agents.$agent_id.capabilities" "$caps_json"
    mesh_state_set "agents.$agent_id.pid" "$$"

    echo "[mesh] Agent '$agent_id' registered with capabilities: $capabilities" >&2
}

# Discover agents by capability from mesh state (runtime discovery)
# Usage: mesh_discover_runtime "capability" → returns online agent IDs
# Unlike mesh_discover (registry-based), this finds currently active agents
mesh_discover_runtime() {
    local capability="$1"

    if [[ ! -f "$MESH_STATE" ]]; then
        return 1
    fi

    # Find online agents with matching capability
    jq -r --arg cap "$capability" '
        .agents | to_entries[] |
        select(.value.status == "online") |
        select(.value.capabilities[]? == $cap) |
        .key
    ' "$MESH_STATE" 2>/dev/null
}

# Get agent info from mesh state (runtime)
# Usage: mesh_agent_info "agent-id" → JSON with status, capabilities, last_seen
mesh_agent_info() {
    local agent_id="$1"

    if [[ ! -f "$MESH_STATE" ]]; then
        echo "{}"
        return 1
    fi

    # Build path array for getpath
    local path_array="[\"agents\",\"$agent_id\"]"
    jq "getpath($path_array) // {}" "$MESH_STATE"
}

# Initialize empty state file
mesh_state_init() {
    cat > "$MESH_STATE" <<'EOF'
{
  "version": "1.0",
  "created": "",
  "agents": {},
  "shared": {},
  "locks": {}
}
EOF
    # Set creation timestamp
    local ts
    ts=$(date -u +%Y-%m-%dT%H:%M:%SZ)
    local tmp="${MESH_STATE}.tmp"
    jq --arg ts "$ts" '.created = $ts' "$MESH_STATE" > "$tmp" && mv "$tmp" "$MESH_STATE"
}

# ============================================================================
# Discovery (A2A Protocol)
# ============================================================================

# Discover agents by capability tag
# Usage: mesh_discover "capability" → returns agent IDs (newline-separated)
mesh_discover() {
    local capability="$1"

    if [[ ! -f "$MESH_REGISTRY" ]]; then
        echo "[mesh] Registry not found: $MESH_REGISTRY" >&2
        return 1
    fi

    # Find agents with matching capability_tags
    jq -r --arg cap "$capability" \
        '.agents[] | select(.capability_tags[]? == $cap) | .id' \
        "$MESH_REGISTRY" 2>/dev/null
}

# Get agent card (A2A format)
# Usage: mesh_agent_card "agent-id" → JSON
mesh_agent_card() {
    local agent_id="$1"

    if [[ ! -f "$MESH_REGISTRY" ]]; then
        return 1
    fi

    jq --arg id "$agent_id" '.agents[] | select(.id == $id)' "$MESH_REGISTRY"
}

# Check if agent is online
# Usage: mesh_is_online "agent-id" → 0 if online, 1 if offline
mesh_is_online() {
    local agent_id="$1"

    local status
    status=$(mesh_state_get "agents.$agent_id.status" 2>/dev/null)
    [[ "$status" == "online" ]]
}

# List all online agents
mesh_list_online() {
    jq -r '.agents | to_entries[] | select(.value.status == "online") | .key' "$MESH_STATE" 2>/dev/null
}

# ============================================================================
# State Sync (Shared Clipboard)
# ============================================================================

# Get value from shared state
# Usage: mesh_state_get "path.to.key" → value
# Note: Uses getpath() to handle keys with special chars (hyphens, etc.)
mesh_state_get() {
    local path="$1"

    if [[ ! -f "$MESH_STATE" ]]; then
        return 1
    fi

    # Convert dot notation to array for getpath
    # e.g., "agents.my-agent.status" → ["agents","my-agent","status"]
    local path_array
    path_array=$(echo "$path" | sed 's/\./","/g' | sed 's/^/["/' | sed 's/$/"]/')

    jq -r "getpath($path_array) // empty" "$MESH_STATE"
}

# Set value in shared state (atomic + locked)
# Usage: mesh_state_set "path.to.key" "value"
# Note: Uses setpath() to handle keys with special chars (hyphens, etc.)
mesh_state_set() {
    local path="$1"
    local value="$2"

    # Convert dot notation to array for setpath
    local path_array
    path_array=$(echo "$path" | sed 's/\./","/g' | sed 's/^/["/' | sed 's/$/"]/')

    _mesh_locked_write "setpath($path_array; \$val)" --arg val "$value"
}

# Set JSON object in shared state (atomic + locked)
# Usage: mesh_state_set_json "path.to.key" '{"foo": "bar"}'
mesh_state_set_json() {
    local path="$1"
    local json="$2"

    # Convert dot notation to array for setpath
    local path_array
    path_array=$(echo "$path" | sed 's/\./","/g' | sed 's/^/["/' | sed 's/$/"]/')

    _mesh_locked_write "setpath($path_array; \$val)" --argjson val "$json"
}

# Write to shared research log (append-only, locked)
# Usage: mesh_log "Research completed: found 5 results"
mesh_log() {
    local message="$1"
    local timestamp
    timestamp=$(date -u +%Y-%m-%dT%H:%M:%SZ)

    local entry
    entry=$(jq -n \
        --arg ts "$timestamp" \
        --arg agent "$_MESH_AGENT_ID" \
        --arg msg "$message" \
        '{"timestamp": $ts, "agent": $agent, "message": $msg}')

    _mesh_locked_write '.shared.log = (.shared.log // []) + [$entry]' --argjson entry "$entry"
}

# Read shared log
mesh_log_read() {
    jq -r '.shared.log // []' "$MESH_STATE"
}

# ============================================================================
# Message Passing (Hand-off)
# ============================================================================

# Send message to another agent
# Usage: mesh_send "target-agent" "message" ["priority"]
mesh_send() {
    local target="$1"
    local message="$2"
    local priority="${3:-P1}"

    local target_inbox="$MESH_INBOX/$target"
    mkdir -p "$target_inbox"

    local timestamp
    timestamp=$(_mesh_msg_id)
    local msg_file="$target_inbox/${timestamp}-${_MESH_AGENT_ID}.msg"

    # Create message envelope
    cat > "$msg_file" <<EOF
{
  "from": "$_MESH_AGENT_ID",
  "to": "$target",
  "timestamp": "$(_mesh_ts_utc_ms)",
  "priority": "$priority",
  "session": "$_MESH_SESSION_ID",
  "message": $(echo "$message" | jq -Rs .)
}
EOF

    echo "[mesh] Sent message to $target" >&2
}

# Receive messages for current agent
# Usage: mesh_receive → prints messages as JSON array
mesh_receive() {
    local inbox="$MESH_INBOX/$_MESH_AGENT_ID"

    if [[ ! -d "$inbox" ]]; then
        echo "[]"
        return
    fi

    local messages="[]"
    for msg_file in "$inbox"/*.msg; do
        [[ -f "$msg_file" ]] || continue

        local content
        content=$(cat "$msg_file")
        messages=$(echo "$messages" | jq --argjson msg "$content" '. + [$msg]')

        # Mark as read (move to .read)
        mv "$msg_file" "${msg_file}.read" 2>/dev/null || true
    done

    echo "$messages"
}

# Check for pending messages (non-blocking)
# Usage: mesh_has_messages → 0 if messages, 1 if empty
mesh_has_messages() {
    local inbox="$MESH_INBOX/$_MESH_AGENT_ID"
    [[ -d "$inbox" ]] && ls "$inbox"/*.msg 1>/dev/null 2>&1
}

# Wait for message with timeout
# Usage: mesh_wait_message [timeout_seconds] → 0 if received, 1 if timeout
mesh_wait_message() {
    local timeout="${1:-$MESH_RECEIVE_TIMEOUT}"
    local waited=0

    while [[ $waited -lt $timeout ]]; do
        if mesh_has_messages; then
            return 0
        fi
        sleep 1
        ((waited++))
    done

    return 1  # Timeout
}

# ============================================================================
# Deadlock Detection
# ============================================================================

# Update agent heartbeat
mesh_heartbeat() {
    mesh_state_set "agents.$_MESH_AGENT_ID.last_seen" "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    mesh_state_set "agents.$_MESH_AGENT_ID.status" "active"
}

# Mark agent as waiting for another
mesh_waiting_for() {
    local target="$1"
    mesh_state_set "agents.$_MESH_AGENT_ID.waiting_for" "$target"
    mesh_state_set "agents.$_MESH_AGENT_ID.status" "waiting"
}

# Clear waiting status
mesh_clear_waiting() {
    mesh_state_set "agents.$_MESH_AGENT_ID.waiting_for" ""
    mesh_state_set "agents.$_MESH_AGENT_ID.status" "active"
}

# Check for deadlock (A waiting for B waiting for A)
# Usage: mesh_check_deadlock → returns 0 if deadlock detected
mesh_check_deadlock() {
    local visited=("$_MESH_AGENT_ID")
    local current="$_MESH_AGENT_ID"

    while true; do
        local waiting_for
        waiting_for=$(mesh_state_get "agents.$current.waiting_for")

        [[ -z "$waiting_for" ]] && return 1  # No deadlock

        # Check if we've seen this agent (cycle)
        for v in "${visited[@]}"; do
            if [[ "$v" == "$waiting_for" ]]; then
                echo "[mesh] DEADLOCK DETECTED: ${visited[*]} → $waiting_for" >&2
                return 0
            fi
        done

        visited+=("$waiting_for")
        current="$waiting_for"
    done
}

# Detect idle agents (no heartbeat for DEADLOCK_TIMEOUT)
mesh_detect_idle() {
    local now
    now=$(date +%s)

    jq -r '.agents | to_entries[] | "\(.key) \(.value.last_seen // "1970-01-01T00:00:00Z")"' "$MESH_STATE" 2>/dev/null | \
    while read -r agent_id last_seen; do
        local last_ts
        last_ts=$(date -d "$last_seen" +%s 2>/dev/null || echo 0)
        local age=$((now - last_ts))

        if [[ $age -gt $MESH_DEADLOCK_TIMEOUT ]]; then
            echo "$agent_id"
        fi
    done
}

# ============================================================================
# Hand-off Protocol
# ============================================================================

# Request hand-off to another agent
# Usage: mesh_handoff "target-agent" "task-description" → waits for ack
mesh_handoff() {
    local target="$1"
    local task="$2"

    # Check target is online
    if ! mesh_is_online "$target"; then
        echo "[mesh] Target agent '$target' is not online" >&2
        return 1
    fi

    # Mark as waiting
    mesh_waiting_for "$target"

    # Send hand-off request
    local request
    request=$(jq -n \
        --arg type "handoff" \
        --arg task "$task" \
        '{"type": $type, "task": $task}')

    mesh_send "$target" "$request" "P0"

    # Wait for acknowledgment
    local timeout=$MESH_SEND_TIMEOUT
    local waited=0

    while [[ $waited -lt $timeout ]]; do
        # Check for deadlock
        if mesh_check_deadlock; then
            mesh_clear_waiting
            return 2  # Deadlock
        fi

        # Check for response
        if mesh_has_messages; then
            local response
            response=$(mesh_receive | jq -r '.[0].message // empty')
            if echo "$response" | jq -e '.type == "ack"' &>/dev/null; then
                mesh_clear_waiting
                return 0  # Success
            fi
        fi

        sleep 1
        ((waited++))
        mesh_heartbeat
    done

    mesh_clear_waiting
    return 1  # Timeout
}

# Acknowledge hand-off request
mesh_ack() {
    local from="$1"
    local ack
    ack=$(jq -n '{"type": "ack", "status": "accepted"}')
    mesh_send "$from" "$ack" "P0"
}

# ============================================================================
# Utility Functions
# ============================================================================

# Print mesh status
mesh_status() {
    echo "┌─────────────────────────────────────────────"
    echo "│ Mesh Status"
    echo "├─────────────────────────────────────────────"
    echo "│ Agent: $_MESH_AGENT_ID"
    echo "│ Session: $_MESH_SESSION_ID"
    echo "├─────────────────────────────────────────────"
    echo "│ Online Agents:"
    mesh_list_online | while read -r agent; do
        local status
        status=$(mesh_state_get "agents.$agent.status")
        printf "│   %s (%s)\n" "$agent" "$status"
    done
    echo "├─────────────────────────────────────────────"
    local inbox_count=0
    [[ -d "$MESH_INBOX/$_MESH_AGENT_ID" ]] && \
        inbox_count=$(ls "$MESH_INBOX/$_MESH_AGENT_ID"/*.msg 2>/dev/null | wc -l || echo 0)
    echo "│ Inbox: $inbox_count message(s)"
    echo "└─────────────────────────────────────────────"
}

# Clean up old messages and state
mesh_cleanup() {
    # Remove read messages older than 1 hour
    find "$MESH_INBOX" -name "*.msg.read" -mmin +60 -delete 2>/dev/null || true

    # Remove offline agents from state (locked)
    _mesh_locked_write '.agents |= with_entries(select(.value.status != "offline"))'
}

# ============================================================================
# Flight Recorder — Durable Execution State Persistence
# ============================================================================
# Records mission state snapshots for time-travel debugging and crash recovery.
# Snapshots are stored in JSONL format (append-only) for durability.

FLIGHT_RECORDER_DIR="${EUXIS_HOME}/euxis-data/runtime/history"
_FLIGHT_MISSION_ID=""
_FLIGHT_MISSION_FILE=""

# Initialize flight recorder for a mission
# Usage: flight_init "mission-id" ["description"]
flight_init() {
    local mission_id="$1"
    local description="${2:-}"

    _FLIGHT_MISSION_ID="$mission_id"
    mkdir -p "$FLIGHT_RECORDER_DIR"
    _FLIGHT_MISSION_FILE="${FLIGHT_RECORDER_DIR}/${mission_id}.jsonl"

    # Record mission start event
    local start_event
    start_event=$(jq -n \
        --arg type "mission_start" \
        --arg mission_id "$mission_id" \
        --arg description "$description" \
        --arg timestamp "$(_mesh_ts_utc_ms)" \
        --arg agent "$_MESH_AGENT_ID" \
        '{
            type: $type,
            mission_id: $mission_id,
            description: $description,
            timestamp: $timestamp,
            initiator: $agent
        }')

    echo "$start_event" >> "$_FLIGHT_MISSION_FILE"
    echo "[flight] Recording started: $mission_id" >&2
}

# Record a state snapshot
# Usage: flight_snapshot "event_type" "data_json"
# Event types: agent_start, agent_complete, thought, tool_call, tool_result,
#              checkpoint, error, handoff, decision
flight_snapshot() {
    local event_type="$1"
    local data="$2"

    [[ -z "$_FLIGHT_MISSION_FILE" ]] && return 1

    local snapshot
    snapshot=$(jq -n \
        --arg type "$event_type" \
        --arg timestamp "$(_mesh_ts_utc_ms)" \
        --arg agent "$_MESH_AGENT_ID" \
        --arg session "$_MESH_SESSION_ID" \
        --argjson data "$data" \
        '{
            type: $type,
            timestamp: $timestamp,
            agent: $agent,
            session: $session,
            data: $data
        }')

    echo "$snapshot" >> "$_FLIGHT_MISSION_FILE"
}

# Record agent thought stream (for live thought visualization)
# Usage: flight_thought "thought content"
flight_thought() {
    local thought="$1"
    flight_snapshot "thought" "$(jq -n --arg t "$thought" '{content: $t}')"
}

# Record tool invocation
# Usage: flight_tool_call "tool_name" "args_json"
flight_tool_call() {
    local tool="$1"
    local args="$2"
    flight_snapshot "tool_call" "$(jq -n --arg t "$tool" --argjson a "$args" '{tool: $t, args: $a}')"
}

# Record tool result
# Usage: flight_tool_result "tool_name" "result_json" "success|error"
flight_tool_result() {
    local tool="$1"
    local result="$2"
    local status="${3:-success}"
    flight_snapshot "tool_result" "$(jq -n --arg t "$tool" --argjson r "$result" --arg s "$status" '{tool: $t, result: $r, status: $s}')"
}

# Record a checkpoint (resumable state)
# Usage: flight_checkpoint "checkpoint_name" "state_json"
flight_checkpoint() {
    local name="$1"
    local state="$2"

    # Also capture current mesh state
    local mesh_state="{}"
    [[ -f "$MESH_STATE" ]] && mesh_state=$(cat "$MESH_STATE")
    local provider_state="{}"
    if declare -F get_provider_state_json >/dev/null 2>&1; then
        provider_state=$(get_provider_state_json 2>/dev/null || echo "{}")
    else
        local sid_file="${EUXIS_HOME}/euxis-data/runtime/provider-usage/${EUXIS_SESSION_ID:-${_MESH_SESSION_ID:-default}}.provider-state.json"
        [[ -f "${sid_file}" ]] && provider_state=$(cat "${sid_file}")
    fi

    flight_snapshot "checkpoint" "$(jq -n \
        --arg name "$name" \
        --argjson state "$state" \
        --argjson mesh "$mesh_state" \
        --argjson provider "$provider_state" \
        '{name: $name, state: $state, mesh_state: $mesh, provider_state: $provider}')"

    echo "[flight] Checkpoint saved: $name" >&2
}

# Record mission completion
# Usage: flight_complete "success|error" ["summary"]
flight_complete() {
    local status="$1"
    local summary="${2:-}"

    [[ -z "$_FLIGHT_MISSION_FILE" ]] && return 1

    local end_event
    end_event=$(jq -n \
        --arg type "mission_complete" \
        --arg status "$status" \
        --arg summary "$summary" \
        --arg timestamp "$(_mesh_ts_utc_ms)" \
        '{
            type: $type,
            status: $status,
            summary: $summary,
            timestamp: $timestamp
        }')

    echo "$end_event" >> "$_FLIGHT_MISSION_FILE"
    echo "[flight] Recording complete: $status" >&2
}

# Record cost event (for burn rate tracking)
# Usage: flight_cost "model" "input_tokens" "output_tokens" "cost_usd"
flight_cost() {
    local model="$1"
    local input_tokens="$2"
    local output_tokens="$3"
    local cost_usd="$4"

    flight_snapshot "cost" "$(jq -n \
        --arg model "$model" \
        --argjson input "$input_tokens" \
        --argjson output "$output_tokens" \
        --argjson cost "$cost_usd" \
        '{model: $model, input_tokens: $input, output_tokens: $output, cost_usd: $cost}')"
}

# ============================================================================
# Flight Recorder — Replay Functions
# ============================================================================

# List all recorded missions
# Usage: flight_list → outputs mission IDs
flight_list() {
    ls -1 "$FLIGHT_RECORDER_DIR"/*.jsonl 2>/dev/null | \
        xargs -I{} basename {} .jsonl | \
        sort -r
}

# Get mission metadata
# Usage: flight_info "mission-id" → JSON with start/end times, status, event count
flight_info() {
    local mission_id="$1"
    local file="${FLIGHT_RECORDER_DIR}/${mission_id}.jsonl"

    [[ ! -f "$file" ]] && echo '{"error": "not found"}' && return 1

    local start_event end_event event_count
    start_event=$(head -1 "$file")
    end_event=$(grep '"type":"mission_complete"' "$file" | tail -1)
    event_count=$(wc -l < "$file")

    jq -n \
        --argjson start "$start_event" \
        --argjson end "${end_event:-null}" \
        --argjson count "$event_count" \
        '{
            mission_id: $start.mission_id,
            description: $start.description,
            started: $start.timestamp,
            completed: (if $end then $end.timestamp else null end),
            status: (if $end then $end.status else "in_progress" end),
            event_count: $count
        }'
}

# Get events in time range
# Usage: flight_events "mission-id" [start_idx] [count] → JSONL events
flight_events() {
    local mission_id="$1"
    local start_idx="${2:-1}"
    local count="${3:-100}"
    local file="${FLIGHT_RECORDER_DIR}/${mission_id}.jsonl"

    [[ ! -f "$file" ]] && return 1

    tail -n +"$start_idx" "$file" | head -n "$count"
}

# Get checkpoint by name
# Usage: flight_get_checkpoint "mission-id" "checkpoint-name" → JSON state
flight_get_checkpoint() {
    local mission_id="$1"
    local checkpoint_name="$2"
    local file="${FLIGHT_RECORDER_DIR}/${mission_id}.jsonl"

    [[ ! -f "$file" ]] && return 1

    grep '"type":"checkpoint"' "$file" | \
        jq -s --arg name "$checkpoint_name" \
        '[.[] | select(.data.name == $name)] | last'
}

# Get latest checkpoint
# Usage: flight_latest_checkpoint "mission-id" → JSON state
flight_latest_checkpoint() {
    local mission_id="$1"
    local file="${FLIGHT_RECORDER_DIR}/${mission_id}.jsonl"

    [[ ! -f "$file" ]] && return 1

    grep '"type":"checkpoint"' "$file" | tail -1
}

# Calculate total mission cost
# Usage: flight_total_cost "mission-id" → cost in USD
flight_total_cost() {
    local mission_id="$1"
    local file="${FLIGHT_RECORDER_DIR}/${mission_id}.jsonl"

    [[ ! -f "$file" ]] && echo "0" && return 1

    grep '"type":"cost"' "$file" | \
        jq -s '[.[].data.cost_usd] | add // 0'
}
