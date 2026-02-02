#!/usr/bin/env bash
# Tests for validation patterns

# Test: all pattern files exist
EXPECTED_PATTERNS="SECURITY-001.md PERF-001.md QUALITY-001.md MEMORY-001.md CONCURRENCY-001.md ERROR-001.md API-001.md TEST-001.md LOG-001.md DEPENDENCY-001.md SCHEMA-001.md"
for p in $EXPECTED_PATTERNS; do
    assert_file_exists "pattern ${p} exists" "${EUXIS_HOME}/patterns/${p}"
done

# Test: README exists
assert_file_exists "patterns README exists" "${EUXIS_HOME}/patterns/README.md"

# Test: each pattern has required sections
for p in $EXPECTED_PATTERNS; do
    pf="${EUXIS_HOME}/patterns/${p}"
    [[ -f "${pf}" ]] || continue
    name="${p%.md}"

    has_pattern=$(grep -c "## Pattern" "${pf}" 2>/dev/null || echo "0")
    assert_eq "${name} has Pattern section" "1" "${has_pattern}"

    has_severity=$(grep -c "## Severity" "${pf}" 2>/dev/null || echo "0")
    assert_eq "${name} has Severity section" "1" "${has_severity}"

    has_rules=$(grep -c "## Detection Rules" "${pf}" 2>/dev/null || echo "0")
    assert_eq "${name} has Detection Rules section" "1" "${has_rules}"

    has_validation=$(grep -c "## Validation" "${pf}" 2>/dev/null || echo "0")
    assert_eq "${name} has Validation section" "1" "${has_validation}"

    has_remediation=$(grep -c "## Remediation" "${pf}" 2>/dev/null || echo "0")
    assert_eq "${name} has Remediation section" "1" "${has_remediation}"
done

# Test: severity values are valid
for p in $EXPECTED_PATTERNS; do
    pf="${EUXIS_HOME}/patterns/${p}"
    [[ -f "${pf}" ]] || continue
    severity=$(sed -n '/^## Severity$/,/^$/p' "${pf}" | grep -v "^## " | tr -d ' ' | head -1)
    case "${severity}" in
        CRITICAL|HIGH|MEDIUM|LOW)
            assert_eq "${p%.md} severity valid" "true" "true" ;;
        *)
            assert_eq "${p%.md} severity valid (got: ${severity})" "true" "false" ;;
    esac
done

# Test: naming convention
for f in "${EUXIS_HOME}/patterns/"*.md; do
    [[ -f "$f" ]] || continue
    name=$(basename "$f")
    [[ "$name" == "README.md" ]] && continue
    if echo "$name" | grep -qE '^[A-Z]+-[0-9]{3}\.md$'; then
        assert_eq "naming ${name}" "valid" "valid"
    else
        assert_eq "naming ${name}" "valid" "invalid"
    fi
done

# Test: total pattern count
count=$(ls -1 "${EUXIS_HOME}/patterns/"*.md 2>/dev/null | grep -v README | wc -l | tr -d ' ')
assert_eq "total pattern count" "11" "${count}"
