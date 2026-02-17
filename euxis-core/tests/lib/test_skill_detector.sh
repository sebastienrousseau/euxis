#!/usr/bin/env bash
# Tests for lib/skill-detector.sh

source "${EUXIS_HOME}/euxis-core/lib/skill-detector.sh"

# Setup: create temporary project directories
TEST_TMPDIR=$(mktemp -d)

# Test detect_project_type: node project
mkdir -p "${TEST_TMPDIR}/node-proj"
touch "${TEST_TMPDIR}/node-proj/package.json"
result=$(detect_project_type "${TEST_TMPDIR}/node-proj")
assert_contains "node project detected" "node" "${result}"

# Test detect_project_type: python project
mkdir -p "${TEST_TMPDIR}/py-proj"
touch "${TEST_TMPDIR}/py-proj/pyproject.toml"
result=$(detect_project_type "${TEST_TMPDIR}/py-proj")
assert_contains "python project detected" "python" "${result}"

# Test detect_project_type: docker project
mkdir -p "${TEST_TMPDIR}/dock-proj"
touch "${TEST_TMPDIR}/dock-proj/Dockerfile"
result=$(detect_project_type "${TEST_TMPDIR}/dock-proj")
assert_contains "docker project detected" "docker" "${result}"

# Test detect_project_type: rust project
mkdir -p "${TEST_TMPDIR}/rust-proj"
touch "${TEST_TMPDIR}/rust-proj/Cargo.toml"
result=$(detect_project_type "${TEST_TMPDIR}/rust-proj")
assert_contains "rust project detected" "rust" "${result}"

# Test detect_project_type: generic (empty dir)
mkdir -p "${TEST_TMPDIR}/empty-proj"
result=$(detect_project_type "${TEST_TMPDIR}/empty-proj")
assert_eq "empty project is generic" "generic" "${result}"

# Test detect_project_domain: node project suggests web agents
result=$(detect_project_domain "${TEST_TMPDIR}/node-proj")
assert_contains "node suggests web-ui-architect" "web-ui-architect" "${result}"

# Test detect_project_domain: docker suggests automation
result=$(detect_project_domain "${TEST_TMPDIR}/dock-proj")
assert_contains "docker suggests automation-engineer" "automation-engineer" "${result}"

# Test detect_project_domain: security indicators
mkdir -p "${TEST_TMPDIR}/sec-proj"
touch "${TEST_TMPDIR}/sec-proj/.env"
result=$(detect_project_domain "${TEST_TMPDIR}/sec-proj")
assert_contains "env file suggests security-lead" "security-lead" "${result}"

# Test detect_project_domain: CI/CD
mkdir -p "${TEST_TMPDIR}/ci-proj/.github/workflows"
result=$(detect_project_domain "${TEST_TMPDIR}/ci-proj")
assert_contains "github workflows suggests automation-engineer" "automation-engineer" "${result}"

# Cleanup
rm -rf "${TEST_TMPDIR}"
