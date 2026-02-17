# Crypto Lib (Python)

crypto_lib provides Python cryptography utilities used by Euxis tooling.

## Purpose
This module is a lightweight Python library for cryptographic primitives and key management. It is separate from the TypeScript packages in `packages/`.

## Structure
- `core.py` — core crypto helpers
- `exceptions.py` — crypto error types
- `key_management.py` — key handling routines
- `__init__.py` — public API exports

## Dependencies
No internal Euxis modules required.

## Usage
```python
from crypto_lib.core import encrypt, decrypt
```

## Development
Edit Python sources directly and run the Python tests in the root test suite.

## Testing
Tests live under `tests/` and run via `pytest`.

## API / Exports
Exports are defined in `crypto_lib/__init__.py`.
