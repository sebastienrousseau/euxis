# Template Conformance: euxis-core-cpp

- Kind: `cpp-library`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- CMakeLists.txt

## Documentation

- Module docs page: `docs/modules/euxis-core-cpp.md`

## Tests

- Test paths: `libs/core/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_core_cpp/core`
- Platform paths: `src/euxis_core_cpp/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
