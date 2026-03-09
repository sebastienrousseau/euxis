# Template Conformance: euxis-metrics-cpp

- Kind: `cpp-library`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- CMakeLists.txt

## Documentation

- Module docs page: `euxis-data/docs/modules/euxis-metrics-cpp.md`

## Tests

- Test paths: `euxis-cpp/euxis-metrics-cpp/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_metrics_cpp/core`
- Platform paths: `src/euxis_metrics_cpp/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
