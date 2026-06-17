# Template Conformance: euxis-cli-cpp

- Kind: `cpp-application`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- CMakeLists.txt

## Documentation

- Module docs page: `docs/modules/euxis-cli-cpp.md`

## Tests

- Test paths: `apps/cli/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_cli_cpp/core`
- Platform paths: `src/euxis_cli_cpp/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
