# Template Conformance: euxis-sdk

- Kind: `rust`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- Cargo.toml, README.md

## Documentation

- Module docs page: `euxis-docs/docs/modules/euxis-sdk.md`

## Tests

- Test paths: `euxis-sdk/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_sdk/core`
- Platform paths: `src/euxis_sdk/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
