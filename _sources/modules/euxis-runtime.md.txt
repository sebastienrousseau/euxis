# euxis-runtime

`euxis-runtime` provides runtime telemetry, performance streams, and execution data used by release gates and orchestration quality checks.

## Focus Areas

- Speed and latency governance via `euxis-data/perf/metrics.jsonl`
- Low memory and CPU policy enforcement through package-level resource governance
- Portability through adapter-driven runtime abstractions in `euxis-core`
- Security and resilience support through signed evidence + runtime isolation contracts

## Quality Controls

- Package policy: `euxis-ops/perf/package_resource_policy.json`
- Governance validation: `euxis-ops/perf/validate_package_resource_governance.py`
- Package excellence gate: `euxis-ops/quality/validate_package_excellence.py`
