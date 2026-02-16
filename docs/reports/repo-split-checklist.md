# Repo Split Checklist (v0.1.0)

## Preparation
- [ ] Freeze releases or use a dedicated split branch
- [ ] Document current CI workflows and release steps
- [ ] Run `cli/bin/euxis-certify`
- [ ] Confirm all module READMEs are current

## Shared Contracts
- [ ] Extract shared schemas into `euxis-core`
- [ ] Define Gateway ↔ Adapters interface tests
- [ ] Define CLI ↔ Core contract tests
- [ ] Define TUI ↔ Core contract tests
- [ ] Define Metrics schema stability tests

## Repo Creation
- [ ] Create empty repos with CI skeletons
- [ ] Add README + CONTRIBUTING + CHANGELOG
- [ ] Add LICENSE where required

## Extraction (order)
1. [ ] `euxis-core`
2. [ ] `euxis-cli`
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

## Rollback
- [ ] Keep monorepo branch intact until all repos are stable
- [ ] Define stop conditions for rollback
