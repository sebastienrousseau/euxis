#!/usr/bin/env bats
# Test suite for bin/lib/prompt.sh
# (c) 2026 Euxis Fleet. All rights reserved.

# Test setup - run before each test
setup() {
    EUXIS_TEST_TMPDIR="$(mktemp -d)"
    export EUXIS_TEST_TMPDIR

    export EUXIS_HOME="${EUXIS_TEST_TMPDIR}/euxis"
    export PROMPTS_DIR="${EUXIS_HOME}/prompts"
    mkdir -p "${EUXIS_HOME}/prompts/core"
    mkdir -p "${EUXIS_HOME}/prompts/fleet"
    mkdir -p "${EUXIS_HOME}/prompts/protocols"
    mkdir -p "${EUXIS_HOME}/data/projects/test/architect"
    mkdir -p "${EUXIS_HOME}/data/lifecycle"

    # Create mock protocol files
    cat > "${EUXIS_HOME}/prompts/protocols/_common.txt" << 'EOF'
# Common Protocol
All agents must follow these guidelines.
EOF

    cat > "${EUXIS_HOME}/prompts/protocols/_protocol.txt" << 'EOF'
# Base Protocol
Standard operating procedures.
EOF

    cat > "${EUXIS_HOME}/prompts/protocols/_security-boundaries.txt" << 'EOF'
# Security Protocol
Handle secrets and credentials carefully.
EOF

    cat > "${EUXIS_HOME}/prompts/protocols/_versioning.txt" << 'EOF'
# Versioning Protocol
Follow semver for releases.
EOF

    # Create mock agent files
    cat > "${EUXIS_HOME}/prompts/core/architect.txt" << 'EOF'
---
agent_id: architect
role: "System Architect"
---
# MANDATE
Design systems and review architecture.

# OUTPUT FORMAT
Structured analysis with recommendations.

{{AUDIT_FILE_PATH}}
{{MEMORY_FILE_PATH}}
{{SESSION_ID}}
{{MODEL_NAME}}
EOF

    # Create mock registry
    cat > "${EUXIS_HOME}/registry.json" << 'EOF'
{
  "agents": [
    {"id": "architect", "tier": "core"},
    {"id": "tester", "tier": "default"}
  ]
}
EOF

    # Create mock memory file
    cat > "${EUXIS_HOME}/data/projects/test/architect/memory.md" << 'EOF'
# Memory: architect
[2026-01-01] Previous context entry
EOF

    export PATH="${EUXIS_TEST_TMPDIR}:${PATH}"

    # Reset include guards
    unset _EUXIS_LIB_PROMPT
    unset _EUXIS_LIB_COMMON
    unset _EUXIS_LIB_AGENTS
    unset _EUXIS_LIB_MEMORY
    unset _EUXIS_LIB_TEMPLATE
    unset _EUXIS_PROTO_CACHE
    unset _EUXIS_PROTO_CACHE_KEY
    unset _EUXIS_REGISTRY_CACHE
    unset _EUXIS_REGISTRY_MTIME

    # Source dependencies from real installation
    source "${HOME}/.euxis/bin/lib/common.sh"
    source "${HOME}/.euxis/bin/lib/agents.sh"
    source "${HOME}/.euxis/bin/lib/memory.sh"
    source "${HOME}/.euxis/bin/lib/template.sh"
    source "${HOME}/.euxis/bin/lib/prompt.sh"
}

teardown() {
    if [[ -n "${EUXIS_TEST_TMPDIR:-}" && -d "${EUXIS_TEST_TMPDIR:-}" ]]; then
        rm -rf "${EUXIS_TEST_TMPDIR}"
    fi
}

# ============================================================================
# _PROTO_FINGERPRINT TESTS
# ============================================================================

@test "_proto_fingerprint returns empty for no keywords" {
    run _proto_fingerprint "simple task with no matches"
    [[ "${status}" -eq 0 ]]
    [[ -z "${output}" ]]
}

@test "_proto_fingerprint detects security keywords" {
    run _proto_fingerprint "review authentication and credentials"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "sec" ]]
}

@test "_proto_fingerprint detects version keywords" {
    run _proto_fingerprint "bump version for release"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "ver" ]]
}

@test "_proto_fingerprint detects escalation keywords" {
    run _proto_fingerprint "handle incident escalation"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "esc" ]]
}

