# Template Conformance: euxis-crypto-cpp

- Kind: `cpp-library`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- CMakeLists.txt

## Documentation

- Module docs page: `data/docs/modules/euxis-crypto-cpp.md`

## Tests

- Test paths: `libs/crypto/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_crypto_cpp/core`
- Platform paths: `src/euxis_crypto_cpp/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
