#!/usr/bin/env bash
# Tests for playbooks, templates, and squads

EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"

# ===== PLAYBOOK EXISTENCE =====
for pb in zero-to-one legacy-overhaul red-alert verify-everything crypto-audit payments-integration rust-release audio-pipeline; do
    assert_file_exists "playbook ${pb}" "${EUXIS_HOME}/euxis-core/config/playbooks/${pb}.json"
done

# ===== PLAYBOOK STRUCTURE =====
for pb in crypto-audit payments-integration rust-release audio-pipeline; do
    pb_file="${EUXIS_HOME}/euxis-core/config/playbooks/${pb}.json"

    # Valid JSON
    if jq empty "${pb_file}" 2>/dev/null; then
        assert_eq "${pb} is valid JSON" "0" "0"
    else
        assert_eq "${pb} is valid JSON" "valid" "invalid"
    fi

    # Has required fields
    has_id=$(jq -r '.id // empty' "${pb_file}" 2>/dev/null)
    assert_eq "${pb} has id" "${pb}" "${has_id}"

    has_phases=$(jq '.phases | length' "${pb_file}" 2>/dev/null)
    if (( has_phases > 0 )); then
        assert_eq "${pb} has phases" "0" "0"
    else
        assert_eq "${pb} has phases" "found" "missing"
    fi

    # Each phase has required fields
    phase_count=$(jq '.phases | length' "${pb_file}" 2>/dev/null)
    for (( i=0; i<phase_count; i++ )); do
        has_name=$(jq -r ".phases[${i}].name // empty" "${pb_file}" 2>/dev/null)
        if [[ -n "${has_name}" ]]; then
            assert_eq "${pb} phase $((i+1)) has name" "0" "0"
        else
            assert_eq "${pb} phase $((i+1)) has name" "found" "missing"
        fi
    done
done

# ===== TEMPLATE EXISTENCE =====
for tpl in dispatch-manifest.json agent-prompt.txt pattern.md adr.md playbook.json; do
    assert_file_exists "template ${tpl}" "${EUXIS_HOME}/euxis-core/config/templates/${tpl}"
done

# ===== SQUADS.JSON =====
squads_file="${EUXIS_HOME}/euxis-core/agents/squads.json"
assert_file_exists "agents/squads.json" "${squads_file}"

# Valid JSON
if jq empty "${squads_file}" 2>/dev/null; then
    assert_eq "agents/squads.json is valid JSON" "0" "0"
else
    assert_eq "agents/squads.json is valid JSON" "valid" "invalid"
fi

# Has 6 squads
squad_count=$(jq '.squads | length' "${squads_file}" 2>/dev/null)
assert_eq "6 squads" "6" "${squad_count}"

# Squad IDs
for sid in vision build quality growth experience specialist; do
    has_squad=$(jq -r ".squads[] | select(.id == \"${sid}\") | .id" "${squads_file}" 2>/dev/null)
    assert_eq "squad ${sid} exists" "${sid}" "${has_squad}"
done

# Has 6 combos
combo_count=$(jq '.combos | length' "${squads_file}" 2>/dev/null)
assert_eq "6 combos" "6" "${combo_count}"

# Combo IDs
for cid in envision protect craft refine seal settle; do
    has_combo=$(jq -r ".combos[] | select(.id == \"${cid}\") | .id" "${squads_file}" 2>/dev/null)
    assert_eq "combo ${cid} exists" "${cid}" "${has_combo}"
done

# Specialist squad has correct members
specialist_members=$(jq -r '.squads[] | select(.id == "specialist") | .members | length' "${squads_file}" 2>/dev/null)
assert_eq "specialist squad has 4 members" "4" "${specialist_members}"
