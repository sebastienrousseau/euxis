# Template Conformance: euxis-memory-cpp

- Kind: `cpp-library`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- CMakeLists.txt

## Documentation

- Module docs page: `data/docs/modules/euxis-memory-cpp.md`

## Tests

- Test paths: `euxis-cpp/euxis-memory-cpp/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_memory_cpp/core`
- Platform paths: `src/euxis_memory_cpp/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
