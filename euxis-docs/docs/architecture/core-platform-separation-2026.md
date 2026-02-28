# Core/Platform Separation Plan (2026)

This document tracks the implementation of the 2026 architecture hardening roadmap.

## Phase 1: Core Boundary Enforcement

- Core contracts are defined in `euxis-core/src/euxis_core/contracts/`.
  - Port contract file: `euxis-core/src/euxis_core/contracts/ports.py`
- Platform adapters are isolated in `euxis-core/src/euxis_core/platform/`.
  - Primary adapter implementation: `euxis-core/src/euxis_core/platform/adapters.py`
- Runtime adapter is isolated in `euxis-core/src/euxis_core/runtime/`.
  - Gateway transport adapter: `euxis-core/src/euxis_core/runtime/gateway_ws.py`
- CI guardrail: `scripts/architecture/check_boundaries.py`.
- Guardrail blocks direct `subprocess`/network imports in pure core logic.
  - IO adapter allowlist: `platform/`, `runtime/`.
- Guardrail also blocks direct `asyncio` imports outside `runtime/`.

## Phase 2: Cross-platform parity

- GitHub Actions matrix for Linux/macOS/Windows: `.github/workflows/cross-platform-ci.yml`.
- Platform normalization tests:
  - `euxis-core/tests/unit/test_platform_adapter.py`
  - `euxis-core/tests/unit/test_contracts.py`
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
  - Verification script: `scripts/supply_chain/verify_signed_artifacts.sh`
- CI now verifies uploaded signed artifacts in a separate job after artifact transfer.

## Phase 4: Resilience and performance

- Resilience primitives:
  - `euxis-core/src/euxis_core/resilience.py`
  - `euxis-core/tests/unit/test_resilience.py`
- Runtime async resilience now guards gateway connection setup:
  - `euxis-core/src/euxis_core/runtime/concurrency.py`
  - `euxis-core/src/euxis_core/mesh/swarm.py`
- Performance budget gate:
  - `scripts/perf/check_perf_budget.py`
  - Policy file: `scripts/perf/perf_policy.json` (`current`, `target_q2_2026`, `target_q4_2026`)
- Governance gate:
  - `scripts/perf/validate_perf_governance.py`
  - Validates policy schema, stage presence, monotonic ratcheting (`current >= q2 >= q4`), and baseline format.
- Warning-only trend reporting:
  - PR job evaluates `target_q2_2026` and posts deltas to the job summary without blocking merge.
  - Summary rendering script: `scripts/perf/render_trend_summary.py`

## Phase 5: Quality/cost scorecard

- Scorecard generator:
  - `scripts/eval/scorecard.py`
- Sample metrics:
  - `data/scorecard/metrics.sample.json`
- Gate output:
  - `data/scorecard/latest.json`
- Release checklist gate:
  - `scripts/release/generate_checklist.py`
  - Fails if p95 regresses by >10% vs `scripts/perf/release_baseline.json`.
- Baseline proposal helper:
  - `scripts/perf/propose_release_baseline.py`
  - Generates `data/release/proposed-baseline.json` for next release review.
- Baseline proposal review:
  - `scripts/release/review_baseline_proposal.py`
  - Flags suspicious baseline drift (default >20%) for manual review.
- Release evidence bundle:
  - `scripts/release/aggregate_release_evidence.py`
  - Generates `data/release/release-evidence.json` as a single gate artifact.
- Release evidence validator:
  - `scripts/release/validate_release_evidence.py`
  - Enforces minimum score thresholds and checklist/baseline-review integrity.
  - Optional strict mode requires signature status `verified`.
  - Supply-chain workflow runs strict mode after artifact signature verification.
- Phase completion validator:
  - `scripts/release/validate_phase_completion.py`
  - Ensures required phase 1-5 implementation artifacts are present.
- 100% phase-critical code coverage gate:
  - `scripts/quality/enforce_phase_code_coverage.py`
- 100% phase docs-coverage gate:
  - `scripts/quality/validate_phase_docs_coverage.py`

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
