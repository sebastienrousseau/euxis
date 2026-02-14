# QUALITY-001: Code Quality Gates

## Pattern
Enforce minimum quality standards before code reaches production.

## Severity
HIGH

## Detection Rules
1. **Missing error handling:** Functions that can fail but don't handle errors
2. **Dead code:** Unreachable branches, unused imports, commented-out code blocks
3. **Magic numbers:** Unexplained numeric literals in business logic
4. **Inconsistent naming:** Mixed conventions (camelCase vs snake_case) within a module
5. **Missing validation:** Public API functions that don't validate inputs

## Validation
- [ ] All public functions have input validation
- [ ] Error paths are handled explicitly (no silent failures)
- [ ] No dead code or commented-out blocks
- [ ] Constants are named and documented
- [ ] Naming conventions are consistent per language

## Remediation
- Enforce linters in CI (shellcheck for bash, eslint for JS, ruff for Python)
- Use pre-commit hooks for automated checks
- Review coverage reports — aim for meaningful coverage, not line count
- Document public APIs with examples
