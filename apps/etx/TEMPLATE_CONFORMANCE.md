# Template Conformance: euxis-etx

- Kind: `cpp-application`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- CMakeLists.txt

## Documentation

- Module docs page: `data/docs/modules/euxis-etx.md`

## Tests

- Test paths: `(none)`

## Core vs Platform Separation

- Core paths: `src/euxis_etx/core`
- Platform paths: `src/euxis_etx/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
