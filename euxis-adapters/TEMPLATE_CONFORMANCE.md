# Template Conformance: euxis-adapters

- Kind: `python`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- pyproject.toml, README.md

## Documentation

- Module docs page: `euxis-docs/docs/modules/euxis-adapters.md`

## Tests

- Test paths: `euxis-adapters/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_adapters/core`
- Platform paths: `src/euxis_adapters/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
