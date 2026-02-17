# Euxis CI Enforcement Manifest
**Zero-Tolerance Policy Implementation**

## System Overview
- **Type:** GitHub Actions CI/CD Pipeline with Zero-Tolerance Enforcement
- **Purpose:** Enforce strict code quality standards with no warnings or failures allowed
- **Idempotent:** YES - All checks are deterministic and repeatable
- **Cost Impact:** Optimized with dependency caching and parallel execution

## Enforcement Matrix

| Check Category | Tool | Policy | Failure Impact |
|---------------|------|--------|----------------|
| **Code Formatting** | Black + isort | Zero tolerance for unformatted code | ❌ Build fails |
| **Linting** | Ruff (ALL rules) | Zero warnings allowed | ❌ Build fails |
| **Type Checking** | mypy | Strict mode enforced | ❌ Build fails |
| **Test Coverage** | pytest-cov | 100% coverage required | ❌ Build fails |
| **Security Scanning** | bandit + pip-audit | Zero vulnerabilities | ❌ Build fails |
| **Shell Scripts** | shellcheck | Zero issues allowed | ❌ Build fails |
| **Architecture** | Custom rules | Dependency violations blocked | ❌ Build fails |

## Pipeline Configuration

### Main CI Pipeline (`cross-platform-ci.yml`)
```yaml
# Strict enforcement workflow
- Code Quality Enforcement (10min timeout)
  ├── Black formatting check (zero tolerance)
  ├── isort import sorting (zero tolerance)
  ├── Ruff linting (zero warnings)
  └── mypy type checking (strict mode)

- Test Suite (10min timeout)
  ├── pytest with 100% coverage requirement
  ├── Fail-under 100% enforced
  └── Upload coverage reports

- Security Scanning (10min timeout)
  ├── pip-audit (zero vulnerabilities)
  ├── bandit (zero security issues)
  └── shellcheck (zero script issues)

- Cross-Platform Verification (10min timeout)
  ├── Ubuntu Linux testing
  └── macOS testing

- Architecture Guardrails (10min timeout)
  ├── Core layer isolation validation
  └── Dependency graph enforcement
```

### Merge Gate (`merge-gate.yml`)
```yaml
# Blocks merges on ANY failure
- Enforces required status checks
- Validates branch protection rules
- Prevents draft PR merges
```

### Nightly Jobs
```yaml
# Extended maintenance (scheduled)
- Dependency update checks
- Extended test suite execution
- Performance benchmarking
- Memory leak detection
```

## Configuration Files

### `pyproject.toml` - Central Configuration
- **Ruff**: All rules enabled, complexity limits enforced
- **mypy**: Strict mode with all safety checks
- **Black**: 88-character line limit, Python 3.12 target
- **pytest**: 100% coverage requirement, 10-minute timeout
- **Coverage**: Branch coverage enabled, fail-under 100%

### `requirements-dev.txt` - Development Dependencies
- Code quality tools (ruff, black, isort, mypy)
- Testing framework (pytest + coverage plugins)
- Security scanners (bandit, pip-audit, safety)
- Performance profiling tools

### `Makefile` - Developer Workflow
- `make ci-local`: Full CI pipeline execution locally
- `make ci-fix`: Auto-fix formatting and linting
- `make format-check`: Verify formatting compliance
- `make lint-check`: Verify zero-warning policy
- `make test-coverage`: Enforce 100% coverage

## Performance Optimizations

### Dependency Caching
```yaml
- name: Cache pip dependencies
  uses: actions/cache@v4
  with:
    path: ~/.cache/pip
    key: pip-${{ runner.os }}-${{ hashFiles('**/requirements*.txt') }}
```

### Parallel Execution
- Code quality, tests, and security scans run independently
- Matrix strategy for cross-platform verification
- Fail-fast disabled only for platform matrix

### Timeout Management
- Global 10-minute timeout for all jobs
- Extended 30-minute timeout for nightly jobs only
- Per-test 10-minute timeout in pytest configuration

