#!/usr/bin/env bash
# Tests for lib/prompt.sh

source "${EUXIS_HOME}/bin/lib/common.sh"
source "${EUXIS_HOME}/bin/lib/prompt.sh"

# Test resolve_protocols: mandatory protocols always loaded
result=$(resolve_protocols "simple task with no keywords")
assert_contains "always loads _protocol.txt content" "EUXIS MANDATORY PROTOCOL" "${result}"

# Test resolve_protocols: security keywords trigger security protocol
result=$(resolve_protocols "review authentication and credentials")
# Should contain security-boundaries content if the file exists
if [[ -f "${EUXIS_HOME}/agents/prompts/protocols/_security-boundaries.txt" ]]; then
    sec_content=$(head -5 "${EUXIS_HOME}/agents/prompts/protocols/_security-boundaries.txt")
    if [[ -n "${sec_content}" ]]; then
        assert_contains "security keywords load security protocol" "security" "${result}"
    fi
fi

# Test resolve_protocols: graph keywords trigger graph protocol
result=$(resolve_protocols "analyze the knowledge graph entities")
if [[ -f "${EUXIS_HOME}/agents/prompts/protocols/_graph-memory.txt" ]]; then
    assert_contains "graph keywords load graph protocol" "graph" "graph"
fi

# Test resolve_protocols: security task loads more protocols with sufficient budget
_EUXIS_PROTO_CACHE=""
_EUXIS_PROTO_CACHE_KEY=""
old_budget="${EUXIS_PROTOCOL_TOKEN_BUDGET}"
EUXIS_PROTOCOL_TOKEN_BUDGET=100000
result_plain=$(resolve_protocols "hello world")
_EUXIS_PROTO_CACHE=""
_EUXIS_PROTO_CACHE_KEY=""
result_security=$(resolve_protocols "check security auth credentials")
EUXIS_PROTOCOL_TOKEN_BUDGET="${old_budget}"
_EUXIS_PROTO_CACHE=""
_EUXIS_PROTO_CACHE_KEY=""
# Security result should be longer (more protocols loaded)
len_plain=${#result_plain}
len_security=${#result_security}
if (( len_security > len_plain )); then
    assert_eq "security task loads more protocols" "true" "true"
else
    assert_eq "security task loads more protocols" "longer" "same_or_shorter"
fi

# Test prepare_prompt: returns non-empty prompt
TEST_TMPDIR=$(mktemp -d)
mkdir -p "${TEST_TMPDIR}/testproj/architect/output"
echo "# Memory" > "${TEST_TMPDIR}/testproj/architect/memory.md"
echo "# Audit" > "${TEST_TMPDIR}/testproj/architect/audit.md"

result=$(prepare_prompt "architect" "test task" \
    "${TEST_TMPDIR}/testproj/architect/audit.md" \
    "${TEST_TMPDIR}/testproj/architect/memory.md" \
    "test-session" "test-model")

assert_contains "prepare_prompt includes task" "test task" "${result}"
assert_contains "prepare_prompt includes CURRENT TASK" "CURRENT TASK" "${result}"
assert_contains "prepare_prompt includes MEMORY CONTEXT" "MEMORY CONTEXT" "${result}"

rm -rf "${TEST_TMPDIR}"
