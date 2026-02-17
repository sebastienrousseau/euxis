#!/usr/bin/env bats
# Test suite for core/lib/agents.sh

# Load dependencies first (agents.sh needs log_info, log_error, log_warn)
source "${BATS_TEST_DIRNAME}/../../../core/lib/common.sh"

# Test setup - run before each test
setup() {
    EUXIS_TEST_TMPDIR="$(mktemp -d)"
    export EUXIS_TEST_TMPDIR

    export EUXIS_HOME="${EUXIS_TEST_TMPDIR}/euxis"
    mkdir -p "${EUXIS_HOME}"

    # Force filesystem mode (skip SQLite in tests)
    export EUXIS_FORCE_FILESYSTEM=1

    # Mock external commands
    export PATH="${EUXIS_TEST_TMPDIR}:${PATH}"

    # Create mock prompts directories (actual agent path structure)
    mkdir -p "${EUXIS_HOME}/euxis-core/agents/prompts/core"
    mkdir -p "${EUXIS_HOME}/euxis-core/agents/prompts/fleet"

    # Create mock agent prompt files
    for agent in architect orchestrator; do
        cat > "${EUXIS_HOME}/euxis-core/agents/prompts/core/${agent}.txt" << EOF
---
agent_id: ${agent}
role: "Mock ${agent} agent"
version: 0.0.8
tags: core
last_updated: 2026-02-13
---
# MANDATE
Mock mandate for ${agent}

# OUTPUT FORMAT
Mock output format
EOF
    done

    for agent in tester debugger reviewer; do
        cat > "${EUXIS_HOME}/euxis-core/agents/prompts/fleet/${agent}.txt" << EOF
---
agent_id: ${agent}
role: "Mock ${agent} agent"
version: 0.0.8
tags: default
last_updated: 2026-02-13
---
# MANDATE
Mock mandate for ${agent}

# OUTPUT FORMAT
Mock output format
EOF
    done

    # Create partial file (should be excluded)
    echo "partial" > "${EUXIS_HOME}/euxis-core/agents/prompts/core/_protocol.txt"

    # Create lifecycle directories
    mkdir -p "${EUXIS_HOME}/euxis-runtime/data/lifecycle"

    # Create mock agents/registry.json (needed for SQL-fallback path validation)
    cat > "${EUXIS_HOME}/euxis-core/agents/registry.json" << 'EOF'
{
  "agents": [
    {"id": "architect", "tier": "core"},
    {"id": "orchestrator", "tier": "core"},
    {"id": "tester", "tier": "default"},
    {"id": "debugger", "tier": "default"},
    {"id": "reviewer", "tier": "default"}
  ]
}
EOF

    # Create empty core/lib so agents.sh can source registry_sql.sh
    mkdir -p "${EUXIS_HOME}/euxis-core/lib"

    # Mock date command for consistent timestamps
    cat > "${EUXIS_TEST_TMPDIR}/date" << 'DATEEOF'
#!/cli/bin/bash
if [[ "${1:-}" == "+%Y%m%d-%H%M%S" ]]; then
    echo "20260209-123456"
elif [[ "${1:-}" == "-u" && "${2:-}" == "+%Y-%m-%dT%H:%M:%SZ" ]]; then
    echo "2026-02-09T12:34:56Z"
else
    command date "$@"
fi
DATEEOF
    chmod +x "${EUXIS_TEST_TMPDIR}/date"

    # Now load the library under test (after EUXIS_HOME is set)
    # Reset include guard so it re-sources
    unset _EUXIS_LIB_AGENTS
    unset _EUXIS_LIB_COMMON
    source "${BATS_TEST_DIRNAME}/../../../core/lib/common.sh"
    source "${BATS_TEST_DIRNAME}/../../../core/lib/agents.sh"
}

teardown() {
    if [[ -n "${EUXIS_TEST_TMPDIR:-}" && -d "${EUXIS_TEST_TMPDIR:-}" ]]; then
        chmod -R u+w "${EUXIS_TEST_TMPDIR}" 2>/dev/null || true
        rm -rf "${EUXIS_TEST_TMPDIR}"
    fi
}

# ============================================================================
# AGENT PATH RESOLUTION
# ============================================================================

@test "resolve_agent_path finds valid core agent" {
    run resolve_agent_path "architect"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "${EUXIS_HOME}/euxis-core/agents/prompts/core/architect.txt" ]]
}

@test "resolve_agent_path finds valid fleet agent" {
    run resolve_agent_path "tester"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "${EUXIS_HOME}/euxis-core/agents/prompts/fleet/tester.txt" ]]
}

