# Template Conformance: euxis-a2a-cpp

- Kind: `cpp-library`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- CMakeLists.txt

## Documentation

- Module docs page: `docs/modules/euxis-a2a-cpp.md`

## Tests

- Test paths: `libs/a2a/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_a2a_cpp/core`
- Platform paths: `src/euxis_a2a_cpp/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
