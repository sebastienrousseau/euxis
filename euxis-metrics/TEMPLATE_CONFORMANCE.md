# Template Conformance: euxis-metrics

- Kind: `python`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- pyproject.toml, README.md

## Documentation

- Module docs page: `euxis-docs/docs/modules/euxis-metrics.md`

## Tests

- Test paths: `euxis-metrics/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_metrics/core`
- Platform paths: `src/euxis_metrics/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
