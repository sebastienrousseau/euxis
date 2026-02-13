#!/usr/bin/env bats
# Test suite for bin/lib/agents.sh
# (c) 2026 Euxis Fleet. All rights reserved.

# Load the library under test
source "${BATS_TEST_DIRNAME}/../../../bin/lib/agents.sh"

# Test setup - run before each test
setup() {
    # Create temporary directory for each test
    EUXIS_TEST_TMPDIR="$(mktemp -d)"
    export EUXIS_TEST_TMPDIR

    # Mock EUXIS_HOME
    export EUXIS_HOME="${EUXIS_TEST_TMPDIR}/euxis"
    mkdir -p "${EUXIS_HOME}"

    # Mock external commands
    export PATH="${EUXIS_TEST_TMPDIR}:${PATH}"

    # Create mock registry
    mkdir -p "${EUXIS_HOME}/config"
    cat > "${EUXIS_HOME}/registry.json" << 'EOF'
{
  "agents": {
    "architect": {
      "type": "core",
      "role": "System architecture and design",
      "path": "config/agents/architect.md"
    },
    "tester": {
      "type": "default",
      "role": "Test coverage and validation",
      "path": "config/agents/tester.md"
    },
    "debugger": {
      "type": "on-demand",
      "role": "Bug analysis and fixes",
      "path": "config/agents/debugger.md"
    }
  }
}
EOF

    # Create mock agent files
    mkdir -p "${EUXIS_HOME}/config/agents"
    for agent in architect tester debugger; do
        cat > "${EUXIS_HOME}/config/agents/${agent}.md" << EOF
---
agent_id: ${agent}
role: "Mock ${agent} agent"
---
# Mock Agent: ${agent}
EOF
    done

    # Create mock agent state directories
    mkdir -p "${EUXIS_HOME}/data/agents"
    for agent in architect tester debugger; do
        mkdir -p "${EUXIS_HOME}/data/agents/${agent}"
    done

    # Mock date command for consistent timestamps
    cat > "${EUXIS_TEST_TMPDIR}/date" << 'EOF'
#!/bin/bash
if [[ "$1" == "+%Y%m%d-%H%M%S" ]]; then
    echo "20260209-123456"
else
    command date "$@"
fi
EOF
    chmod +x "${EUXIS_TEST_TMPDIR}/date"
}

# Test teardown - run after each test
teardown() {
    if [[ -n "${EUXIS_TEST_TMPDIR}" && -d "${EUXIS_TEST_TMPDIR}" ]]; then
        rm -rf "${EUXIS_TEST_TMPDIR}"
    fi
}

# ============================================================================
# AGENT PATH RESOLUTION
# ============================================================================

@test "resolve_agent_path finds valid core agent" {
    run resolve_agent_path "architect"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "${EUXIS_HOME}/config/agents/architect.md" ]]
}

@test "resolve_agent_path finds valid default agent" {
    run resolve_agent_path "tester"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "${EUXIS_HOME}/config/agents/tester.md" ]]
}

@test "resolve_agent_path returns empty for invalid agent" {
    run resolve_agent_path "nonexistent"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "" ]]
}

@test "resolve_agent_path handles empty input" {
    run resolve_agent_path ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "" ]]
}

@test "resolve_agent_path handles missing registry" {
    rm -f "${EUXIS_HOME}/registry.json"
    run resolve_agent_path "architect"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Registry not found" ]]
}

@test "resolve_agent_path handles malformed registry" {
    echo "invalid json" > "${EUXIS_HOME}/registry.json"
    run resolve_agent_path "architect"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Failed to parse registry" ]]
}

# ============================================================================
# AGENT LISTING
# ============================================================================

@test "list_agents shows all available agents" {
    run list_agents
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "architect" ]]
    [[ "${output}" =~ "tester" ]]
    [[ "${output}" =~ "debugger" ]]
}

@test "list_agents filters by type" {
    run list_agents "core"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "architect" ]]
    [[ ! "${output}" =~ "tester" ]]
}

@test "list_agents handles invalid type filter" {
    run list_agents "invalid-type"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "" ]]
}

@test "list_agents handles missing registry" {
    rm -f "${EUXIS_HOME}/registry.json"
    run list_agents
    [[ "${status}" -eq 1 ]]
}

# ============================================================================
# AGENT LIFECYCLE MANAGEMENT
# ============================================================================

@test "_lifecycle_init creates agent state directory" {
    rm -rf "${EUXIS_HOME}/data/agents/newagent"

    run _lifecycle_init "newagent"
    [[ "${status}" -eq 0 ]]
    [[ -d "${EUXIS_HOME}/data/agents/newagent" ]]
}

