# Template Conformance: euxis-bridge-cpp

- Kind: `cpp-library`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- CMakeLists.txt

## Documentation

- Module docs page: `data/docs/modules/euxis-bridge-cpp.md`

## Tests

- Test paths: `libs/bridge/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_bridge_cpp/core`
- Platform paths: `src/euxis_bridge_cpp/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
