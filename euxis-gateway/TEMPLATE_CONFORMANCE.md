# Template Conformance: euxis-gateway

- Kind: `python`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- pyproject.toml, README.md

## Documentation

- Module docs page: `euxis-docs/docs/modules/euxis-gateway.md`

## Tests

- Test paths: `euxis-gateway/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_gateway/core`
- Platform paths: `src/euxis_gateway/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
