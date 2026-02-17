#!/usr/bin/env bash
# Tests verifying all performance optimizations are in place

EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"

# ===== memory.sh: No subprocess forks in keyword extraction =====
mem_file="${EUXIS_HOME}/core/lib/memory.sh"

# _extract_keywords exists
if grep -q '_extract_keywords' "${mem_file}"; then
    assert_eq "memory has _extract_keywords" "0" "0"
else
    assert_eq "memory has _extract_keywords" "found" "missing"
fi

# Uses bash parameter expansion for lowercase
if grep -q '${1,,}' "${mem_file}"; then
    assert_eq "memory uses bash lowercase" "0" "0"
else
    assert_eq "memory uses bash lowercase" "found" "missing"
fi

# Uses associative array (local -A) for dedup
if grep -q 'local -A seen' "${mem_file}"; then
    assert_eq "memory uses assoc array dedup" "0" "0"
else
    assert_eq "memory uses assoc array dedup" "found" "missing"
fi

# No tr/grep/sort/head subprocess calls in keyword extraction path
# The old code had: echo | tr | grep -oE | sort -u | head
# Check that the old pattern is gone
if grep -q "tr '\[:upper:\]' '\[:lower:\]'" "${mem_file}"; then
    assert_eq "old tr uppercase removed" "removed" "still-present"
else
    assert_eq "old tr uppercase removed" "0" "0"
fi

if grep -q "grep -oE '\[a-z\]" "${mem_file}"; then
    assert_eq "old grep keyword extract removed" "removed" "still-present"
else
    assert_eq "old grep keyword extract removed" "0" "0"
fi

# No basename/dirname subprocess in cross-agent memory
if grep -q '$(basename' "${mem_file}" || grep -q '$(dirname' "${mem_file}"; then
    assert_eq "no basename/dirname forks" "no-forks" "has-forks"
else
    assert_eq "no basename/dirname forks" "0" "0"
fi

# Uses pure bash path manipulation for sibling agent name
if grep -q '${sibling_mem%/memory.md}' "${mem_file}"; then
    assert_eq "pure bash path extraction" "0" "0"
else
    assert_eq "pure bash path extraction" "found" "missing"
fi

# ===== skill-detector.sh: No pipe chains =====
skill_file="${EUXIS_HOME}/core/lib/skill-detector.sh"

# Uses associative array (local -A)
if grep -q 'local -A seen' "${skill_file}"; then
    assert_eq "skill-detector uses assoc array" "0" "0"
else
    assert_eq "skill-detector uses assoc array" "found" "missing"
fi

# Old pipe chain removed
if grep -q "tr ' ' '\\\\n' | sort -u | grep" "${skill_file}"; then
    assert_eq "old pipe chain removed" "removed" "still-present"
else
    assert_eq "old pipe chain removed" "0" "0"
fi

# Uses ${PWD} instead of $(pwd)
if grep -q '$(pwd)' "${skill_file}"; then
    assert_eq "uses PWD not pwd fork" "no-fork" "has-fork"
else
    assert_eq "uses PWD not pwd fork" "0" "0"
fi

# detect_project_type uses pure bash trim
if grep -q '${types# }' "${skill_file}"; then
    assert_eq "pure bash trim" "0" "0"
else
    assert_eq "pure bash trim" "found" "missing"
fi

# ===== session.sh: Pure bash basename =====
session_file="${EUXIS_HOME}/core/lib/session.sh"

# No basename subprocess
if grep -q '$(basename' "${session_file}"; then
    assert_eq "session no basename fork" "no-fork" "has-basename"
else
    assert_eq "session no basename fork" "0" "0"
fi

# Uses ${PWD##*/}
if grep -q 'PWD##\*/' "${session_file}"; then
    assert_eq "session uses PWD expansion" "0" "0"
else
    assert_eq "session uses PWD expansion" "found" "missing"
fi

# ===== prompt.sh: Pure bash dirname =====
prompt_file="${EUXIS_HOME}/core/lib/prompt.sh"

# No dirname subprocess in prompt assembly (comment doesn't count)
if grep -v '^[[:space:]]*#' "${prompt_file}" | grep -q '$(dirname'; then
    assert_eq "prompt no dirname fork" "no-fork" "has-dirname"
else
    assert_eq "prompt no dirname fork" "0" "0"
fi

# Registry uses single jq call (join instead of tr|sed pipe)
if grep -q 'join(", ")' "${prompt_file}"; then
    assert_eq "registry single jq join" "0" "0"
else
    assert_eq "registry single jq join" "found" "missing"
fi

# ===== template.sh: No sed =====
tpl_file="${EUXIS_HOME}/core/lib/template.sh"
if grep -q '| sed' "${tpl_file}"; then
    assert_eq "template no sed" "no-sed" "has-sed"
else
    assert_eq "template no sed" "0" "0"
fi

# ===== Boot guards exist =====
euxis_file="${EUXIS_HOME}/cli/bin/euxis.sh"

# Health check skip
if grep -q 'EUXIS_HEALTH_CHECK.*skip' "${euxis_file}"; then
    assert_eq "health check skip guard" "0" "0"
else
    assert_eq "health check skip guard" "found" "missing"
fi

# Dispatch skip
if grep -q 'EUXIS_DISPATCH.*true' "${euxis_file}"; then
    assert_eq "dispatch skip guard" "0" "0"
else
    assert_eq "dispatch skip guard" "found" "missing"
fi

# Non-terminal skip
if grep -q '\-t 1' "${euxis_file}"; then
    assert_eq "non-terminal skip" "0" "0"
else
    assert_eq "non-terminal skip" "found" "missing"
fi
