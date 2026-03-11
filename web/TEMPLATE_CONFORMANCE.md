# Template Conformance: euxis-web

- Kind: `node`
- Strict naming: `enabled`
- Platform targets: `macOS`, `Linux`, `WSL`

## Required Files

- package.json, README.md

## Documentation

- Module docs page: `euxis-data/docs/modules/euxis-web.md`

## Tests

- Test paths: `euxis-web/src/crypto-lib/tests`

## Core vs Platform Separation

- Core paths: `src/euxis_web/core`
- Platform paths: `src/euxis_web/platform`
- Rule: Core logic must depend on contracts, not concrete OS adapters.
