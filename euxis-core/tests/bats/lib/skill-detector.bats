#!/usr/bin/env bats
# Test suite for core/lib/skill-detector.sh

# Test setup - run before each test
setup() {
    EUXIS_TEST_TMPDIR="$(mktemp -d)"
    export EUXIS_TEST_TMPDIR

    export EUXIS_HOME="${HOME}/.euxis"

    # Create test project directories
    export TEST_PROJECT_DIR="${EUXIS_TEST_TMPDIR}/project"
    mkdir -p "${TEST_PROJECT_DIR}"

    export PATH="${EUXIS_TEST_TMPDIR}:${PATH}"

    # Reset include guard
    unset _EUXIS_LIB_SKILL_DETECTOR

    # Source the library from real installation
    source "${EUXIS_HOME}/euxis-core/lib/skill-detector.sh"
}

teardown() {
    if [[ -n "${EUXIS_TEST_TMPDIR:-}" && -d "${EUXIS_TEST_TMPDIR:-}" ]]; then
        rm -rf "${EUXIS_TEST_TMPDIR}"
    fi
}

# ============================================================================
# DETECT_PROJECT_DOMAIN TESTS - WEB/FRONTEND
# ============================================================================

@test "detect_project_domain detects Node.js project" {
    echo '{"name": "test"}' > "${TEST_PROJECT_DIR}/package.json"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "web-ui-architect" ]] || [[ "${output}" =~ "cli-ui-artisan" ]]
}

@test "detect_project_domain detects TypeScript project" {
    echo '{"name": "test"}' > "${TEST_PROJECT_DIR}/package.json"
    echo '{}' > "${TEST_PROJECT_DIR}/tsconfig.json"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "unit-tester" ]]
}

@test "detect_project_domain detects node_modules" {
    mkdir -p "${TEST_PROJECT_DIR}/node_modules"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "web-ui-architect" ]] || [[ "${output}" =~ "cli-ui-artisan" ]]
}

# ============================================================================
# DETECT_PROJECT_DOMAIN TESTS - PYTHON
# ============================================================================

@test "detect_project_domain detects Python pyproject.toml" {
    touch "${TEST_PROJECT_DIR}/pyproject.toml"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "unit-tester" ]] || [[ "${output}" =~ "automation-engineer" ]]
}

@test "detect_project_domain detects Python setup.py" {
    touch "${TEST_PROJECT_DIR}/setup.py"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "unit-tester" ]] || [[ "${output}" =~ "automation-engineer" ]]
}

@test "detect_project_domain detects Python requirements.txt" {
    touch "${TEST_PROJECT_DIR}/requirements.txt"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "unit-tester" ]] || [[ "${output}" =~ "automation-engineer" ]]
}

# ============================================================================
# DETECT_PROJECT_DOMAIN TESTS - DOCKER/INFRASTRUCTURE
# ============================================================================

@test "detect_project_domain detects Dockerfile" {
    touch "${TEST_PROJECT_DIR}/Dockerfile"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "automation-engineer" ]] || [[ "${output}" =~ "perf-optimizer" ]]
}

@test "detect_project_domain detects docker-compose.yml" {
    touch "${TEST_PROJECT_DIR}/docker-compose.yml"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "automation-engineer" ]]
}

@test "detect_project_domain detects docker-compose.yaml" {
    touch "${TEST_PROJECT_DIR}/docker-compose.yaml"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "automation-engineer" ]]
}

# ============================================================================
# DETECT_PROJECT_DOMAIN TESTS - KUBERNETES
# ============================================================================

@test "detect_project_domain detects k8s directory" {
    mkdir -p "${TEST_PROJECT_DIR}/k8s"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "automation-engineer" ]] || [[ "${output}" =~ "incident-commander" ]]
}

@test "detect_project_domain detects helm directory" {
    mkdir -p "${TEST_PROJECT_DIR}/helm"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "automation-engineer" ]]
}

# ============================================================================
# DETECT_PROJECT_DOMAIN TESTS - SECURITY
# ============================================================================

