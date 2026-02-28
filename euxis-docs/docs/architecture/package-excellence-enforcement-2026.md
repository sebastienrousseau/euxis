# Package Excellence Enforcement (2026)

This document defines repository-wide enforcement of speed, performance, memory, CPU, portability, security, usability, UX, functionality, and quality for every Euxis package.

## Scope

All workspace packages are covered:

- `euxis-core`
- `euxis-cli`
- `euxis-gateway`
- `euxis-tui`
- `euxis-metrics`
- `euxis-adapters`
- `euxis-security`
- `euxis-crypto-lib`
- `euxis-runtime`
- `euxis-scripts`
- `euxis-sdk-rust`
- `euxis-crypto-packages`
- `euxis-docs`

## Enforcement Artifacts

- Package standards manifest: `scripts/quality/package_standards.json`
- Package resource policy: `scripts/perf/package_resource_policy.json`
- Resource governance validator: `scripts/perf/validate_package_resource_governance.py`
- Package excellence validator: `scripts/quality/validate_package_excellence.py`
- Package excellence scorecard: `scripts/eval/package_excellence_scorecard.py`
- Package benchmark collector: `scripts/perf/collect_package_benchmarks.py`
- Package benchmark budget validator: `scripts/perf/validate_package_benchmark_budget.py`
- Package benchmark PR trend renderer: `scripts/perf/render_package_benchmark_trend.py`
- Package benchmark regression validator: `scripts/perf/validate_package_benchmark_regression.py`
- Package benchmark baseline proposal generator: `scripts/perf/propose_package_benchmark_baseline.py`
- Package benchmark baseline review gate: `scripts/release/review_package_benchmark_baseline.py`
- Package benchmark baseline governance validator: `scripts/perf/validate_package_benchmark_baseline_governance.py`
- Benchmark baseline dataset: `scripts/perf/package_benchmarks_baseline.json`
- Package structure matrix generator/checker: `scripts/quality/render_package_structure_matrix.py`
- Generated matrix doc: `euxis-docs/docs/architecture/package-structure-matrix.md`

## Resource Governance

Each package must define stage-based limits for:

- `p95_ms`
- `memory_mb`
- `cpu_pct`

Ratchet policy is enforced as:

- `current >= target_q2_2026 >= target_q4_2026`

## Local Commands

```bash
make package-resource-governance-check
make package-excellence-check
make package-excellence-scorecard
make package-structure-matrix
make package-structure-matrix-check
make package-structure-matrix-report
make package-bench-collect
make package-bench-gate
make package-bench-regression-gate
make package-bench-baseline-propose
make package-bench-baseline-review
make package-bench-baseline-governance-check
make gate-all
```

## CI Integration

`make gate-all` now includes package governance and package excellence checks to keep quality and performance standards continuously enforced.
CI additionally publishes package benchmark datasets and PR warning-only trend summaries in `architecture-quality.yml`.
Push runs include a blocking package benchmark regression gate against the maintained baseline.
CI now also produces package benchmark baseline proposal/review artifacts and a bundle-consistency governance report for release traceability.
Baseline review artifacts are cryptographically linked to proposals via canonical `proposal_sha256`, and governance validation fails on hash mismatch.
Package structural consistency is also enforced through a deterministic generated matrix and stale-doc check in `gate-all`.
PR CI additionally publishes a matrix staleness summary and unified diff artifact for fast review when docs drift.