@test "resolve_agent_path returns error for invalid agent" {
    run resolve_agent_path "nonexistent"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" == "" ]]
}

@test "resolve_agent_path handles empty input" {
    run resolve_agent_path ""
    [[ "${status}" -eq 1 ]]
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

@test "list_agents excludes partial files" {
    run list_agents
    [[ "${status}" -eq 0 ]]
    [[ ! "${output}" =~ "_protocol" ]]
}

@test "list_agents handles empty prompts directory" {
    rm -f "${EUXIS_HOME}/euxis-core/agents/prompts/core"/*.txt
    rm -f "${EUXIS_HOME}/euxis-core/agents/prompts/fleet"/*.txt
    run list_agents
    # Should not crash, output may be empty
    [[ "${status}" -eq 0 ]]
}

# ============================================================================
# AGENT LIFECYCLE MANAGEMENT
# ============================================================================

@test "_lifecycle_init creates lifecycle directory" {
    rm -rf "${EUXIS_HOME}/euxis-runtime/data/lifecycle"
    _lifecycle_init
    [[ -d "${EUXIS_HOME}/euxis-runtime/data/lifecycle" ]]
}

@test "agent_lifecycle_transition records state" {
    agent_lifecycle_transition "tester" "active" "session-123"

    state_file="${EUXIS_HOME}/euxis-runtime/data/lifecycle/tester.state"
    [[ -f "${state_file}" ]]
    [[ "$(cat "${state_file}")" == "active" ]]
}

@test "agent_lifecycle_transition creates transition log" {
    agent_lifecycle_transition "tester" "active" "session-123"

    log_file="${EUXIS_HOME}/euxis-runtime/data/lifecycle/transitions.jsonl"
    [[ -f "${log_file}" ]]
    grep -q '"agent":"tester"' "${log_file}"
    grep -q '"state":"active"' "${log_file}"
}

@test "agent_get_state returns current state" {
    echo "active" > "${EUXIS_HOME}/euxis-runtime/data/lifecycle/tester.state"
    run agent_get_state "tester"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "active" ]]
}

@test "agent_get_state returns idle for missing state" {
    rm -f "${EUXIS_HOME}/euxis-runtime/data/lifecycle/tester.state"
    run agent_get_state "tester"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "idle" ]]
}

# ============================================================================
# ACTIVE AGENT MANAGEMENT
# ============================================================================

@test "list_active_agents shows agents with active state" {
    echo "active" > "${EUXIS_HOME}/euxis-runtime/data/lifecycle/architect.state"
    echo "active" > "${EUXIS_HOME}/euxis-runtime/data/lifecycle/tester.state"
    echo "idle" > "${EUXIS_HOME}/euxis-runtime/data/lifecycle/debugger.state"

    run list_active_agents
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "architect" ]]
    [[ "${output}" =~ "tester" ]]
    [[ ! "${output}" =~ "debugger" ]]
}

@test "count_active_agents returns correct count" {
    echo "active" > "${EUXIS_HOME}/euxis-runtime/data/lifecycle/architect.state"
    echo "active" > "${EUXIS_HOME}/euxis-runtime/data/lifecycle/tester.state"
    echo "idle" > "${EUXIS_HOME}/euxis-runtime/data/lifecycle/debugger.state"

    run count_active_agents
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "2" ]]
}

@test "count_active_agents returns 0 when no active agents" {
    # Remove any existing state files
    rm -f "${EUXIS_HOME}/euxis-runtime/data/lifecycle"/*.state

    run count_active_agents
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "0" ]]
}

@test "cleanup_stale_agents handles no stale agents" {
    # Create recent state file
    echo "active" > "${EUXIS_HOME}/euxis-runtime/data/lifecycle/architect.state"
    run cleanup_stale_agents
    [[ "${status}" -eq 0 ]]
    [[ -f "${EUXIS_HOME}/euxis-runtime/data/lifecycle/architect.state" ]]
}

# ============================================================================
# PLUGIN MANAGEMENT
# ============================================================================

@test "register_agent_plugin creates plugin from manifest file" {
    # Create manifest file (the actual API takes a file path)
    manifest_file="${EUXIS_TEST_TMPDIR}/plugin-manifest.json"
    # prompt_file must be within EUXIS_HOME (path traversal guard)
    prompt_file="${EUXIS_HOME}/euxis-core/agents/prompts/plugin-prompt.txt"
    echo "Plugin prompt content" > "${prompt_file}"

    cat > "${manifest_file}" << EOF
{
  "agent_id": "test-plugin",
  "role": "Test plugin agent",
  "prompt_file": "${prompt_file}",
  "tier": "standard"
}
EOF

    run register_agent_plugin "${manifest_file}"
    [[ "${status}" -eq 0 ]]
    # Plugin metadata should be saved
    [[ -f "${EUXIS_HOME}/euxis-core/config/plugins/test-plugin.json" ]]
    # Prompt should be symlinked into fleet
    [[ -L "${EUXIS_HOME}/euxis-core/agents/prompts/fleet/test-plugin.txt" ]]
}

@test "register_agent_plugin rejects missing manifest" {
    run register_agent_plugin "/nonexistent/manifest.json"
    [[ "${status}" -eq 1 ]]
}

@test "register_agent_plugin rejects manifest with missing agent_id" {
    manifest_file="${EUXIS_TEST_TMPDIR}/bad-manifest.json"
    cat > "${manifest_file}" << 'EOF'
{
  "role": "Bad plugin"
}
EOF
    run register_agent_plugin "${manifest_file}"
    [[ "${status}" -eq 1 ]]
}

@test "unregister_agent_plugin removes plugin files" {
    # Set up plugin first
    mkdir -p "${EUXIS_HOME}/euxis-core/config/plugins"
    echo '{"agent_id":"test-plugin"}' > "${EUXIS_HOME}/euxis-core/config/plugins/test-plugin.json"
    echo "prompt" > "${EUXIS_HOME}/euxis-core/agents/prompts/fleet/test-plugin.txt"

    run unregister_agent_plugin "test-plugin"
    [[ "${status}" -eq 0 ]]
    [[ ! -f "${EUXIS_HOME}/euxis-core/config/plugins/test-plugin.json" ]]
    [[ ! -f "${EUXIS_HOME}/euxis-core/agents/prompts/fleet/test-plugin.txt" ]]
}

@test "list_plugins shows registered plugins" {
    mkdir -p "${EUXIS_HOME}/euxis-core/config/plugins"
    echo '{}' > "${EUXIS_HOME}/euxis-core/config/plugins/plugin1.json"
    echo '{}' > "${EUXIS_HOME}/euxis-core/config/plugins/plugin2.json"

    run list_plugins
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "plugin1" ]]
    [[ "${output}" =~ "plugin2" ]]
}

@test "list_plugins handles no plugins" {
    rm -rf "${EUXIS_HOME}/euxis-core/config/plugins"
    run list_plugins
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "" ]]
}

# ============================================================================
# HEALTH PROBES
# ============================================================================

@test "agent_probe_liveness returns live for valid agent" {
    run agent_probe_liveness "tester"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "live" ]]
}

@test "agent_probe_liveness returns dead for non-existent agent" {
    run agent_probe_liveness "nonexistent"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" == "dead" ]]
}

@test "agent_health_report lists agents" {
    run agent_health_report
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "AGENT" ]]
    [[ "${output}" =~ "architect" ]]
    [[ "${output}" =~ "tester" ]]
}

# ============================================================================
# ERROR PATHS AND EDGE CASES
# ============================================================================

@test "agents functions handle set -u mode" {
    set -u
    run list_agents
    [[ "${status}" -eq 0 ]] || [[ "${status}" -eq 1 ]]
}

@test "agents functions handle pipefail mode" {
    set -e -o pipefail
    run list_agents
    [[ "${status}" -eq 0 ]]
}

@test "agents functions handle concurrent access" {
    agent_get_state "tester" &
    pid1=$!
    agent_get_state "tester" &
    pid2=$!
    wait $pid1 && wait $pid2
}

@test "agents functions handle large state files" {
    large_state="${EUXIS_HOME}/euxis-runtime/data/lifecycle/tester.state"
    printf "%*s" 10000 | tr ' ' 'a' > "${large_state}"
    echo "active" >> "${large_state}"
    run agent_get_state "tester"
    [[ "${status}" -eq 0 ]] || [[ "${status}" -eq 1 ]]
}

# ============================================================================
# INTEGRATION AND IDEMPOTENCY TESTS
# ============================================================================

@test "agent operations are idempotent" {
    result1=$(list_agents 2>&1)
    result2=$(list_agents 2>&1)
    [[ "${result1}" == "${result2}" ]]
}

@test "agent lifecycle maintains consistency" {
    agent_lifecycle_transition "tester" "active" "session-1"
    state1=$(agent_get_state "tester")
    [[ "${state1}" == "active" ]]

    agent_lifecycle_transition "tester" "idle" "session-2"
    state2=$(agent_get_state "tester")
    [[ "${state2}" == "idle" ]]
}
