#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors
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
#   source "${EUXIS_HOME}/euxis-core/lib/mesh.sh"
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
MESH_DIR="${EUXIS_HOME}/euxis-runtime/mesh"
MESH_STATE="${MESH_DIR}/state.json"
MESH_INBOX="${MESH_DIR}/inbox"
MESH_REGISTRY="${EUXIS_HOME}/euxis-core/agents/registry.json"

# Timeouts
MESH_SEND_TIMEOUT="${MESH_SEND_TIMEOUT:-30}"      # Seconds to wait for delivery
MESH_RECEIVE_TIMEOUT="${MESH_RECEIVE_TIMEOUT:-5}" # Seconds to poll for messages
MESH_DEADLOCK_TIMEOUT="${MESH_DEADLOCK_TIMEOUT:-60}" # Seconds before deadlock detection

# Current agent identity (set via mesh_init)
_MESH_AGENT_ID=""
_MESH_SESSION_ID=""

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

# Set value in shared state (atomic)
# Usage: mesh_state_set "path.to.key" "value"
# Note: Uses setpath() to handle keys with special chars (hyphens, etc.)
mesh_state_set() {
    local path="$1"
    local value="$2"

    if [[ ! -f "$MESH_STATE" ]]; then
        mesh_state_init
    fi

    local tmp="${MESH_STATE}.tmp.$$"

    # Convert dot notation to array for setpath
    local path_array
    path_array=$(echo "$path" | sed 's/\./","/g' | sed 's/^/["/' | sed 's/$/"]/')

    # Atomic write: write to temp, then rename
    if jq --arg val "$value" "setpath($path_array; \$val)" "$MESH_STATE" > "$tmp" 2>/dev/null; then
        mv "$tmp" "$MESH_STATE"
    else
        rm -f "$tmp" 2>/dev/null
        return 1
    fi
}

# Set JSON object in shared state
# Usage: mesh_state_set_json "path.to.key" '{"foo": "bar"}'
mesh_state_set_json() {
    local path="$1"
    local json="$2"

    local tmp="${MESH_STATE}.tmp.$$"

    # Convert dot notation to array for setpath
    local path_array
    path_array=$(echo "$path" | sed 's/\./","/g' | sed 's/^/["/' | sed 's/$/"]/')

    if jq --argjson val "$json" "setpath($path_array; \$val)" "$MESH_STATE" > "$tmp" 2>/dev/null; then
        mv "$tmp" "$MESH_STATE"
    else
        rm -f "$tmp" 2>/dev/null
        return 1
    fi
}

# Write to shared research log (append-only)
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

    local tmp="${MESH_STATE}.tmp.$$"
    jq --argjson entry "$entry" '.shared.log = (.shared.log // []) + [$entry]' "$MESH_STATE" > "$tmp" && mv "$tmp" "$MESH_STATE"
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
    timestamp=$(date +%s%N)
    local msg_file="$target_inbox/${timestamp}-${_MESH_AGENT_ID}.msg"

    # Create message envelope
    cat > "$msg_file" <<EOF
{
  "from": "$_MESH_AGENT_ID",
  "to": "$target",
  "timestamp": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
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

    # Remove offline agents from state
    local tmp="${MESH_STATE}.tmp.$$"
    jq '.agents |= with_entries(select(.value.status != "offline"))' "$MESH_STATE" > "$tmp" && mv "$tmp" "$MESH_STATE"
}
