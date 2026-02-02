#!/usr/bin/env bash
# Tests for registry.json integrity

EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"
registry="${EUXIS_HOME}/registry.json"

# Valid JSON
if jq empty "${registry}" 2>/dev/null; then
    assert_eq "registry is valid JSON" "0" "0"
else
    assert_eq "registry is valid JSON" "valid" "invalid"
fi

# Has 38 agents
agent_count=$(jq '.agents | length' "${registry}" 2>/dev/null)
assert_eq "38 agents in registry" "38" "${agent_count}"

# Every agent has required fields
all_have_id=$(jq '[.agents[] | select(.id == null or .id == "")] | length' "${registry}" 2>/dev/null)
assert_eq "all agents have id" "0" "${all_have_id}"

all_have_path=$(jq '[.agents[] | select(.path == null or .path == "")] | length' "${registry}" 2>/dev/null)
assert_eq "all agents have path" "0" "${all_have_path}"

all_have_tier=$(jq '[.agents[] | select(.tier == null or .tier == "")] | length' "${registry}" 2>/dev/null)
assert_eq "all agents have tier" "0" "${all_have_tier}"

all_have_version=$(jq '[.agents[] | select(.version == null or .version == "")] | length' "${registry}" 2>/dev/null)
assert_eq "all agents have version" "0" "${all_have_version}"

# All versions are 0.0.7
non_007=$(jq '[.agents[] | select(.version != "0.0.7")] | length' "${registry}" 2>/dev/null)
assert_eq "all versions 0.0.7" "0" "${non_007}"

# All prompt files exist
while IFS= read -r agent_path; do
    full_path="${EUXIS_HOME}/${agent_path}"
    agent_id=$(jq -r ".agents[] | select(.path == \"${agent_path}\") | .id" "${registry}" 2>/dev/null)
    if [[ -f "${full_path}" ]]; then
        assert_eq "prompt file ${agent_id}" "0" "0"
    else
        assert_eq "prompt file ${agent_id}" "exists" "missing: ${full_path}"
    fi
done < <(jq -r '.agents[].path' "${registry}" 2>/dev/null)

# Specialist agents have correct tags
crypto_tags=$(jq -r '.agents[] | select(.id == "crypto-cryptography-auditor") | .tags | join(",")' "${registry}" 2>/dev/null)
assert_contains "crypto has cryptography tag" "cryptography" "${crypto_tags}"

payments_tags=$(jq -r '.agents[] | select(.id == "payments-domain-steward") | .tags | join(",")' "${registry}" 2>/dev/null)
assert_contains "payments has iso20022 tag" "iso20022" "${payments_tags}"

audio_tags=$(jq -r '.agents[] | select(.id == "realtime-audio-engineer") | .tags | join(",")' "${registry}" 2>/dev/null)
assert_contains "audio has realtime tag" "realtime" "${audio_tags}"

rust_tags=$(jq -r '.agents[] | select(.id == "rust-crate-steward") | .tags | join(",")' "${registry}" 2>/dev/null)
assert_contains "rust has publishing tag" "publishing" "${rust_tags}"