@test "detect_project_domain detects .env file" {
    touch "${TEST_PROJECT_DIR}/.env"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "security-lead" ]] || [[ "${output}" =~ "edge-hunter" ]]
}

@test "detect_project_domain detects .env.example" {
    touch "${TEST_PROJECT_DIR}/.env.example"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "security" ]] || [[ "${output}" =~ "edge-hunter" ]]
}

@test "detect_project_domain detects certs directory" {
    mkdir -p "${TEST_PROJECT_DIR}/certs"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "security" ]] || [[ "${output}" =~ "crypto" ]]
}

# ============================================================================
# DETECT_PROJECT_DOMAIN TESTS - CI/CD
# ============================================================================

@test "detect_project_domain detects GitHub workflows" {
    mkdir -p "${TEST_PROJECT_DIR}/.github/workflows"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "automation-engineer" ]] || [[ "${output}" =~ "release-manager" ]]
}

@test "detect_project_domain detects GitLab CI" {
    touch "${TEST_PROJECT_DIR}/.gitlab-ci.yml"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "automation-engineer" ]]
}

@test "detect_project_domain detects Jenkinsfile" {
    touch "${TEST_PROJECT_DIR}/Jenkinsfile"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "automation-engineer" ]]
}

# ============================================================================
# DETECT_PROJECT_DOMAIN TESTS - DOCUMENTATION
# ============================================================================

@test "detect_project_domain detects docs directory" {
    mkdir -p "${TEST_PROJECT_DIR}/docs"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "tech-writer" ]]
}

@test "detect_project_domain detects README.md" {
    touch "${TEST_PROJECT_DIR}/README.md"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "tech-writer" ]]
}

# ============================================================================
# DETECT_PROJECT_DOMAIN TESTS - SYSTEMS LANGUAGES
# ============================================================================

@test "detect_project_domain detects Rust Cargo.toml" {
    touch "${TEST_PROJECT_DIR}/Cargo.toml"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "perf-optimizer" ]] || [[ "${output}" =~ "rust-crate-steward" ]]
}

@test "detect_project_domain detects Go go.mod" {
    touch "${TEST_PROJECT_DIR}/go.mod"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "perf-optimizer" ]] || [[ "${output}" =~ "bug-fixer" ]]
}

@test "detect_project_domain detects Makefile" {
    touch "${TEST_PROJECT_DIR}/Makefile"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "perf-optimizer" ]] || [[ "${output}" =~ "bug-fixer" ]]
}

# ============================================================================
# DETECT_PROJECT_DOMAIN TESTS - LEGACY
# ============================================================================

@test "detect_project_domain detects Ruby Gemfile" {
    touch "${TEST_PROJECT_DIR}/Gemfile"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "legacy-maintainer" ]]
}

@test "detect_project_domain detects .ruby-version" {
    touch "${TEST_PROJECT_DIR}/.ruby-version"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "legacy-maintainer" ]]
}

# ============================================================================
# DETECT_PROJECT_DOMAIN TESTS - CRYPTO
# ============================================================================

@test "detect_project_domain detects crypto in Cargo.toml" {
    echo 'ring = "0.17"' > "${TEST_PROJECT_DIR}/Cargo.toml"
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "crypto" ]]
}

# ============================================================================
# DETECT_PROJECT_DOMAIN TESTS - EMPTY/EDGE CASES
# ============================================================================

@test "detect_project_domain returns empty for empty directory" {
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    # Output may be empty
}

@test "detect_project_domain handles missing directory" {
    run detect_project_domain "/nonexistent/directory"
    [[ "${status}" -eq 0 ]]
}

@test "detect_project_domain uses PWD as default" {
    cd "${TEST_PROJECT_DIR}"
    touch "package.json"
    run detect_project_domain
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "web-ui-architect" ]] || [[ "${output}" =~ "cli-ui-artisan" ]]
}

