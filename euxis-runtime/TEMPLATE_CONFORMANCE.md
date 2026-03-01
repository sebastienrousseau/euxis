# Template Conformance: euxis-runtime

- Kind: `runtime-data`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- README.md, data/perf/metrics.jsonl

## Documentation

- Module docs page: `euxis-docs/docs/modules/euxis-runtime.md`

## Tests

- Test paths: `(none)`

## Core vs Platform Separation

- Core paths: `src/euxis_runtime/core`
- Platform paths: `src/euxis_runtime/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
