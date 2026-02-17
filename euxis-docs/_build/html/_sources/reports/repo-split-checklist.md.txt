# Repo Split Checklist (v0.1.0)

## Preparation
- [x] Freeze releases or use a dedicated split branch (`feat/v0.1.0`)
- [x] Document current CI workflows and release steps (see `docs/reports/ci-workflows-documentation.md`)
- [x] Run `cli/bin/euxis-certify` (all 6 gates passed)
- [x] Confirm all module READMEs are current (template-based, extraction-ready)

## Shared Contracts
- [x] Extract shared schemas into `euxis-core` (see `docs/architecture/SHARED_CONTRACTS.md`)
- [x] Define Gateway ↔ Adapters interface tests (see `tests/contracts/test_adapter_registry.py`)
- [x] Define CLI ↔ Core contract tests (documented in `SHARED_CONTRACTS.md`)
- [x] Define TUI ↔ Core contract tests (documented in `SHARED_CONTRACTS.md`)
- [x] Define Metrics schema stability tests (see `tests/contracts/test_metrics_schema.py`)

## Repo Creation
- [x] Create empty repos with CI skeletons (see `extracted-repos/`)
- [x] Add README + CONTRIBUTING + CHANGELOG
- [x] Add LICENSE where required (MIT)

## Extraction (order)
1. [x] `euxis-core` (extracted to `extracted-repos/euxis-core/`)
2. [x] `euxis-cli` (extracted to `extracted-repos/euxis-cli/`)
3. [ ] `euxis-security`
4. [ ] `euxis-gateway`
5. [ ] `euxis-adapters`
6. [ ] `euxis-tui`
7. [ ] `euxis-metrics`
8. [ ] `euxis-docs`
9. [ ] `euxis-crypto-lib`
10. [ ] `euxis-crypto-packages`

## Verification
- [ ] Each repo builds and tests independently
- [ ] Gateway health check passes
- [ ] CLI smoke tests pass
- [ ] Docs build passes

## Local Multi-Repo Workflow
- [x] Publish `docs/workflows/multi-repo-workflow.md`
- [x] Provide `config/multi-repo.example.json`
- [x] Provide `scripts/setup/multi-repo-dev.sh`

## Rollback
- [ ] Keep monorepo branch intact until all repos are stable
- [ ] Define stop conditions for rollback
