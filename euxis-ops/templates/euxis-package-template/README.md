# Euxis Package Template (Strict 2026)

This template defines the canonical structure for a new `euxis-*` package with:

- strict core vs platform separation
- deterministic tests (unit + platform)
- module-level docs
- cross-platform support targets: macOS, Linux, WSL

## Copy and Rename

1. Copy this directory to your new package path, for example `euxis-<name>`.
2. Replace `euxis_template` with your module name (usually `euxis_<name>`).
3. Update `pyproject.toml` package metadata.
4. Add a module page at `euxis-docs/docs/modules/euxis-<name>.md`.
5. Register the package in `euxis-ops/quality/package_standards.json`.

## Canonical Structure

```text
<package>/
├── .github/workflows/ci.yml
├── docs/module-template.md
├── pyproject.toml
├── README.md
├── src/euxis_template/
│   ├── __init__.py
│   ├── core/
│   │   ├── __init__.py
│   │   └── service.py
│   └── platform/
│       ├── __init__.py
│       ├── contracts.py
│       ├── factory.py
│       ├── linux.py
│       ├── macos.py
│       └── wsl.py
└── tests/
    ├── platform/
    │   └── test_platform_adapters.py
    └── unit/
        ├── test_factory.py
        └── test_service.py
```

## Enforcement Rules

- `core/` must not import from `platform/*` concrete files.
- `core/` may depend only on `platform/contracts.py` protocol interfaces.
- platform-specific behavior lives only in `platform/{macos,linux,wsl}.py`.
- all package public APIs must be exported in `src/euxis_template/__init__.py`.
- tests must run with `-p no:cacheprovider` in strict mode.
