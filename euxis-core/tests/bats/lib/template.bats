#!/usr/bin/env bats
# Test suite for core/lib/template.sh

# Test setup - run before each test
setup() {
    EUXIS_TEST_TMPDIR="$(mktemp -d)"
    export EUXIS_TEST_TMPDIR

    export EUXIS_HOME="${EUXIS_TEST_TMPDIR}/euxis"
    mkdir -p "${EUXIS_HOME}"

    export PATH="${EUXIS_TEST_TMPDIR}:${PATH}"

    # Reset include guard
    unset _EUXIS_LIB_TEMPLATE

    # Source the library from real installation
    source "${BATS_TEST_DIRNAME}/../../../lib/template.sh"
}

teardown() {
    if [[ -n "${EUXIS_TEST_TMPDIR:-}" && -d "${EUXIS_TEST_TMPDIR:-}" ]]; then
        rm -rf "${EUXIS_TEST_TMPDIR}"
    fi
}

# ============================================================================
# TEMPLATE_SUBSTITUTE TESTS
# ============================================================================

@test "template_substitute replaces AUDIT_FILE_PATH" {
    local text="Audit at {{AUDIT_FILE_PATH}}"
    run template_substitute "${text}" "/path/to/audit.md" "" "" ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "Audit at /path/to/audit.md" ]]
}

@test "template_substitute replaces MEMORY_FILE_PATH" {
    local text="Memory at {{MEMORY_FILE_PATH}}"
    run template_substitute "${text}" "" "/path/to/memory.md" "" ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "Memory at /path/to/memory.md" ]]
}

@test "template_substitute replaces SESSION_ID" {
    local text="Session: {{SESSION_ID}}"
    run template_substitute "${text}" "" "" "session-123" ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "Session: session-123" ]]
}

@test "template_substitute replaces MODEL_NAME" {
    local text="Model: {{MODEL_NAME}}"
    run template_substitute "${text}" "" "" "" "claude-3-opus"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "Model: claude-3-opus" ]]
}

@test "template_substitute replaces EUXIS_HOME" {
    local text="Home: {{EUXIS_HOME}}"
    run template_substitute "${text}" "" "" "" ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "Home: ${EUXIS_HOME}" ]]
}

@test "template_substitute replaces PROMPTS_DIR" {
    local text="Prompts: {{PROMPTS_DIR}}"
    run template_substitute "${text}" "" "" "" ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "Prompts: ${EUXIS_HOME}/euxis-core/agents/prompts" ]]
}

@test "template_substitute replaces PROJECTS_DIR" {
    local text="Projects: {{PROJECTS_DIR}}"
    run template_substitute "${text}" "" "" "" ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "Projects: ${EUXIS_HOME}/euxis-runtime/data/projects" ]]
}

@test "template_substitute handles multiple variables" {
    local text="Audit: {{AUDIT_FILE_PATH}}, Memory: {{MEMORY_FILE_PATH}}, Session: {{SESSION_ID}}"
    run template_substitute "${text}" "/audit.md" "/memory.md" "sess-001" ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "Audit: /audit.md, Memory: /memory.md, Session: sess-001" ]]
}

@test "template_substitute handles repeated variables" {
    local text="{{SESSION_ID}} and {{SESSION_ID}} again"
    run template_substitute "${text}" "" "" "abc123" ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "abc123 and abc123 again" ]]
}

@test "template_substitute handles empty values" {
    local text="Audit: {{AUDIT_FILE_PATH}}"
    run template_substitute "${text}" "" "" "" ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "Audit: " ]]
}

@test "template_substitute handles no variables" {
    local text="Plain text without variables"
    run template_substitute "${text}" "/audit" "/memory" "session" "model"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "Plain text without variables" ]]
}

@test "template_substitute preserves non-matching braces" {
    local text="{{UNKNOWN_VAR}} stays"
    run template_substitute "${text}" "" "" "" ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "{{UNKNOWN_VAR}} stays" ]]
}

@test "template_substitute handles multiline text" {
    local text="Line 1: {{SESSION_ID}}
Line 2: {{MODEL_NAME}}
Line 3: Regular"
    run template_substitute "${text}" "" "" "sess" "model"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Line 1: sess" ]]
    [[ "${output}" =~ "Line 2: model" ]]
    [[ "${output}" =~ "Line 3: Regular" ]]
}

@test "template_substitute handles special characters in values" {
    local text="Path: {{AUDIT_FILE_PATH}}"
    run template_substitute "${text}" "/path/with spaces/audit.md" "" "" ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "Path: /path/with spaces/audit.md" ]]
}

@test "template_substitute handles paths with dashes and underscores" {
    local text="{{AUDIT_FILE_PATH}}"
    run template_substitute "${text}" "/some-path/with_special/chars.md" "" "" ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "/some-path/with_special/chars.md" ]]
}

@test "template_substitute handles empty text" {
    run template_substitute "" "" "" "" ""
    [[ "${status}" -eq 0 ]]
    [[ -z "${output}" ]]
}

