#!/usr/bin/env bash
# Tests for lib/session.sh

source "${EUXIS_HOME}/bin/lib/session.sh"

# Test get_project_name: defaults to basename of PWD
unset EUXIS_PROJECT 2>/dev/null || true
result=$(get_project_name)
expected=$(basename "$(pwd)")
assert_eq "get_project_name defaults to PWD basename" "${expected}" "${result}"

# Test get_project_name: respects EUXIS_PROJECT
EUXIS_PROJECT="test-project-123"
result=$(get_project_name)
assert_eq "get_project_name uses EUXIS_PROJECT" "test-project-123" "${result}"
unset EUXIS_PROJECT

# Test get_session_id: returns EUXIS_SESSION_ID if set
EUXIS_SESSION_ID="custom-session-42"
result=$(get_session_id)
assert_eq "get_session_id uses EUXIS_SESSION_ID" "custom-session-42" "${result}"
unset EUXIS_SESSION_ID

# Test get_session_id: default format is YYYYMMDD-HHMMSS
unset EUXIS_SESSION_ID 2>/dev/null || true
result=$(get_session_id)
if [[ "${result}" =~ ^[0-9]{8}-[0-9]{6}$ ]]; then
    assert_eq "get_session_id format YYYYMMDD-HHMMSS" "true" "true"
else
    assert_eq "get_session_id format YYYYMMDD-HHMMSS" "YYYYMMDD-HHMMSS" "${result}"
fi

# Test ensure_project_dirs: creates directories and init files
TEST_TMPDIR=$(mktemp -d)
PROJECTS_DIR="${TEST_TMPDIR}"
ensure_project_dirs "testproj" "testagent"
assert_dir_exists "ensure_project_dirs creates output dir" "${TEST_TMPDIR}/testproj/testagent/output"
assert_file_exists "ensure_project_dirs creates audit.md" "${TEST_TMPDIR}/testproj/testagent/audit.md"
assert_file_exists "ensure_project_dirs creates memory.md" "${TEST_TMPDIR}/testproj/testagent/memory.md"
rm -rf "${TEST_TMPDIR}"
# Restore PROJECTS_DIR
PROJECTS_DIR="${EUXIS_HOME}/projects"
