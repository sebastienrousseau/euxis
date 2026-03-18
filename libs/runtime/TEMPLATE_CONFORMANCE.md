# Template Conformance: euxis-runtime-cpp

- Kind: `cpp-library`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- CMakeLists.txt

## Documentation

- Module docs page: `data/docs/modules/euxis-runtime-cpp.md`

## Tests

- Test paths: `libs/runtime/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_runtime_cpp/core`
- Platform paths: `src/euxis_runtime_cpp/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
