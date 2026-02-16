#!/usr/bin/env bash
# Tests for specialist agents: crypto, payments, audio, rust

EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"
source "${EUXIS_HOME}/bin/lib/agents.sh"
source "${EUXIS_HOME}/bin/lib/providers.sh"

# Agent prompt files exist
assert_file_exists "crypto-cryptography-auditor prompt" "${EUXIS_HOME}/agents/prompts/fleet/crypto-cryptography-auditor.txt"
assert_file_exists "payments-domain-steward prompt" "${EUXIS_HOME}/agents/prompts/fleet/payments-domain-steward.txt"
assert_file_exists "realtime-audio-engineer prompt" "${EUXIS_HOME}/agents/prompts/fleet/realtime-audio-engineer.txt"
assert_file_exists "rust-crate-steward prompt" "${EUXIS_HOME}/agents/prompts/fleet/rust-crate-steward.txt"

# Agent paths resolve correctly
crypto_path=$(resolve_agent_path "crypto-cryptography-auditor")
assert_contains "crypto path resolves" "crypto-cryptography-auditor.txt" "${crypto_path}"

payments_path=$(resolve_agent_path "payments-domain-steward")
assert_contains "payments path resolves" "payments-domain-steward.txt" "${payments_path}"

audio_path=$(resolve_agent_path "realtime-audio-engineer")
assert_contains "audio path resolves" "realtime-audio-engineer.txt" "${audio_path}"

rust_path=$(resolve_agent_path "rust-crate-steward")
assert_contains "rust path resolves" "rust-crate-steward.txt" "${rust_path}"

# Provider tiering
crypto_provider=$(resolve_tiered_provider "crypto-cryptography-auditor")
assert_eq "crypto → claude" "claude" "${crypto_provider}"

payments_provider=$(resolve_tiered_provider "payments-domain-steward")
assert_eq "payments → claude" "claude" "${payments_provider}"

audio_provider=$(resolve_tiered_provider "realtime-audio-engineer")
assert_eq "audio → goose" "goose" "${audio_provider}"

rust_provider=$(resolve_tiered_provider "rust-crate-steward")
assert_eq "rust → goose" "goose" "${rust_provider}"

# P0 overrides to claude
crypto_p0=$(resolve_tiered_provider "crypto-cryptography-auditor" "P0")
assert_eq "crypto P0 → claude" "claude" "${crypto_p0}"

audio_p0=$(resolve_tiered_provider "realtime-audio-engineer" "P0")
assert_eq "audio P0 → claude" "claude" "${audio_p0}"

# Prompt header validation
for agent in crypto-cryptography-auditor payments-domain-steward realtime-audio-engineer rust-crate-steward; do
    prompt_file=$(resolve_agent_path "${agent}")
    # Has YAML frontmatter
    first_line=$(head -1 "${prompt_file}")
    assert_eq "${agent} has YAML frontmatter" "---" "${first_line}"

    # Has agent_id
    if grep -q "agent_id:" "${prompt_file}"; then
        assert_eq "${agent} has agent_id" "0" "0"
    else
        assert_eq "${agent} has agent_id" "found" "missing"
    fi

    # Has Mandate
    if grep -qi "mandate" "${prompt_file}"; then
        assert_eq "${agent} has Mandate" "0" "0"
    else
        assert_eq "${agent} has Mandate" "found" "missing"
    fi

    # Has Output Format
    if grep -qi "output format" "${prompt_file}"; then
        assert_eq "${agent} has Output Format" "0" "0"
    else
        assert_eq "${agent} has Output Format" "found" "missing"
    fi
done
