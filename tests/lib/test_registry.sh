#!/usr/bin/env bash
# Tests for registry integrity (SQLite + JSON)

EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"
registry="${EUXIS_HOME}/registry.json"
registry_db="${EUXIS_HOME}/registry.db"

# ============================================================================
# JSON Tests (original)
# ============================================================================

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

# ============================================================================
# SQLite Tests (parallel verification)
# ============================================================================

if [[ -f "${registry_db}" ]]; then
    # SQLite DB is accessible
    sql_agent_count=$(sqlite3 -init /dev/null "${registry_db}" "SELECT COUNT(*) FROM agents" 2>/dev/null)
    assert_eq "SQLite DB has agents" "true" "$([[ "${sql_agent_count}" -gt 0 ]] && echo true || echo false)"

    # Agent counts match between JSON and SQLite
    json_count=$(jq '.agents | length' "${registry}" 2>/dev/null)
    assert_eq "JSON/SQLite agent count match" "${json_count}" "${sql_agent_count}"

    # All SQLite agents have required fields (no NULL id/path/tier/version)
    sql_null_fields=$(sqlite3 -init /dev/null "${registry_db}" "SELECT COUNT(*) FROM agents WHERE id IS NULL OR path IS NULL OR tier IS NULL OR version IS NULL" 2>/dev/null)
    assert_eq "SQLite agents have required fields" "0" "${sql_null_fields}"

    # SQLite version matches JSON version
    sql_version=$(sqlite3 -init /dev/null "${registry_db}" "SELECT value FROM registry_metadata WHERE key='protocol_version'" 2>/dev/null)
    json_version=$(jq -r '.protocol_version' "${registry}" 2>/dev/null)
    assert_eq "SQLite/JSON version match" "${json_version}" "${sql_version}"

    # Agent IDs match between backends
    json_ids=$(jq -r '.agents[].id' "${registry}" 2>/dev/null | sort)
    sql_ids=$(sqlite3 -init /dev/null "${registry_db}" "SELECT id FROM agents ORDER BY id" 2>/dev/null)
    assert_eq "SQLite/JSON agent IDs match" "${json_ids}" "${sql_ids}"
else
    assert_eq "SQLite DB exists (optional)" "skip" "skip"
fi
