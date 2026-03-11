# Core/Platform Separation Plan (2026)

This document tracks the implementation of the 2026 architecture hardening roadmap.

## Phase 1: Core Boundary Enforcement

- Core contracts are defined in `euxis-cpp/euxis-core-cpp/include/euxis/core/`.
  - Contract header: `euxis-cpp/euxis-core-cpp/include/euxis/core/contracts.hpp`
- Platform type adapters are co-located in the core header:
  - `euxis-cpp/euxis-core-cpp/include/euxis/core/types.hpp`
- Agent routing logic:
  - `euxis-cpp/euxis-core-cpp/src/router.cpp`
- Gateway transport is now HTTP-only (WebSocket adapter removed).
- CI guardrail: `euxis-ops/architecture/check_boundaries.py`.
- Guardrail blocks direct `subprocess`/network imports in pure core logic.
  - IO adapter allowlist: `platform/`, `runtime/`.
- Guardrail also blocks direct `asyncio` imports outside `runtime/`.

## Phase 2: Cross-platform parity

- GitHub Actions matrix for Linux/macOS: `.github/workflows/cpp.yml`.
- Platform normalization tests:
  - `euxis-cpp/euxis-core-cpp/tests/test_platform_adapter.cpp`
  - `euxis-cpp/euxis-core-cpp/tests/test_contracts.cpp`
  - `euxis-cpp/euxis-core-cpp/tests/test_router.cpp`
  - `euxis-cpp/euxis-core-cpp/tests/test_swarm.cpp`
  - Includes OS mapping and WSL environment detection coverage.

## Phase 3: Supply chain integrity

- SBOM generation (SPDX JSON), provenance, and keyless signature:
- SBOM generation (SPDX JSON), provenance, and keyless signature:
  - `.github/workflows/supply-chain.yml`
- Artifacts produced:
  - `sbom.spdx.json`
  - `sbom.spdx.sig`
  - `sbom.spdx.pem`
- Release artifacts are also built, keyless-signed, and verified:
  - `release-artifacts/**/*.whl`
  - `release-artifacts/**/*.tar.gz`
  - paired `*.sig` and `*.pem` for each artifact
- Local verification command:
  - `make verify-signed-artifacts`
  - Verification script: `euxis-ops/supply_chain/verify_signed_artifacts.sh`
- CI now verifies uploaded signed artifacts in a separate job after artifact transfer.

## Phase 4: Resilience and performance

- Resilience primitives:
  - `euxis-cpp/euxis-core-cpp/src/resilience.cpp`
  - `euxis-cpp/euxis-core-cpp/tests/test_resilience.cpp`
- Runtime concurrency uses `std::jthread` (dedicated concurrency module removed):
  - `euxis-cpp/euxis-core-cpp/src/swarm.cpp`
- Performance budget gate:
  - `euxis-ops/perf/check_perf_budget.py`
  - Policy file: `euxis-ops/perf/perf_policy.json` (`current`, `target_q2_2026`, `target_q4_2026`)
- Governance gate:
  - `euxis-ops/perf/validate_perf_governance.py`
  - Validates policy schema, stage presence, monotonic ratcheting (`current >= q2 >= q4`), and baseline format.
- Warning-only trend reporting:
  - PR job evaluates `target_q2_2026` and posts deltas to the job summary without blocking merge.
  - Summary rendering script: `euxis-ops/perf/render_trend_summary.py`

## Phase 5: Quality/cost scorecard

- Scorecard generator:
  - `euxis-ops/eval/scorecard.py`
- Sample metrics:
  - `euxis-data/scorecard/metrics.sample.json`
- Gate output:
  - `euxis-data/scorecard/latest.json`
- Release checklist gate:
  - `euxis-ops/release/generate_checklist.py`
  - Fails if p95 regresses by >10% vs `euxis-ops/perf/release_baseline.json`.
- Baseline proposal helper:
  - `euxis-ops/perf/propose_release_baseline.py`
  - Generates `euxis-data/release/proposed-baseline.json` for next release review.
- Baseline proposal review:
  - `euxis-ops/release/review_baseline_proposal.py`
  - Flags suspicious baseline drift (default >20%) for manual review.
- Release evidence bundle:
  - `euxis-ops/release/aggregate_release_evidence.py`
  - Generates `euxis-data/release/release-evidence.json` as a single gate artifact.
- Release evidence validator:
  - `euxis-ops/release/validate_release_evidence.py`
  - Enforces minimum score thresholds and checklist/baseline-review integrity.
  - Optional strict mode requires signature status `verified`.
  - Supply-chain workflow runs strict mode after artifact signature verification.
- Phase completion validator:
  - `euxis-ops/release/validate_phase_completion.py`
  - Ensures required phase 1-5 implementation artifacts are present.
- 100% phase-critical code coverage gate:
  - `euxis-ops/quality/enforce_phase_code_coverage.py`
- 100% phase docs-coverage gate:
  - `euxis-ops/quality/validate_phase_docs_coverage.py`

## Local commands

```bash
make architecture-check
make perf-gate
make scorecard
make release-checklist
make propose-release-baseline
make perf-governance-check
make baseline-proposal-review
make release-evidence
make validate-release-evidence
make validate-release-evidence-strict
make phase-completion-check
make code-coverage-100
make docs-coverage-100
make package-resource-governance-check
make package-excellence-check
make package-excellence-scorecard
make gate-all
```

`make gate-all` is the canonical local/CI entrypoint for architecture + perf + scorecard gates.