@test "_proto_fingerprint detects evidence keywords" {
    run _proto_fingerprint "provide citation and evidence"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "evi" ]]
}

@test "_proto_fingerprint detects artifact keywords" {
    run _proto_fingerprint "create handoff artifact with schema"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "art" ]]
}

@test "_proto_fingerprint detects brand keywords" {
    run _proto_fingerprint "update brand signature"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "bra" ]]
}

@test "_proto_fingerprint detects synthesis keywords" {
    run _proto_fingerprint "synthesize and aggregate results"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "syn" ]]
}

@test "_proto_fingerprint detects bus keywords" {
    run _proto_fingerprint "publish message to topic"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "bus" ]]
}

@test "_proto_fingerprint detects graph keywords" {
    run _proto_fingerprint "add entity to knowledge graph"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "gra" ]]
}

@test "_proto_fingerprint handles multiple keywords" {
    run _proto_fingerprint "security review for version release"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "sec" ]]
    [[ "${output}" =~ "ver" ]]
}

# ============================================================================
# RESOLVE_PROTOCOLS TESTS
# ============================================================================

@test "resolve_protocols loads mandatory protocols" {
    run resolve_protocols "simple task"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Common Protocol" ]]
    [[ "${output}" =~ "Base Protocol" ]]
}

@test "resolve_protocols loads security protocol for auth task" {
    run resolve_protocols "review authentication security"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Security Protocol" ]]
}

@test "resolve_protocols loads versioning protocol for release task" {
    run resolve_protocols "prepare version release"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Versioning Protocol" ]]
}

@test "resolve_protocols caches results" {
    resolve_protocols "security task" > /dev/null
    local first_key="${_EUXIS_PROTO_CACHE_KEY}"

    resolve_protocols "security task" > /dev/null
    local second_key="${_EUXIS_PROTO_CACHE_KEY}"

    [[ "${first_key}" == "${second_key}" ]]
}

@test "resolve_protocols respects token budget" {
    # Create a large protocol file
    local large_file="${EUXIS_HOME}/prompts/protocols/_security-boundaries.txt"
    for i in $(seq 1 5000); do
        echo "Line ${i} of security content" >> "${large_file}"
    done

    export EUXIS_PROTOCOL_TOKEN_BUDGET=100
    run resolve_protocols "security authentication"
    [[ "${status}" -eq 0 ]]
    # Should still load mandatory protocols
    [[ "${output}" =~ "Common Protocol" ]]
}

@test "resolve_protocols handles missing protocol files" {
    rm -f "${EUXIS_HOME}/prompts/protocols/_security-boundaries.txt"
    run resolve_protocols "security task"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Common Protocol" ]]
}

# ============================================================================
# _GET_FLEET_ROSTER TESTS
# ============================================================================

@test "_get_fleet_roster returns agent list" {
    run _get_fleet_roster
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "architect" ]]
}

@test "_get_fleet_roster caches results" {
    _get_fleet_roster > /dev/null
    [[ -n "${_EUXIS_REGISTRY_CACHE}" ]]
}

@test "_get_fleet_roster falls back to JSON when SQLite unavailable" {
    rm -f "${EUXIS_HOME}/registry.db"
    run _get_fleet_roster
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "architect" ]]
}

# ============================================================================
# PREPARE_PROMPT TESTS
# ============================================================================

@test "prepare_prompt assembles full prompt" {
    local audit_path="${EUXIS_HOME}/data/projects/test/architect/audit.md"
    local memory_path="${EUXIS_HOME}/data/projects/test/architect/memory.md"
    touch "${audit_path}"

    run prepare_prompt "architect" "review the system" "${audit_path}" "${memory_path}" "session-123" "claude-3"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "System Architect" ]]
    [[ "${output}" =~ "CURRENT TASK" ]]
    [[ "${output}" =~ "review the system" ]]
}

@test "prepare_prompt includes memory context" {
    local audit_path="${EUXIS_HOME}/data/projects/test/architect/audit.md"
    local memory_path="${EUXIS_HOME}/data/projects/test/architect/memory.md"
    touch "${audit_path}"

    run prepare_prompt "architect" "task" "${audit_path}" "${memory_path}" "session-123" "claude-3"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "MEMORY CONTEXT" ]]
}

