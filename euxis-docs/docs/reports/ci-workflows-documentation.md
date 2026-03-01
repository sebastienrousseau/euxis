# CI Workflows Documentation (v0.0.3)

This document describes the CI/CD workflows and release processes for the Euxis monorepo, generated as part of the multi-repo split preparation.

## GitHub Actions Workflows

### 1. Enforced CI Pipeline (`.github/workflows/enforced-ci.yml`)

Main CI pipeline with zero-tolerance enforcement.

**Triggers:** Push to `main`, `feat/*`, `release/*` branches; Pull requests to `main`

**Jobs:**

| Job | Description | Enforcement |
|-----|-------------|-------------|
| `lint-and-format` | Ruff, Black, isort, flake8, pylint | Zero warnings |
| `type-check` | mypy with basic + strict mode | Basic enforced, strict advisory |
| `security-scan` | pip-audit, safety, bandit, shellcheck | Blocking |
| `test-coverage` | pytest with 95% coverage requirement | Blocking |
| `integration-tests` | Shell-based integration tests | Blocking |
| `merge-gate` | Consolidates all job results | Gate for merge |

### 2. Architecture Guardrails (`.github/workflows/architecture-guardrails.yml`)

Validates module boundaries and dependency rules.

**Checks:**
- Core layer isolation (no UI/Network/Filesystem layer imports)
- Library dependency graph compliance
- Module boundary enforcement

### 3. Shell Script CI (`.github/workflows/shell-script-ci.yml`)

Runs BATS tests for shell scripts in `core/lib/`.

### 4. Cross-Platform CI (`.github/workflows/cross-platform-ci.yml`)

Tests on both Ubuntu and macOS for portability.

### 5. Naming Conventions (`.github/workflows/naming-conventions.yml`)

Enforces file naming standards using `euxis-ops/check-naming-conventions.py`.

### 6. Branding Check (`.github/workflows/branding-check.yml`)

Validates branding signatures in commits and PRs.

### 7. Merge Gate (`.github/workflows/merge-gate.yml`)

Final gate before merge approval.

### 8. Cache Cleanup (`.github/workflows/cache-cleanup.yml`)

Periodic cleanup of CI caches.

## Per-Module CI

Each extractable module has its own CI template at `<module>/ci/module-ci.yml`:

```yaml
name: Module CI
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.12'
      - run: pip install -e .[dev]
      - run: pytest -q
```

Modules with CI templates:
- `adapters/ci/module-ci.yml`
- `api/ci/module-ci.yml`
- `cli/ci/module-ci.yml`
- `config/ci/module-ci.yml`
- `core/ci/module-ci.yml`
- `crypto/ci/module-ci.yml`
- `metrics/ci/module-ci.yml`
- `packages/shared/ci/module-ci.yml`
- `euxis-policy/ci/module-ci.yml`
- `ui/ci/module-ci.yml`

## Release Process

### Current Release Steps

1. **Version Bump:** Update version in `agents/registry.json` (source of truth)
2. **Version Sync:** Run `euxis-ops/sync-docs.sh` to propagate version
3. **Certification:** Run `euxis-cli/bin/euxis-certify` to validate all gates
4. **Tag:** Create signed git tag `vX.Y.Z`
5. **Push:** Push tag to origin

### Post-Split Release Process

After extraction to separate repos:
1. Each repo manages its own version in `pyproject.toml`
2. Shared contracts define compatibility matrix
3. `euxis-core` releases first (dependency for others)
4. Dependent repos update their dependency versions
5. Integration tests run across all repos

## Certification Gates

The `euxis-cli/bin/euxis-certify` script runs 6 gates:

| Gate | Description |
|------|-------------|
| 0 | Environment validation |
| 0a | Certification prerequisites |
| 0b | Security & quality enforcement |
| 1+2 | Static analysis (lint) |
| 3 | Infrastructure tests |
| 4 | Semantic verification (golden dataset) |
| 5 | Branding compliance |
| 6 | Documentation governance |

---
*Generated:-02-16 as part of v0.0.3 multi-repo preparation*
