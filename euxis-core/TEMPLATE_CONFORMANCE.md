# Template Conformance: euxis-core

- Kind: `python`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- pyproject.toml, README.md

## Documentation

- Module docs page: `euxis-docs/docs/modules/euxis-core.md`

## Tests

- Test paths: `euxis-core/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_core/core`
- Platform paths: `src/euxis_core/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
