#!/usr/bin/env bash
# Tests for prompt.sh caching and optimization

source "${EUXIS_HOME}/euxis-core/lib/common.sh"
source "${EUXIS_HOME}/euxis-core/lib/prompt.sh"

# Test: _proto_fingerprint returns deterministic key
fp1=$(_proto_fingerprint "check security authentication")
fp2=$(_proto_fingerprint "check security authentication")
assert_eq "fingerprint deterministic" "${fp1}" "${fp2}"

# Test: different tasks produce different fingerprints
fp_sec=$(_proto_fingerprint "review security auth credentials")
fp_plain=$(_proto_fingerprint "hello world task")
if [[ "${fp_sec}" != "${fp_plain}" ]]; then
    assert_eq "different tasks different fingerprints" "true" "true"
else
    assert_eq "different tasks different fingerprints" "different" "same"
fi

# Test: security keywords produce fingerprint with sec
fp=$(_proto_fingerprint "review authentication security")
assert_contains "security fingerprint contains sec" "sec" "${fp}"

# Test: graph keywords produce fingerprint with gra
fp=$(_proto_fingerprint "analyze knowledge graph entities")
assert_contains "graph fingerprint contains gra" "gra" "${fp}"

# Test: empty task produces empty fingerprint (no optional protocols)
fp=$(_proto_fingerprint "hello world")
assert_eq "plain task empty fingerprint" "" "${fp}"

# Test: multiple keywords produce compound fingerprint
fp=$(_proto_fingerprint "security auth graph knowledge version release")
assert_contains "compound fingerprint has sec" "sec" "${fp}"
assert_contains "compound fingerprint has gra" "gra" "${fp}"
assert_contains "compound fingerprint has ver" "ver" "${fp}"

# Test: protocol resolution returns content
result=$(resolve_protocols "simple task")
assert_contains "protocols include mandatory protocol" "EUXIS MANDATORY PROTOCOL" "${result}"

# Test: security task includes security content (with higher budget)
old_budget="${EUXIS_PROTOCOL_TOKEN_BUDGET}"
EUXIS_PROTOCOL_TOKEN_BUDGET=100000
_EUXIS_PROTO_CACHE=""
_EUXIS_PROTO_CACHE_KEY=""
result_sec=$(resolve_protocols "review security authentication credentials")
EUXIS_PROTOCOL_TOKEN_BUDGET="${old_budget}"
_EUXIS_PROTO_CACHE=""
_EUXIS_PROTO_CACHE_KEY=""
assert_contains "security protocols loaded" "security" "${result_sec}"

# Test: token budget enforcement
EUXIS_PROTOCOL_TOKEN_BUDGET=100
_EUXIS_PROTO_CACHE=""
_EUXIS_PROTO_CACHE_KEY=""
result_small=$(resolve_protocols "security auth graph knowledge version release")
EUXIS_PROTOCOL_TOKEN_BUDGET=100000
_EUXIS_PROTO_CACHE=""
_EUXIS_PROTO_CACHE_KEY=""
result_large=$(resolve_protocols "security auth graph knowledge version release")
EUXIS_PROTOCOL_TOKEN_BUDGET="${old_budget}"

# Small budget should produce shorter or equal result
len_small=${#result_small}
len_large=${#result_large}
if (( len_small <= len_large )); then
    assert_eq "token budget limits content" "true" "true"
else
    assert_eq "token budget limits content" "small<=large" "small>large"
fi

# Test: bash-native operations (no grep -qiE forks in resolve_protocols)
if grep -q 'grep -qiE' "${EUXIS_HOME}/euxis-core/lib/prompt.sh" 2>/dev/null; then
    assert_eq "no grep forks in prompt.sh" "false" "true"
else
    assert_eq "no grep forks in prompt.sh" "false" "false"
fi

# Test: _get_fleet_roster works
roster=$(_get_fleet_roster)
if [[ -f "${EUXIS_HOME}/euxis-core/agents/registry.db" ]] || { [[ -f "${EUXIS_HOME}/euxis-core/agents/registry.json" ]] && command -v jq &>/dev/null; }; then
    assert_contains "fleet roster has agents" "architect" "${roster}"
else
    assert_eq "fleet roster skipped (no registry)" "ok" "ok"
fi

# Test: log_debug exists in common.sh
assert_contains "log_debug function exists" "log_debug" "$(grep 'log_debug' "${EUXIS_HOME}/euxis-core/lib/common.sh")"
