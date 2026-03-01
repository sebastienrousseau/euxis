# Template Conformance: euxis-security

- Kind: `python`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- pyproject.toml, README.md

## Documentation

- Module docs page: `euxis-docs/docs/modules/euxis-security.md`

## Tests

- Test paths: `euxis-security/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_security/core`
- Platform paths: `src/euxis_security/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
