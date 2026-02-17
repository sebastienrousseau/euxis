# Dependency Graph


This is a static import graph derived from Python source (excluding tests).

Files scanned: 66

## Module Dependencies

- `adapters` -> packages/shared
- `api` -> adapters, packages/shared
- `cli` -> (none)
- `core` -> (none)
- `crypto` -> core
- `metrics` -> (none)
- `ui` -> crypto
- `config` -> (none)
- `security` -> (none)
- `packages/shared` -> (none)

## Notes

- This report only includes Python imports and ignores runtime shell calls.
- If a dependency is required by shell scripts, add it explicitly to module docs.