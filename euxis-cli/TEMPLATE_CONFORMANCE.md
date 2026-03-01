# Template Conformance: euxis-cli

- Kind: `python`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- pyproject.toml, README.md

## Documentation

- Module docs page: `euxis-docs/docs/modules/euxis-cli.md`

## Tests

- Test paths: `euxis-cli/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_cli/core`
- Platform paths: `src/euxis_cli/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
