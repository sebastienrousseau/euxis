#!/usr/bin/env bash
# Tests for memory.sh performance optimizations

EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"
source "${EUXIS_HOME}/euxis-core/lib/memory.sh"

# ===== _extract_keywords (pure bash) =====

# Basic keyword extraction
kw=$(_extract_keywords "Check authentication token validation")
assert_contains "extracts 'check'" "check" "${kw}"
assert_contains "extracts 'authentication'" "authentication" "${kw}"
assert_contains "extracts 'token'" "token" "${kw}"
assert_contains "extracts 'validation'" "validation" "${kw}"

# Short words filtered (< 5 chars)
kw2=$(_extract_keywords "Fix the bug in app")
# "fix", "the", "bug", "app" are all < 5 chars
assert_eq "short words filtered" "" "${kw2}"

# Deduplication
kw3=$(_extract_keywords "token token token token token")
assert_eq "deduplication works" "token" "${kw3}"

# Max 10 keywords
long_task="alpha bravo charlie delta echo foxtrot golf hotel india juliet mike lima"
kw4=$(_extract_keywords "${long_task}")
# Count pipes (separators) — max 10 keywords = max 9 pipes
pipe_count=$(echo "${kw4}" | tr -cd '|' | wc -c)
if (( pipe_count <= 9 )); then
    assert_eq "max 10 keywords enforced" "0" "0"
else
    assert_eq "max 10 keywords enforced" "<=9" "${pipe_count}"
fi

# Case insensitive
kw5=$(_extract_keywords "AUTHENTICATION TOKEN")
assert_contains "case insensitive" "authentication" "${kw5}"

# ===== No subprocess forks in _extract_keywords =====
# The old pattern was: echo | tr '[:upper:]' '[:lower:]' | grep -oE '[a-z]{5,}' | sort -u | head
# Verify the old pattern is gone (ignore comments)
if grep -v '^[[:space:]]*#' "${EUXIS_HOME}/euxis-core/lib/memory.sh" | grep -q "grep -oE"; then
    assert_eq "_extract_keywords has no forks" "no-forks" "has-forks"
else
    assert_eq "_extract_keywords has no forks" "0" "0"
fi

# ===== No basename/dirname subprocess in cross-agent =====
if grep -q '$(basename' "${EUXIS_HOME}/euxis-core/lib/memory.sh" || grep -q '$(dirname' "${EUXIS_HOME}/euxis-core/lib/memory.sh"; then
    assert_eq "cross-agent uses pure bash paths" "pure-bash" "has-subprocess"
else
    assert_eq "cross-agent uses pure bash paths" "0" "0"
fi

# ===== Pipe-format pattern (for grep compatibility) =====
kw6=$(_extract_keywords "security authentication validation")
# Should be pipe-separated (grep alternation format)
if [[ "${kw6}" =~ \| ]]; then
    assert_eq "pipe-separated format" "0" "0"
else
    assert_eq "pipe-separated format" "has-pipes" "no-pipes"
fi
