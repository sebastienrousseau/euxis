# Euxis Validation Patterns

Validation patterns are structured detection rules that agents apply to verify code quality, security, performance, and correctness before producing final output.

## Pattern Catalog

| Pattern | Domain | Severity | Detection Rules |
|---------|--------|----------|-----------------|
| SECURITY-001 | Injection Vulnerabilities | CRITICAL | 4 |
| CONCURRENCY-001 | Race Conditions & Thread Safety | CRITICAL | 5 |
| PERF-001 | Hot Path Optimization | HIGH | 5 |
| ERROR-001 | Error Handling Strategy | HIGH | 5 |
| API-001 | API Design Consistency | HIGH | 5 |
| DEPENDENCY-001 | Dependency Management | HIGH | 5 |
| SCHEMA-001 | Data Schema Evolution | HIGH | 5 |
| QUALITY-001 | Code Quality Gates | HIGH | 5 |
| MEMORY-001 | Memory & Resource Management | HIGH | 5 |
| TEST-001 | Testing Best Practices | MEDIUM | 5 |
| LOG-001 | Observability Standards | MEDIUM | 5 |

**Total: 11 patterns, 54 detection rules**

## Severity Calibration

| Severity | Meaning | CI Gate | Response SLA |
|----------|---------|---------|-------------|
| **CRITICAL** | Blocks deployment. Data loss, security breach, or system crash risk. | Hard block — no override | Fix before merge |
| **HIGH** | Blocks PR merge. Correctness, reliability, or maintainability risk. | Soft block — tech lead override | Fix within sprint |
| **MEDIUM** | Should fix. Code health, readability, or best practice deviation. | Warning — no block | Fix when touching file |
| **LOW** | Nice to have. Style, convention, or minor improvement. | Info only | At developer discretion |

## Pattern Interaction Matrix

Some patterns interact or create tension. Resolution rules:

| Pattern A | Pattern B | Interaction | Resolution |
|-----------|-----------|-------------|------------|
| PERF-001 (minimize subprocesses) | MEMORY-001 (cleanup in traps) | Traps add overhead | Keep traps — correctness > speed |
| SECURITY-001 (sanitize input) | PERF-001 (minimize hot path work) | Sanitization adds latency | Sanitize at boundary, cache result |
| CONCURRENCY-001 (lock shared state) | PERF-001 (avoid contention) | Locks slow hot paths | Use lock-free structures for counters |
| ERROR-001 (catch all errors) | PERF-001 (minimize overhead) | Try/catch has cost | Only wrap I/O; trust internal code |
| TEST-001 (test edge cases) | PERF-001 (fast test suite) | More tests = slower CI | Parallelize; separate fast/slow suites |
| SCHEMA-001 (backward compat) | API-001 (clean design) | Compat adds fields | Use expand-contract migration |

**General precedence:** Security > Correctness > Reliability > Performance > Style

## Automated Verification

Each pattern maps to automated checks:

| Pattern | Verification Method |
|---------|-------------------|
| SECURITY-001 | `euxis audit` — security probes |
| CONCURRENCY-001 | Thread sanitizer (`-fsanitize=thread`), `go vet -race` |
| PERF-001 | `euxis bench` — performance benchmarks |
| ERROR-001 | Static analysis (eslint no-empty-catch, pylint bare-except) |
| API-001 | Contract tests against OpenAPI spec |
| DEPENDENCY-001 | `npm audit`, `pip-audit`, `cargo audit` in CI |
| SCHEMA-001 | Schema compatibility checker in migration pipeline |
| QUALITY-001 | `euxis lint` — static analysis, shellcheck |
| MEMORY-001 | Valgrind, heaptrack, or custom leak detection |
| TEST-001 | `euxis test` — mutation score + coverage |
| LOG-001 | Grep for PII patterns in log output |

## Pattern Versioning

- Each pattern carries a version (currently v1.0.0 for all)
- Minor versions add detection rules or validation items
- Major versions change severity, remove rules, or restructure
- Deprecated patterns are marked `## Status: DEPRECATED` with migration notes
- Pattern files follow naming: `<CATEGORY>-<NNN>.md` (uppercase, 3-digit number)

## Adding a New Pattern

1. Create `patterns/<CATEGORY>-<NNN>.md`
2. Include all required sections: Pattern, Severity, Detection Rules, Validation, Remediation
3. Run `euxis lint` to validate format
4. Add to the catalog table above
5. Map to automated verification if possible
