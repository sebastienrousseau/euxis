# Template Conformance: euxis-cli-cpp

- Kind: `cpp-application`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- CMakeLists.txt

## Documentation

- Module docs page: `data/docs/modules/euxis-cli-cpp.md`

## Tests

- Test paths: `euxis-cpp/euxis-cli-cpp/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_cli_cpp/core`
- Platform paths: `src/euxis_cli_cpp/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
