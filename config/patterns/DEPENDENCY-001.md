# DEPENDENCY-001: Dependency Management

## Pattern
Enforce secure, minimal, and reproducible dependency management across all projects.

## Severity
HIGH

## Detection Rules
1. **Unpinned versions:** Dependencies specified with ranges (`^`, `~`, `>=`) in production lockfiles — pin exact versions for reproducible builds
2. **Known vulnerabilities:** Dependencies with published CVEs not updated within SLA (CRITICAL: 24h, HIGH: 7d, MEDIUM: 30d) — run `npm audit` / `pip-audit` / `cargo audit` in CI
3. **License violations:** Dependencies with incompatible licenses (GPL in proprietary code, AGPL in SaaS) — audit licenses with automated tooling before adoption
4. **Excessive transitive depth:** A single direct dependency pulling 100+ transitive dependencies — evaluate lighter alternatives or vendor critical functionality
5. **Abandoned dependencies:** Dependencies with no commits in 2+ years, no maintainer response to security issues, or deprecated status — plan migration or fork

## Validation
- [ ] All production dependencies pinned to exact versions in lockfile
- [ ] Vulnerability scanning runs in CI pipeline on every PR
- [ ] License compatibility verified for all direct dependencies
- [ ] Transitive dependency count reviewed quarterly; outliers investigated
- [ ] No abandoned dependencies in critical paths (auth, crypto, networking)

## Remediation
- Use lockfiles (`package-lock.json`, `poetry.lock`, `Cargo.lock`) and commit them
- Enable automated dependency updates (Dependabot, Renovate) with auto-merge for patch versions
- Run license audits with `license-checker`, `pip-licenses`, or `cargo-deny`
- Prefer standard library over third-party for simple operations
- Maintain an internal dependency allowlist for security-critical projects
