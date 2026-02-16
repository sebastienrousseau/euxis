# Core

Shared shell libraries and common utilities used by Euxis CLI and infrastructure tools.

## Purpose
Core contains the reusable shell libraries that power routing, validation, prompt assembly, and dispatch across the CLI. It is not a runtime by itself; it provides shared functions to other modules.

## Structure
- `lib/` — Shell libraries (common, providers, agents, dispatch, validation)

## Dependencies
- None (core is foundational)

## Usage
Core libraries are sourced by CLI entrypoints:

```bash
source "$EUXIS_HOME/core/lib/common.sh"
```

## Development
```bash
# Lint shell libraries
bin/euxis-shell-lint
```

## Testing
```bash
# Shell library tests
bash tests/lib/test_registry.sh
```

## API / Exports
Shell functions defined in `core/lib/*.sh` and documented in `docs/reference/lib-architecture.md`.
