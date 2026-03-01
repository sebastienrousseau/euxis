# Repo Split Pilot: euxis-sdk

## Scope

Pilot extraction of `euxis-sdk` into a standalone repository while preserving workspace strict gates.

## Manifest

- Source manifest: `euxis-data/release/repo-split-manifests/euxis-sdk.json`

## Steps

1. Initialize new repository `euxis-sdk`.
2. Copy `euxis-sdk/` package directory contents as repository root.
3. Copy required governance files:
   - `LICENSE`
   - `NOTICE`
   - package-local `.euxis-template.json`
   - package-local `TEMPLATE_CONFORMANCE.md`
4. Add package CI with strict checks:
   - unit tests
   - coverage gates
   - template conformance
5. Publish package docs page and wire cross-repo docs links.

## Validation Checklist

- `cargo test --all-targets --all-features` passes.
- README/API/module docs are present.
- release and signature workflow is configured.
- no unresolved path dependencies to monorepo internals.

## Rollback

If the pilot fails gating, keep monorepo as source of truth and iterate on manifest boundaries.
