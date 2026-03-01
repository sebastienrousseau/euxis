# Template Conformance: euxis-docs

- Kind: `docs`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- README.md, pyproject.toml

## Documentation

- Module docs page: `euxis-docs/docs/modules/euxis-docs.md`

## Tests

- Test paths: `euxis-docs/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_docs/core`
- Platform paths: `src/euxis_docs/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
