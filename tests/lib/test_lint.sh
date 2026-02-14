#!/usr/bin/env bash
# Tests for euxis-lint itself

EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"

# Lint passes cleanly
lint_output=$(bash "${EUXIS_HOME}/bin/euxis-lint" 2>&1)
lint_exit=$?
assert_eq "lint exits 0" "0" "${lint_exit}"
assert_contains "lint passes" "LINT PASSED" "${lint_output}"

# Has 6 checks
assert_contains "check 1 registry" "[1/6]" "${lint_output}"
assert_contains "check 2 protocol" "[2/6]" "${lint_output}"
assert_contains "check 3 versions" "[3/6]" "${lint_output}"
assert_contains "check 4 permissions" "[4/6]" "${lint_output}"
assert_contains "check 5 headers" "[5/6]" "${lint_output}"
assert_contains "check 6 patterns" "[6/6]" "${lint_output}"

# Reports 38 agents
assert_contains "38 agents" "38 agents" "${lint_output}"

# Reports 11 patterns
assert_contains "11 patterns" "11 patterns" "${lint_output}"

# Reports 54 detection rules
assert_contains "54 detection rules" "54 total detection rules" "${lint_output}"

# Lint script is executable
if [[ -x "${EUXIS_HOME}/bin/euxis-lint" ]]; then
    assert_eq "lint is executable" "0" "0"
else
    assert_eq "lint is executable" "executable" "not-executable"
fi