## Security Enforcement

### Zero-Tolerance Vulnerability Policy
```bash
# Any dependency vulnerability fails the build
pip-audit --format=json --output=pip-audit-report.json
if [ $? -ne 0 ]; then
  echo "❌ DEPENDENCY VULNERABILITIES DETECTED"
  exit 1
fi
```

### Python Security Scanning
```bash
# Zero tolerance for security issues
bandit -r . -f json -o bandit-report.json
if [ $? -ne 0 ]; then
  echo "❌ SECURITY VULNERABILITIES DETECTED"
  exit 1
fi
```

### Shell Script Security
```bash
# All shell scripts must pass shellcheck
while IFS= read -r -d '' script; do
  if ! shellcheck "$script"; then
    error_found=true
  fi
done < <(find . -name "*.sh" -type f -print0)
```

## Branch Protection Integration

### Required Status Checks
- Code Quality Enforcement
- Test Suite (100% Coverage)
- Security Vulnerability Scan
- Architecture Guardrails Validation
- Cross-Platform Verification (Linux + macOS)

### Merge Requirements
- All status checks must pass
- Branch must be up to date
- No administrator bypass allowed
- Draft PRs automatically blocked

## Developer Experience

### Local Development Workflow
1. **Setup**: `make dev-setup`
2. **Development**: `make ci-fix` (auto-fixes issues)
3. **Verification**: `make ci-local` (full pipeline)
4. **Pre-commit**: Automated with `make pre-commit`
5. **Pre-push**: Validation with `make pre-push`

### IDE Integration Support
- All configurations compatible with VS Code, PyCharm
- Real-time linting and formatting feedback
- Type checking integration
- Coverage highlighting

## Monitoring and Reporting

### Artifact Collection
- Coverage reports (HTML + XML)
- Security scan results
- Platform verification reports
- Consolidated CI summary

### Failure Diagnostics
- Detailed error messages for each violation type
- Suggested remediation commands
- Link to specific line numbers and files
- Clear failure categorization

## Enforcement Verification

### Dry Run Commands
```bash
# Validate configuration without applying
make format-check     # Verify formatting
make lint-check      # Verify linting
make type-check      # Verify types
make test-coverage   # Verify 100% coverage
make security        # Verify security
```

### Apply Commands
```bash
make ci-fix          # Auto-fix all issues
make ci-local        # Run full pipeline locally
```

### Rollback Strategy
```bash
# Revert problematic commits
git revert <commit-hash>

# Reset formatting
git checkout HEAD -- .

# Disable specific rules temporarily (NOT RECOMMENDED)
# Edit pyproject.toml [tool.ruff.ignore] section
```

## Dependencies
- **GitHub Actions**: Workflow execution platform
- **Python 3.12**: Runtime environment
- **Development tools**: Listed in requirements-dev.txt
- **System tools**: shellcheck, curl, git
- **Secrets**: None required (uses GitHub tokens)

## Security Checklist
- [x] No hardcoded secrets in configuration
- [x] Least-privilege GitHub token usage
- [x] Dependency versions pinned in requirements-dev.txt
- [x] Security scanning integrated in pipeline
- [x] Branch protection rules enforced
- [x] Merge gate validation required

## Compliance Statement
This CI enforcement system implements a **ZERO-TOLERANCE POLICY** for:
- Code formatting violations (Black/isort)
- Linting warnings (Ruff with ALL rules)
- Type checking errors (mypy strict mode)
- Test coverage below 100% (pytest-cov)
- Security vulnerabilities (bandit/pip-audit)
- Shell script issues (shellcheck)
- Architecture violations (custom rules)

**ANY VIOLATION FAILS THE BUILD** - No exceptions, no `|| true` on security-critical steps.

---
*Engineered with precision by the Euxis Fleet - Principal Automation Engineer*
*Version: 0.1.0 | Last Updated: 2026-02-09*