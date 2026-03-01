#!/usr/bin/env bash
# Euxis Library: Skill-based auto-detection
# Examines the working directory to suggest domain-relevant agents.
# Optimized: bash arrays for deduplication, no subprocess forks
[[ -n "${_EUXIS_LIB_SKILL_DETECTOR:-}" ]] && return; _EUXIS_LIB_SKILL_DETECTOR=1

set -euo pipefail


EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"

# Detect project domain from working directory contents.
# Returns a space-separated list of recommended agent IDs.
detect_project_domain() {
    local dir="${1:-${PWD}}"
    local -A seen=()

    # Helper: add agent to deduplicated set
    _add() { local a; for a in "$@"; do seen["${a}"]=1; done; }

    # Web/Frontend
    if [[ -f "${dir}/package.json" ]] || [[ -d "${dir}/node_modules" ]]; then
        _add web-ui-architect cli-ui-artisan
        if [[ -f "${dir}/tsconfig.json" ]]; then
            _add unit-tester
        fi
    fi

    # Python
    if [[ -f "${dir}/pyproject.toml" ]] || [[ -f "${dir}/setup.py" ]] || [[ -f "${dir}/requirements.txt" ]]; then
        _add unit-tester automation-engineer
    fi

    # Docker/Infrastructure
    if [[ -f "${dir}/Dockerfile" ]] || [[ -f "${dir}/docker-compose.yml" ]] || [[ -f "${dir}/docker-compose.yaml" ]]; then
        _add automation-engineer perf-optimizer
    fi

    # Kubernetes
    if [[ -d "${dir}/k8s" ]] || [[ -d "${dir}/helm" ]] || ls "${dir}"/*.yaml 2>/dev/null | grep -q 'kind:'; then
        _add automation-engineer incident-commander
    fi

    # Security indicators
    if [[ -f "${dir}/.env" ]] || [[ -f "${dir}/.env.example" ]] || [[ -d "${dir}/certs" ]]; then
        _add security-lead edge-hunter
    fi

    # CI/CD
    if [[ -d "${dir}/.github/workflows" ]] || [[ -f "${dir}/.gitlab-ci.yml" ]] || [[ -f "${dir}/Jenkinsfile" ]]; then
        _add automation-engineer release-manager
    fi

    # Documentation-heavy
    if [[ -d "${dir}/docs" ]] || [[ -f "${dir}/README.md" ]]; then
        _add tech-writer
    fi

    # Rust/Go/C (systems)
    if [[ -f "${dir}/Cargo.toml" ]] || [[ -f "${dir}/go.mod" ]] || [[ -f "${dir}/Makefile" ]]; then
        _add perf-optimizer bug-fixer
    fi

    # Rust-specific: add crate steward
    if [[ -f "${dir}/Cargo.toml" ]]; then
        _add rust-crate-steward
    fi

    # Legacy indicators
    if [[ -f "${dir}/Gemfile" ]] || [[ -f "${dir}/.ruby-version" ]]; then
        _add legacy-maintainer
    fi

    # Crypto indicators
    if [[ -d "${dir}/certs" ]] || [[ -f "${dir}/Cargo.toml" ]] && grep -q 'ring\|rustls\|openssl' "${dir}/Cargo.toml" 2>/dev/null; then
        _add crypto-cryptography-auditor
    fi

    # Output sorted unique agents (pure bash — no forks)
    local result=""
    local key
    for key in $(printf '%s\n' "${!seen[@]}" | sort); do
        result="${result}${result:+ }${key}"
    done
    printf '%s' "${result}"
}

# Detect project type label for logging
detect_project_type() {
    local dir="${1:-${PWD}}"
    local types=""

    [[ -f "${dir}/package.json" ]] && types="${types} node"
    [[ -f "${dir}/pyproject.toml" ]] || [[ -f "${dir}/setup.py" ]] && types="${types} python"
    [[ -f "${dir}/Cargo.toml" ]] && types="${types} rust"
    [[ -f "${dir}/go.mod" ]] && types="${types} go"
    [[ -f "${dir}/Dockerfile" ]] && types="${types} docker"
    [[ -d "${dir}/.github" ]] && types="${types} github"
    [[ -f "${dir}/Makefile" ]] && types="${types} make"

    if [[ -z "${types}" ]]; then
        echo "generic"
    else
        # Pure bash trim leading space
        printf '%s' "${types# }"
    fi
}
