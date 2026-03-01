# euxis-scripts

`euxis-scripts` and top-level `euxis-ops/` automate quality, security, release, and governance workflows.

## Focus Areas

- Repository-wide enforcement for quality, security, and performance
- Deterministic release evidence generation and validation
- Fast feedback loops in CI/CD for resilience and portability regressions

## Quality Controls

- Package standards manifest: `euxis-ops/quality/package_standards.json`
- Package excellence validator: `euxis-ops/quality/validate_package_excellence.py`
- Package scorecard generator: `euxis-ops/eval/package_excellence_scorecard.py`
