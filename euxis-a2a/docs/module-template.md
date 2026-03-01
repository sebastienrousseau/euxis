# euxis-a2a

## Purpose

State package purpose in one sentence.

## Architecture

- `core/`: platform-agnostic domain and orchestration logic.
- `platform/`: adapters for `macOS`, `Linux`, and `WSL`.
- `tests/unit/`: pure logic tests.
- `tests/platform/`: adapter contract tests.

## Public API

List exported entrypoints from `src/euxis_<package>/__init__.py`.

## Quality Gates

- Unit + platform tests pass.
- 100% coverage for critical modules.
- No direct imports from `core/` to concrete platform adapter modules.
