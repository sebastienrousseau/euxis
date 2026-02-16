# CLI

CLI provides the Euxis command entrypoints and hook tooling.

## Purpose
This module owns all user-facing `euxis-*` commands and helper scripts. It is responsible for orchestration entrypoints, diagnostics, and developer tooling. It does not own agent prompts or gateway runtime logic.

## Structure
- `bin/` — executable entrypoints (e.g., `euxis`, `euxis-lint`, `euxis-certify`)
- `bin/hooks/` — Git hooks managed by `euxis-hooks`

## Dependencies
- `core/` — shared shell libraries and validation helpers
- `agents/` — registry and prompt metadata
- `gateway/` — gateway launcher entrypoint

## Usage
```bash
cli/bin/euxis --help
cli/bin/euxis-lint
cli/bin/euxis-certify
```

## Development
```bash
# Lint CLI scripts
cli/bin/euxis-shell-lint

# Run certification gates
cli/bin/euxis-certify
```

## Testing
CLI is exercised by shell tests under `tests/` and documentation tests via `cli/bin/euxis-docs-test`.

## API / Exports
CLI exports executable scripts in `cli/bin/`.
