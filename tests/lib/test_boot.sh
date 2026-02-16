#!/usr/bin/env bash
# Tests for euxis.sh boot path optimizations

# Test: euxis.sh has EUXIS_HEALTH_CHECK skip guard
assert_contains "health check skip guard" "EUXIS_HEALTH_CHECK" "$(grep 'EUXIS_HEALTH_CHECK' "${EUXIS_HOME}/cli/bin/euxis.sh")"

# Test: euxis.sh has dispatch skip for health check
assert_contains "dispatch skips health check" "EUXIS_DISPATCH" "$(grep 'EUXIS_DISPATCH' "${EUXIS_HOME}/cli/bin/euxis.sh" | head -1)"

# Test: show_context is conditional on terminal
assert_contains "context display terminal check" "\-t 1" "$(grep '\-t 1' "${EUXIS_HOME}/cli/bin/euxis.sh" | head -1)"

# Test: git_guard skips in dispatch mode
assert_contains "git guard dispatch skip" "EUXIS_DISPATCH" "$(grep 'EUXIS_DISPATCH' "${EUXIS_HOME}/cli/bin/euxis.sh" | head -3 | tail -1)"

# Test: uses $(< file) instead of $(cat file) for hash check
assert_contains "uses bash read instead of cat" '< "${hash_file}"' "$(grep 'hash_file' "${EUXIS_HOME}/cli/bin/euxis.sh" | grep '<')"

# Test: euxis.sh syntax is valid
bash -n "${EUXIS_HOME}/cli/bin/euxis.sh" 2>/dev/null
assert_eq "euxis.sh syntax valid" "0" "$?"

# Test: all lib files have valid syntax
for lib in "${EUXIS_HOME}/core/lib/"*.sh; do
    [[ -f "${lib}" ]] || continue
    bash -n "${lib}" 2>/dev/null
    name=$(basename "${lib}")
    assert_eq "${name} syntax valid" "0" "$?"
done

# Test: common.sh has log_debug
assert_contains "common.sh has log_debug" "log_debug" "$(cat "${EUXIS_HOME}/core/lib/common.sh")"

# Test: common.sh log_debug is conditional on EUXIS_DEBUG
assert_contains "log_debug checks EUXIS_DEBUG" "EUXIS_DEBUG" "$(grep 'EUXIS_DEBUG' "${EUXIS_HOME}/core/lib/common.sh")"
