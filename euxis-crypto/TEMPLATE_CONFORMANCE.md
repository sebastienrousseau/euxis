# Template Conformance: euxis-crypto

- Kind: `python-rust`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- pyproject.toml, Cargo.toml, README.md

## Documentation

- Module docs page: `euxis-docs/docs/modules/euxis-crypto.md`

## Tests

- Test paths: `euxis-crypto/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_crypto/core`
- Platform paths: `src/euxis_crypto/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
