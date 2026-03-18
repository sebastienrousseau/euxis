# Template Conformance: euxis-identity-cpp

- Kind: `cpp-library`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- CMakeLists.txt

## Documentation

- Module docs page: `data/docs/modules/euxis-identity-cpp.md`

## Tests

- Test paths: `libs/identity/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_identity_cpp/core`
- Platform paths: `src/euxis_identity_cpp/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
