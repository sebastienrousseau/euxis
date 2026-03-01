# Template Conformance: euxis-scripts

- Kind: `ops`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- README.md

## Documentation

- Module docs page: `euxis-docs/docs/modules/euxis-scripts.md`

## Tests

- Test paths: `(none)`

## Core vs Platform Separation

- Core paths: `src/euxis_scripts/core`
- Platform paths: `src/euxis_scripts/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