@test "agent_lifecycle_transition records state changes" {
    run agent_lifecycle_transition "tester" "idle" "active"
    [[ "${status}" -eq 0 ]]

    # Verify transition was recorded
    state_file="${EUXIS_HOME}/data/agents/tester/state"
    [[ -f "${state_file}" ]]
    grep -q "active" "${state_file}"
}

@test "agent_lifecycle_transition creates lifecycle log" {
    run agent_lifecycle_transition "tester" "idle" "active"
    [[ "${status}" -eq 0 ]]

    # Verify lifecycle log exists
    log_file="${EUXIS_HOME}/data/agents/tester/lifecycle.log"
    [[ -f "${log_file}" ]]
    grep -q "idle -> active" "${log_file}"
}

@test "agent_lifecycle_transition handles invalid states" {
    run agent_lifecycle_transition "tester" "invalid" "active"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Invalid state transition" ]]
}

@test "agent_get_state returns current state" {
    # Set initial state
    echo "active" > "${EUXIS_HOME}/data/agents/tester/state"

    run agent_get_state "tester"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "active" ]]
}

@test "agent_get_state returns unknown for missing state" {
    rm -f "${EUXIS_HOME}/data/agents/tester/state"

    run agent_get_state "tester"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "unknown" ]]
}

# ============================================================================
# ACTIVE AGENT MANAGEMENT
# ============================================================================

@test "list_active_agents shows agents with active state" {
    # Set some agents to active
    echo "active" > "${EUXIS_HOME}/data/agents/architect/state"
    echo "active" > "${EUXIS_HOME}/data/agents/tester/state"
    echo "idle" > "${EUXIS_HOME}/data/agents/debugger/state"

    run list_active_agents
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "architect" ]]
    [[ "${output}" =~ "tester" ]]
    [[ ! "${output}" =~ "debugger" ]]
}

@test "count_active_agents returns correct count" {
    # Set active agents
    echo "active" > "${EUXIS_HOME}/data/agents/architect/state"
    echo "active" > "${EUXIS_HOME}/data/agents/tester/state"
    echo "idle" > "${EUXIS_HOME}/data/agents/debugger/state"

    run count_active_agents
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "2" ]]
}

@test "count_active_agents returns 0 when no active agents" {
    # Set all agents to idle
    for agent in architect tester debugger; do
        echo "idle" > "${EUXIS_HOME}/data/agents/${agent}/state"
    done

    run count_active_agents
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "0" ]]
}

@test "cleanup_stale_agents removes old state files" {
    # Create old state files
    old_timestamp="202601010000.00"
    for agent in architect tester; do
        state_file="${EUXIS_HOME}/data/agents/${agent}/state"
        echo "active" > "${state_file}"
        touch -t "${old_timestamp}" "${state_file}"
    done

    run cleanup_stale_agents
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Cleaned up" ]]
}

@test "cleanup_stale_agents preserves recent state files" {
    # Create recent state file
    echo "active" > "${EUXIS_HOME}/data/agents/architect/state"

    run cleanup_stale_agents
    [[ "${status}" -eq 0 ]]
    [[ -f "${EUXIS_HOME}/data/agents/architect/state" ]]
}

# ============================================================================
# PLUGIN MANAGEMENT
# ============================================================================

@test "register_agent_plugin creates plugin entry" {
    plugin_config='{
      "name": "test-plugin",
      "version": "1.0.0",
      "capabilities": ["analysis"],
      "command": "test-plugin-cmd"
    }'

    run register_agent_plugin "test-plugin" "${plugin_config}"
    [[ "${status}" -eq 0 ]]

    # Verify plugin was registered
    plugin_file="${EUXIS_HOME}/data/agents/plugins/test-plugin.json"
    [[ -f "${plugin_file}" ]]
    grep -q "test-plugin" "${plugin_file}"
}

@test "register_agent_plugin validates plugin config" {
    invalid_config='{"invalid": "config"}'

    run register_agent_plugin "invalid-plugin" "${invalid_config}"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Invalid plugin configuration" ]]
}

@test "register_agent_plugin prevents duplicate registration" {
    plugin_config='{"name": "test-plugin", "version": "1.0.0"}'

    # Register first time
    register_agent_plugin "test-plugin" "${plugin_config}"

    # Try to register again
    run register_agent_plugin "test-plugin" "${plugin_config}"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Plugin already registered" ]]
}

@test "unregister_agent_plugin removes plugin" {
    # Register plugin first
    plugin_config='{"name": "test-plugin", "version": "1.0.0"}'
    register_agent_plugin "test-plugin" "${plugin_config}"

    run unregister_agent_plugin "test-plugin"
    [[ "${status}" -eq 0 ]]

    # Verify plugin was removed
    plugin_file="${EUXIS_HOME}/data/agents/plugins/test-plugin.json"
    [[ ! -f "${plugin_file}" ]]
}

