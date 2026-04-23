# Template Conformance: euxis-security-cpp

- Kind: `cpp-library`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- CMakeLists.txt

## Documentation

- Module docs page: `docs/modules/euxis-security-cpp.md`

## Tests

- Test paths: `libs/security/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_security_cpp/core`
- Platform paths: `src/euxis_security_cpp/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