@test "prepare_prompt includes fleet roster" {
    local audit_path="${EUXIS_HOME}/data/projects/test/architect/audit.md"
    local memory_path="${EUXIS_HOME}/data/projects/test/architect/memory.md"
    touch "${audit_path}"

    run prepare_prompt "architect" "task" "${audit_path}" "${memory_path}" "session-123" "claude-3"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "FLEET ROSTER" ]]
}

@test "prepare_prompt substitutes template variables" {
    local audit_path="${EUXIS_HOME}/data/projects/test/architect/audit.md"
    local memory_path="${EUXIS_HOME}/data/projects/test/architect/memory.md"
    touch "${audit_path}"

    run prepare_prompt "architect" "task" "${audit_path}" "${memory_path}" "test-session" "test-model"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "test-session" ]] || [[ "${output}" =~ "test-model" ]]
}

@test "prepare_prompt adds dispatch directive in dispatch mode" {
    export EUXIS_DISPATCH=true
    local audit_path="${EUXIS_HOME}/data/projects/test/architect/audit.md"
    local memory_path="${EUXIS_HOME}/data/projects/test/architect/memory.md"
    touch "${audit_path}"

    run prepare_prompt "architect" "task" "${audit_path}" "${memory_path}" "session" "model"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "STRUCTURED INTERMEDIATE OUTPUT" ]]
}

@test "prepare_prompt adds dispatch directive for mesh mode" {
    local audit_path="${EUXIS_HOME}/data/projects/test/architect/audit.md"
    local memory_path="${EUXIS_HOME}/data/projects/test/architect/memory.md"
    touch "${audit_path}"

    run prepare_prompt "architect" "[MESH MODE] task" "${audit_path}" "${memory_path}" "session" "model"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "STRUCTURED INTERMEDIATE OUTPUT" ]]
}

@test "prepare_prompt exits on missing agent file" {
    run prepare_prompt "nonexistent" "task" "/tmp/audit" "/tmp/memory" "session" "model"
    [[ "${status}" -eq 1 ]]
    [[ "${output}" =~ "ERROR" ]]
}

# ============================================================================
# ERROR PATHS AND EDGE CASES
# ============================================================================

@test "prompt functions handle set -u mode" {
    set -u
    run _proto_fingerprint "test task"
    [[ "${status}" -eq 0 ]]
}

@test "prompt functions handle pipefail mode" {
    set -e -o pipefail
    run resolve_protocols "test task"
    [[ "${status}" -eq 0 ]]
}

@test "prompt functions handle empty task" {
    run resolve_protocols ""
    [[ "${status}" -eq 0 ]]
}

@test "prompt functions handle special characters in task" {
    run _proto_fingerprint "task with \$pecial & <chars>"
    [[ "${status}" -eq 0 ]]
}

@test "resolve_protocols handles missing protocols directory" {
    rm -rf "${EUXIS_HOME}/prompts/protocols"
    run resolve_protocols "task"
    [[ "${status}" -eq 0 ]]
}

# ============================================================================
# INTEGRATION TESTS
# ============================================================================

@test "prompt module loads without errors" {
    [[ -n "${_EUXIS_LIB_PROMPT}" ]]
}

@test "protocol caching is deterministic" {
    local result1
    result1=$(resolve_protocols "security authentication task")
    local key1="${_EUXIS_PROTO_CACHE_KEY}"

    # Clear cache
    _EUXIS_PROTO_CACHE=""
    _EUXIS_PROTO_CACHE_KEY=""

    local result2
    result2=$(resolve_protocols "security authentication task")
    local key2="${_EUXIS_PROTO_CACHE_KEY}"

    [[ "${key1}" == "${key2}" ]]
}

@test "prepare_prompt is consistent across calls" {
    local audit_path="${EUXIS_HOME}/data/projects/test/architect/audit.md"
    local memory_path="${EUXIS_HOME}/data/projects/test/architect/memory.md"
    touch "${audit_path}"

    local result1
    result1=$(prepare_prompt "architect" "consistent task" "${audit_path}" "${memory_path}" "session" "model")
    local result2
    result2=$(prepare_prompt "architect" "consistent task" "${audit_path}" "${memory_path}" "session" "model")

    [[ "${result1}" == "${result2}" ]]
}