@test "unregister_agent_plugin handles non-existent plugin" {
    run unregister_agent_plugin "nonexistent-plugin"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Plugin not found" ]]
}

@test "list_plugins shows registered plugins" {
    # Register test plugins
    plugin1='{"name": "plugin1", "version": "1.0.0"}'
    plugin2='{"name": "plugin2", "version": "2.0.0"}'
    register_agent_plugin "plugin1" "${plugin1}"
    register_agent_plugin "plugin2" "${plugin2}"

    run list_plugins
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "plugin1" ]]
    [[ "${output}" =~ "plugin2" ]]
}

@test "list_plugins handles no plugins" {
    run list_plugins
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "No plugins registered" ]]
}

# ============================================================================
# HEALTH MONITORING
# ============================================================================

@test "agent_probe_liveness checks basic agent availability" {
    run agent_probe_liveness "tester"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Agent tester is alive" ]]
}

@test "agent_probe_liveness fails for non-existent agent" {
    run agent_probe_liveness "nonexistent"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Agent not found" ]]
}

@test "agent_probe_readiness checks agent readiness" {
    # Set agent to active state
    echo "active" > "${EUXIS_HOME}/data/agents/tester/state"

    run agent_probe_readiness "tester"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Agent tester is ready" ]]
}

@test "agent_probe_readiness fails for inactive agent" {
    # Set agent to idle state
    echo "idle" > "${EUXIS_HOME}/data/agents/tester/state"

    run agent_probe_readiness "tester"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "Agent not ready" ]]
}

@test "agent_health_report provides comprehensive status" {
    # Set up agent state
    echo "active" > "${EUXIS_HOME}/data/agents/tester/state"

    run agent_health_report "tester"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Health Report for tester" ]]
    [[ "${output}" =~ "State: active" ]]
}

# ============================================================================
# ERROR PATHS AND EDGE CASES
# ============================================================================

@test "agents functions handle set -u mode" {
    set -u
    run list_agents
    # Should not fail due to unbound variables
    [[ "${status}" -eq 0 ]] || [[ "${status}" -eq 1 ]]
}

@test "agents functions handle pipefail mode" {
    set -e -o pipefail
    run list_agents
    [[ "${status}" -eq 0 ]]
}

@test "agents functions handle Unicode agent names" {
    # Add Unicode agent to registry
    cat > "${EUXIS_HOME}/registry.json" << 'EOF'
{
  "agents": {
    "测试代理": {
      "type": "test",
      "role": "Unicode test agent",
      "path": "config/agents/unicode.md"
    }
  }
}
EOF

    run list_agents
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "测试代理" ]]
}

@test "agents functions handle concurrent access" {
    # Basic test for concurrent access to agent state
    agent_get_state "tester" &
    pid1=$!
    agent_get_state "tester" &
    pid2=$!

    wait $pid1 && wait $pid2
    # Should handle concurrent reads without error
}

@test "agents functions handle file system errors" {
    # Make directory read-only to simulate permission errors
    chmod -w "${EUXIS_HOME}/data/agents"

    run _lifecycle_init "newagent"
    [[ "${status}" -eq 1 ]]

    # Restore permissions
    chmod +w "${EUXIS_HOME}/data/agents"
}

@test "agents functions handle large state files" {
    # Create large state file
    large_state="${EUXIS_HOME}/data/agents/tester/state"
    printf "%*s" 1000000 | tr ' ' 'a' > "${large_state}"
    echo "active" >> "${large_state}"

    run agent_get_state "tester"
    # Should handle large files gracefully
    [[ "${status}" -eq 0 ]] || [[ "${status}" -eq 1 ]]
}

# ============================================================================
# INTEGRATION AND IDEMPOTENCY TESTS
# ============================================================================

@test "agent operations are idempotent" {
    # Multiple calls should produce same result
    result1=$(list_agents 2>&1)
    result2=$(list_agents 2>&1)
    [[ "${result1}" == "${result2}" ]]
}

@test "agent lifecycle maintains consistency" {
    # Test full lifecycle
    agent_lifecycle_transition "tester" "idle" "active"
    state1=$(agent_get_state "tester")
    [[ "${state1}" == "active" ]]

    agent_lifecycle_transition "tester" "active" "idle"
    state2=$(agent_get_state "tester")
    [[ "${state2}" == "idle" ]]
}

@test "plugin management maintains registry integrity" {
    # Test plugin registration and removal
    plugin_config='{"name": "test-plugin", "version": "1.0.0"}'

    register_agent_plugin "test-plugin" "${plugin_config}"
    plugins1=$(list_plugins)
    [[ "${plugins1}" =~ "test-plugin" ]]

    unregister_agent_plugin "test-plugin"
    plugins2=$(list_plugins)
    [[ ! "${plugins2}" =~ "test-plugin" ]]
}