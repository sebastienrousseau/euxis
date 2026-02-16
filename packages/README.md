# Packages

Packages contains standalone subprojects that can be split into separate repos.

## Purpose
This module houses Node/TypeScript packages that are built and versioned independently from the Python `crypto_lib/` helpers. It does not contain core Euxis runtime logic.

## Structure
- `crypto-lib/` — TypeScript crypto library
- `crypto-server/` — TypeScript crypto service

## Dependencies
These packages are self-contained and managed with their own `package.json` files.

## Usage
```bash
cd packages/crypto-lib
npm install
npm test
```

## Development
Each package owns its own build and test scripts.

## Testing
Run tests from inside each package directory.

## API / Exports
Exports are defined by each package's `package.json`.
