# Multi-Repo Workflow

This workflow supports a gradual move from the Euxis monorepo to multiple repos without disrupting daily development.

## Goals
- Keep local development fast with side-by-side repos.
- Preserve safe defaults while repo split work is in flight.
- Make path-based dependencies explicit during development.

## Local Layout
Recommended local layout (one folder per repo):

```
~/dev/euxis/
  euxis-core/
  euxis-cli/
  euxis-gateway/
  euxis-adapters/
  euxis-tui/
  euxis-security/
  euxis-metrics/
  euxis-docs/
  euxis-crypto-lib/
  euxis-crypto-packages/
```

## Setup
1. Copy `config/multi-repo.example.json` to `config/multi-repo.json` and fill in repo URLs.
2. Run:

```bash
scripts/setup/validate-multi-repo-config.sh
scripts/setup/multi-repo-dev.sh
```

This creates the layout above and clones each repo into it.
The validator enforces a strict allowlist of expected repo names.

## Path Dependencies (local dev)
Use path-based dependencies while iterating across repos:

- **Python**: `pip install -e ../euxis-core` from within a repo that depends on core.
- **Node**: use workspace linking (pnpm or npm workspaces) if present in the repo; otherwise `npm link` to local packages.
- **Shell**: keep `core/lib` and CLI repos adjacent and reference via relative paths.

## Worktree Alternative
If you prefer git worktrees:

```
scripts/setup/multi-repo-dev.sh --worktree --branch main
```

## Guardrails
- Keep the monorepo branch intact until split repos are stable.
- Treat `euxis-core` as the source of shared contracts.
- Add contract tests before moving tightly-coupled modules.