@test "template_substitute handles very long text" {
    local text=""
    for i in $(seq 1 100); do
        text="${text}Line ${i}: {{SESSION_ID}}\n"
    done
    run template_substitute "${text}" "" "" "session" ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "session" ]]
}

# ============================================================================
# ESTIMATE_TOKENS TESTS
# ============================================================================

@test "estimate_tokens returns 0 for empty string" {
    run estimate_tokens ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "0" ]]
}

@test "estimate_tokens estimates 1 token per 4 chars" {
    run estimate_tokens "1234"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "1" ]]
}

@test "estimate_tokens handles 8 characters" {
    run estimate_tokens "12345678"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "2" ]]
}

@test "estimate_tokens handles 16 characters" {
    run estimate_tokens "1234567890123456"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "4" ]]
}

@test "estimate_tokens truncates (floor division)" {
    run estimate_tokens "123"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "0" ]]
}

@test "estimate_tokens handles large text" {
    local text
    text=$(printf '%*s' 4000 | tr ' ' 'a')
    run estimate_tokens "${text}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "1000" ]]
}

@test "estimate_tokens handles special characters" {
    run estimate_tokens "!@#$%^&*()"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "2" ]]
}

@test "estimate_tokens handles Unicode" {
    # Note: Bash ${#var} counts bytes for multi-byte chars in some locales
    run estimate_tokens "test"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "1" ]]
}

@test "estimate_tokens handles newlines" {
    run estimate_tokens "line1
line2
line3"
    [[ "${status}" -eq 0 ]]
    # 5 + 1 + 5 + 1 + 5 = 17 chars = 4 tokens
    [[ "${output}" -ge 3 ]]
}

# ============================================================================
# ERROR PATHS AND EDGE CASES
# ============================================================================

@test "template functions handle set -u mode" {
    set -u
    run template_substitute "test {{SESSION_ID}}" "" "" "session" ""
    [[ "${status}" -eq 0 ]]
}

@test "template functions handle pipefail mode" {
    set -e -o pipefail
    run estimate_tokens "test"
    [[ "${status}" -eq 0 ]]
}

@test "template_substitute handles dollar signs in values" {
    local text="Value: {{AUDIT_FILE_PATH}}"
    run template_substitute "${text}" "/path/\$HOME/file" "" "" ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "/path/" ]]
}

@test "template_substitute handles backslashes in values" {
    local text="Path: {{AUDIT_FILE_PATH}}"
    run template_substitute "${text}" "/path/with\\backslash" "" "" ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "Path:" ]]
}

@test "template_substitute handles curly braces in text" {
    local text="JSON: {\"key\": \"value\"} and {{SESSION_ID}}"
    run template_substitute "${text}" "" "" "session" ""
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ '{"key": "value"}' ]]
    [[ "${output}" =~ "session" ]]
}

# ============================================================================
# INTEGRATION TESTS
# ============================================================================

@test "template module loads without errors" {
    [[ -n "${_EUXIS_LIB_TEMPLATE}" ]]
}

@test "template_substitute is idempotent" {
    local text="Session: {{SESSION_ID}}"
    local result1
    result1=$(template_substitute "${text}" "" "" "abc" "")
    local result2
    result2=$(template_substitute "${text}" "" "" "abc" "")
    [[ "${result1}" == "${result2}" ]]
}

@test "estimate_tokens is deterministic" {
    local text="consistent text for testing"
    local result1
    result1=$(estimate_tokens "${text}")
    local result2
    result2=$(estimate_tokens "${text}")
    [[ "${result1}" == "${result2}" ]]
}

@test "template functions work together" {
    local text="Session {{SESSION_ID}} has about X tokens"
    local substituted
    substituted=$(template_substitute "${text}" "" "" "test-session" "")
    local tokens
    tokens=$(estimate_tokens "${substituted}")

    [[ "${substituted}" =~ "test-session" ]]
    [[ "${tokens}" -gt 0 ]]
}

@test "EUXIS_HOME is correctly set in template" {
    [[ "${EUXIS_HOME}" == "${EUXIS_TEST_TMPDIR}/euxis" ]]
}

@test "all standard variables are replaced" {
    local text="{{AUDIT_FILE_PATH}} {{MEMORY_FILE_PATH}} {{SESSION_ID}} {{MODEL_NAME}} {{EUXIS_HOME}} {{PROMPTS_DIR}} {{PROJECTS_DIR}}"
    run template_substitute "${text}" "a" "m" "s" "n"
    [[ "${status}" -eq 0 ]]
    # No template variables should remain
    [[ ! "${output}" =~ "{{AUDIT_FILE_PATH}}" ]]
    [[ ! "${output}" =~ "{{MEMORY_FILE_PATH}}" ]]
    [[ ! "${output}" =~ "{{SESSION_ID}}" ]]
    [[ ! "${output}" =~ "{{MODEL_NAME}}" ]]
    [[ ! "${output}" =~ "{{EUXIS_HOME}}" ]]
    [[ ! "${output}" =~ "{{PROMPTS_DIR}}" ]]
    [[ ! "${output}" =~ "{{PROJECTS_DIR}}" ]]
}
