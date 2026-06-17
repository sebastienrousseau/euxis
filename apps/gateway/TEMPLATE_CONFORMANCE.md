# Template Conformance: euxis-gateway-cpp

- Kind: `cpp-library`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- CMakeLists.txt

## Documentation

- Module docs page: `docs/modules/euxis-gateway-cpp.md`

## Tests

- Test paths: `apps/gateway/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_gateway_cpp/core`
- Platform paths: `src/euxis_gateway_cpp/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
