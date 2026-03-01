# Template Conformance: euxis-tui

- Kind: `python-rust`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- pyproject.toml, Cargo.toml, README.md

## Documentation

- Module docs page: `euxis-docs/docs/modules/euxis-tui.md`

## Tests

- Test paths: `euxis-tui/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_tui/core`
- Platform paths: `src/euxis_tui/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
