# Template Conformance: euxis-adapters-cpp

- Kind: `cpp-library`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- CMakeLists.txt

## Documentation

- Module docs page: `euxis-data/docs/modules/euxis-adapters-cpp.md`

## Tests

- Test paths: `euxis-cpp/euxis-adapters-cpp/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_adapters_cpp/core`
- Platform paths: `src/euxis_adapters_cpp/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