@test "detect_project_domain deduplicates agents" {
    # Create multiple indicators that would suggest the same agent
    touch "${TEST_PROJECT_DIR}/Dockerfile"
    touch "${TEST_PROJECT_DIR}/docker-compose.yml"
    mkdir -p "${TEST_PROJECT_DIR}/.github/workflows"

    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]

    # Count occurrences of automation-engineer
    local count
    count=$(echo "${output}" | tr ' ' '\n' | grep -c "automation-engineer" || echo "0")
    [[ "${count}" -le 1 ]]
}

# ============================================================================
# DETECT_PROJECT_TYPE TESTS
# ============================================================================

@test "detect_project_type identifies Node project" {
    touch "${TEST_PROJECT_DIR}/package.json"
    run detect_project_type "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "node" ]]
}

@test "detect_project_type identifies Python project" {
    touch "${TEST_PROJECT_DIR}/pyproject.toml"
    run detect_project_type "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "python" ]]
}

@test "detect_project_type identifies Rust project" {
    touch "${TEST_PROJECT_DIR}/Cargo.toml"
    run detect_project_type "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "rust" ]]
}

@test "detect_project_type identifies Go project" {
    touch "${TEST_PROJECT_DIR}/go.mod"
    run detect_project_type "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "go" ]]
}

@test "detect_project_type identifies Docker project" {
    touch "${TEST_PROJECT_DIR}/Dockerfile"
    run detect_project_type "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "docker" ]]
}

@test "detect_project_type identifies GitHub project" {
    mkdir -p "${TEST_PROJECT_DIR}/.github"
    run detect_project_type "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "github" ]]
}

@test "detect_project_type identifies Make project" {
    touch "${TEST_PROJECT_DIR}/Makefile"
    run detect_project_type "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "make" ]]
}

@test "detect_project_type returns generic for empty directory" {
    run detect_project_type "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" == "generic" ]]
}

@test "detect_project_type identifies multiple types" {
    touch "${TEST_PROJECT_DIR}/package.json"
    touch "${TEST_PROJECT_DIR}/Dockerfile"
    mkdir -p "${TEST_PROJECT_DIR}/.github"

    run detect_project_type "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "node" ]]
    [[ "${output}" =~ "docker" ]]
    [[ "${output}" =~ "github" ]]
}

@test "detect_project_type uses PWD as default" {
    cd "${TEST_PROJECT_DIR}"
    touch "go.mod"
    run detect_project_type
    [[ "${status}" -eq 0 ]]
    [[ "${output}" =~ "go" ]]
}

# ============================================================================
# ERROR PATHS AND EDGE CASES
# ============================================================================

@test "skill-detector functions handle set -u mode" {
    set -u
    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
}

@test "skill-detector functions handle pipefail mode" {
    set -e -o pipefail
    run detect_project_type "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]
}

# ============================================================================
# INTEGRATION TESTS
# ============================================================================

@test "skill-detector module loads without errors" {
    [[ -n "${_EUXIS_LIB_SKILL_DETECTOR}" ]]
}

@test "detect_project_domain returns sorted results" {
    touch "${TEST_PROJECT_DIR}/package.json"
    touch "${TEST_PROJECT_DIR}/Dockerfile"
    mkdir -p "${TEST_PROJECT_DIR}/docs"

    run detect_project_domain "${TEST_PROJECT_DIR}"
    [[ "${status}" -eq 0 ]]

    # Results should be space-separated and sorted
    local sorted
    sorted=$(echo "${output}" | tr ' ' '\n' | sort | tr '\n' ' ' | xargs)
    local original
    original=$(echo "${output}" | xargs)
    [[ "${sorted}" == "${original}" ]]
}

@test "detection is consistent across calls" {
    touch "${TEST_PROJECT_DIR}/package.json"

    local result1
    result1=$(detect_project_domain "${TEST_PROJECT_DIR}")
    local result2
    result2=$(detect_project_domain "${TEST_PROJECT_DIR}")

    [[ "${result1}" == "${result2}" ]]
}
