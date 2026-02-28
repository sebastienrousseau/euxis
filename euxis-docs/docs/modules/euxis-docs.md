# euxis-docs

`euxis-docs` is the documentation system for architecture, API references, operations, governance, and quality standards.

## Focus Areas

- Documentation quality and discoverability
- Clear operational runbooks and architectural boundaries
- Evidence-backed release and security narratives
- UX-oriented guidance for operators and developers

## Quality Controls

- Docs manifest: `euxis-docs/pyproject.toml`
- Package tests: `euxis-docs/tests/`
- Stable docs gate: `scripts/quality/run_docs_tests_stable.sh`
- Phase docs coverage gate: `scripts/quality/validate_phase_docs_coverage.py`
- Package excellence gate: `scripts/quality/validate_package_excellence.py`
