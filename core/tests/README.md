# Tests

Tests contains cross-module integration and verification suites.

## Purpose
This module validates system behavior across CLI, gateway, TUI, and infrastructure boundaries. It does not contain module-local tests that live alongside code.

## Structure
- `bats/` — Bats-core shell test libraries
- `coverage/` — coverage artifacts (generated)
- `lib/` — shared test helpers
- `unit/` — Python unit tests for TUI and utilities
- `run_shell_tests.sh` — shell test runner
- `test_*` — integration and verification tests

## Dependencies
- `core/` — shared shell helpers
- `cli/` — CLI entrypoints
- `gateway/` — gateway interfaces
- `tui/` — UI screens

## Usage
```bash
tests/run_shell_tests.sh
pytest -q
```

## Development
```bash
# Run a focused shell test
tests/run_shell_tests.sh --focus lint

# Run a focused pytest file
pytest tests/unit/test_app.py
```

## Testing
This module is the testing harness for the rest of the monorepo.

## API / Exports
This module exports test runners and helpers under `tests/`.
