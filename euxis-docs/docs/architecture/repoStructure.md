# Repository Structure Strategy

This document captures the target split for functional repositories to reduce coupling and simplify releases.

## Naming Convention

- Canonical package repository names must be exactly two words: `euxis-<singleword>`.
- Multi-word package suffixes are treated as legacy aliases during migration only.
- Workspace-only infrastructure directories are explicitly allowlisted by policy and are not package repos.

## Target Repos

- `euxis-core`
- `euxis-cli`
- `euxis-gateway`
- `euxis-metrics`
- `euxis-adapters`
- `euxis-security`
- `euxis-runtime`
- `euxis-scripts`
- `euxis-sdk`
- `euxis-docs`
- `euxis-cpp` (contains crypto, bridge, memory, identity, inference, a2a, bench, etx)

## Migration Notes

- Keep shared conventions in `.github/` and `config/templates/` and sync across repos.
- Extract one domain at a time to avoid large, breaking migrations.
- Publish versioned release notes for each repo as the split progresses.
- Keep workspace-level policy assets (e.g. `euxis-policy/gateway.json`) in a dedicated policy repo or bundled release asset set during split.
- Canonical migration mapping:
  - `euxis-sdk` -> `euxis-sdk`
