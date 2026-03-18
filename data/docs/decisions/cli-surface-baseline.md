# CLI Surface Baseline

**Date**: 2026-03-15
**Version**: v0.0.10
**Status**: Frozen

## Command Surface

### Core Commands (6)

| Command | Handler | Delegates to |
|---------|---------|-------------|
| `check [target]` | `cmd_check` | `playbook verify-everything --mode standard` |
| `triage [target]` | `cmd_triage` | `playbook verify-everything --mode flash` |
| `review [target]` | `cmd_review` | `playbook verify-everything --mode standard` |
| `compare <target>` | `cmd_compare` | `playbook verify-everything --compare` |
| `stats` | `cmd_stats` | `playbook --stats` |
| `policy <sub>` | `cmd_policy` | standalone (show/validate/check) |

### Aliases (6)

| Alias | Resolves to |
|-------|-------------|
| `quick` | `triage` |
| `deep` | `review` |
| `diag` | `doctor` |
| `metrics` | `stats` |
| `pb` | `playbook` |
| `verify-all` | `check` |

### Collision Avoidance

- `verify` (System) — unchanged, "Verify agent prompt integrity"
- `audit` (Specialized) — unchanged, "Security and compliance audit"
- No existing commands renamed or removed

## Help Group Order

1. Core
2. System
3. Fleet
4. Knowledge
5. Infrastructure
6. Development
7. Specialized

## Test Coverage

- 961 tests, all passing
- 30 surface command tests (`test_cmd_surface.cpp`)
- 6 alias resolution tests
- 9 help smoke tests
- 3 shell completion scripts (bash, zsh, fish)

## Files

| File | Role |
|------|------|
| `apps/cli/include/euxis/cli/cmd/surface.hpp` | Header |
| `apps/cli/src/cmd/surface.cpp` | Implementation (~280 lines) |
| `apps/cli/tests/test_cmd_surface.cpp` | Tests (~450 lines) |
| `data/config/completions/euxis.{bash,zsh,fish}` | Shell completions |

## Rules

- Adding a new Core command requires updating: surface.hpp, surface.cpp, engine.cpp, test_cmd_surface.cpp, all 3 completions, cli-reference.md
- Aliases must not shadow registered command names (except `verify-all` which was intentional)
- `compare` requires an explicit target (exit 2 if omitted)
- `policy check [target]` with target delegates to `cmd_check --policy`; without target checks latest artifact
