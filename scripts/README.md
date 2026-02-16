# Scripts

Scripts provides developer and automation utilities for Euxis.

## Purpose
This module contains non-runtime tooling such as version sync, documentation checks, stress testing, and performance baselines. It does not ship with production runtime services.

## Structure
- `check-version-consistency.sh` — validates version alignment
- `perf-baseline.sh` — performance baseline harness
- `stress-test.sh` — stress testing harness
- `sync-docs.sh` — docs synchronization helper
- `sync-versions.sh` — version sync helper
- `test-docs-as-code.sh` — documentation validation runner

## Dependencies
- `core/` — shared shell helpers
- `config/` — manifests and schemas

## Usage
```bash
scripts/perf-baseline.sh
scripts/stress-test.sh
scripts/sync-docs.sh
```

## Development
```bash
# Run documentation validation
scripts/test-docs-as-code.sh
```

## Testing
Scripts are exercised via CI and `euxis-certify` gates.

## API / Exports
This module exports runnable shell scripts in `scripts/`.
