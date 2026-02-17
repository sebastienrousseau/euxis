#!/usr/bin/env bash
# Tests for lib/template.sh (pure-bash substitution)

source "${EUXIS_HOME}/core/lib/template.sh"

# Test template_substitute: basic variable replacement
result=$(template_substitute "Hello {{SESSION_ID}} world" "" "" "test-123" "")
assert_contains "template replaces SESSION_ID" "test-123" "${result}"

# Test template_substitute: multiple variables
result=$(template_substitute "Audit: {{AUDIT_FILE_PATH}} Memory: {{MEMORY_FILE_PATH}}" "/tmp/a.md" "/tmp/m.md" "" "")
assert_contains "template replaces AUDIT_FILE_PATH" "/tmp/a.md" "${result}"
assert_contains "template replaces MEMORY_FILE_PATH" "/tmp/m.md" "${result}"

# Test template_substitute: MODEL_NAME
result=$(template_substitute "Model is {{MODEL_NAME}}" "" "" "" "claude-sonnet-4")
assert_contains "template replaces MODEL_NAME" "claude-sonnet-4" "${result}"

# Test template_substitute: EUXIS_HOME
result=$(template_substitute "Home: {{EUXIS_HOME}}" "" "" "" "")
assert_contains "template replaces EUXIS_HOME" "${EUXIS_HOME}" "${result}"

# Test template_substitute: PROMPTS_DIR
result=$(template_substitute "Prompts: {{PROMPTS_DIR}}" "" "" "" "")
assert_contains "template replaces PROMPTS_DIR" "${EUXIS_HOME}/prompts" "${result}"

# Test template_substitute: PROJECTS_DIR
result=$(template_substitute "Projects: {{PROJECTS_DIR}}" "" "" "" "")
assert_contains "template replaces PROJECTS_DIR" "${EUXIS_HOME}/data/projects" "${result}"

# Test template_substitute: no variables (passthrough)
result=$(template_substitute "no variables here" "" "" "" "")
assert_eq "passthrough without variables" "no variables here" "${result}"

# Test template_substitute: all variables at once
result=$(template_substitute "{{AUDIT_FILE_PATH}}|{{MEMORY_FILE_PATH}}|{{SESSION_ID}}|{{MODEL_NAME}}|{{EUXIS_HOME}}" "/a" "/m" "s1" "mod")
assert_eq "all variables replaced" "/a|/m|s1|mod|${EUXIS_HOME}" "${result}"

# Test template_substitute: no sed fork (verify pure bash)
# If sed is not in the template.sh, substitution is bash-native
assert_eq "no sed in template.sh" "0" "$(grep -c '| sed' "${EUXIS_HOME}/core/lib/template.sh")"

# Test estimate_tokens: rough estimate
tokens=$(estimate_tokens "hello world this is a test string")
# 33 chars / 4 = 8
assert_eq "estimate_tokens rough count" "8" "${tokens}"

# Test estimate_tokens: empty string
tokens=$(estimate_tokens "")
assert_eq "estimate_tokens empty" "0" "${tokens}"
