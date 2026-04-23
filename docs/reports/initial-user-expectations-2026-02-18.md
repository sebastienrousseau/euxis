# Initial User Expectations (February 18,)

This page captures the latest measured quality and performance snapshot so first-time users can set realistic expectations.

## Verified Metrics

### Provider unit test status

- Suite: `euxis-core/tests/bats/lib/providers.bats`
- Result: `53/53` tests passing
- Runtime (local): approximately `3.94s`

Command used:

```bash
bats --tap euxis-core/tests/bats/lib/providers.bats
```

### Provider shell function coverage gate

- Library: `euxis-bin/lib/providers.sh`
- Coverage method: shell function-name coverage matcher used by `run_shell_tests.sh`
- Result: `100%` (`63/63` functions referenced by provider tests)

### Adjacent shell suite stabilization status

- `euxis-core/tests/bats/lib/common.bats`: `29/30` passing
- `euxis-core/tests/bats/lib/session.bats`: `30/34` passing

These are active hardening targets and are documented here to avoid overpromising green status outside the provider-focused path.

## Coverage Policy

Python coverage policy remains enforced in the repository configuration:

- File: `pyproject.toml`
- Setting: `fail_under = 95`

This is the target quality bar for Python coverage.

## What Early Users Should Expect

- Provider-first workflows are the most stable path today.
- Some shell test surfaces are still under active stabilization.
- Docs and implementation are being updated together; status values are date-stamped and should be treated as point-in-time measurements.

## Recommended First Validation

Run these checks before relying on a new environment:

```bash
bats --tap euxis-core/tests/bats/lib/providers.bats
bats --tap euxis-core/tests/bats/lib/common.bats
bats --tap euxis-core/tests/bats/lib/session.bats
```

If your goal is provider reliability only, prioritize the first command and ensure it is fully green.
