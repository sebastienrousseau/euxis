# Euxis Library Architecture (`core/lib/`)

## Overview

Euxis v0.0.2 modularized the monolithic `euxis.sh` (~940 lines) into 6 focused library modules under `core/lib/`. The main script (`euxis-cli/bin/euxis.sh`) retains only bootstrapping, routing, and top-level orchestration (~400 lines). Each library owns a single domain and can be sourced independently by other scripts.

## Modules

| Module | Domain | Key Functions |
|--------|--------|---------------|
| `common.sh` | Logging, UI & Perf | `log_info`, `log_error`, `log_debug`, `log_warn`, `start_spinner`, `stop_spinner`, `_perf_start`, `_perf_elapsed_ms`, `_perf_check_budget`, `_perf_record` |
| `providers.sh` | Provider routing | `resolve_tiered_provider`, `resolve_provider_config`, `run_*`, `execute_provider` |
| `agents.sh` | Agent discovery, lifecycle & plugins | `resolve_agent_path`, `list_agents`, `agent_lifecycle_transition`, `agent_get_state`, `list_active_agents`, `count_active_agents`, `cleanup_stale_agents`, `register_agent_plugin`, `unregister_agent_plugin`, `list_plugins`, `agent_probe_liveness`, `agent_probe_readiness`, `agent_health_report` |
| `memory.sh` | Tiered memory, pruning & drift | `get_hot_memory`, `get_relevant_memory`, `get_cross_agent_memory`, `build_tiered_memory`, `_extract_keywords`, `prune_memory`, `prune_project_memory`, `detect_semantic_drift`, `resolve_memory_contradiction`, `auto_evolve_graph` |
| `session.sh` | Project/session | `get_project_name`, `get_session_id`, `ensure_project_dirs`, `get_memory_context` |
| `template.sh` | Variable substitution | `template_substitute`, `estimate_tokens` |
| `skill-detector.sh` | Auto-detection | `detect_project_domain`, `detect_project_type` |
| `prompt.sh` | Prompt assembly | `resolve_protocols`, `prepare_prompt`, `_proto_fingerprint`, `_get_fleet_roster` |

## Code Metrics

The framework core consists of:
- **8 library modules** in `core/lib/` (~870 LOC)
- **1 main entry point** `euxis-cli/bin/euxis.sh` (~400 LOC)
- **20+ CLI tools** in `euxis-cli/bin/` (dispatch, loop, council, bench, lint, etc.)
- **6 test files** in `tests/` (E2E, concurrency, integration)
- **11 validation patterns** in `config/patterns/`
- **35 agent prompts** in `agents/prompts/core/` and `agents/prompts/fleet/`
- **11 protocol files** in `agents/prompts/protocols/`
- **6 templates** in `config/templates/`
- **6 ADRs** in `docs/adr/`

Note: `.venv-voice/` contains a Python virtualenv for the voice interface feature. These ~17K files are third-party dependencies, not framework code. They are excluded via `.euxisignore` and should not be counted in framework metrics.

## Include Guard Pattern

Every lib file uses a guard to prevent double-sourcing:

```bash
#!/usr/bin/env bash
[[ -n "${_EUXIS_LIB_<NAME>:-}" ]] && return; _EUXIS_LIB_<NAME>=1

EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"
```

This ensures:
- Safe to source the same lib multiple times (idempotent)
- `EUXIS_HOME` always has a sane default regardless of load order
- No duplicate function definitions or variable resets

## Dependency Graph

```
euxis.sh
  ├── lib/common.sh            (no dependencies)
  ├── lib/providers.sh          → lib/common.sh
  ├── lib/agents.sh             (no dependencies)
  ├── lib/memory.sh             (no dependencies)
  ├── lib/session.sh            (no dependencies)
  ├── lib/template.sh           (no dependencies)
  ├── lib/skill-detector.sh     (no dependencies)
  └── lib/prompt.sh             → lib/agents.sh, lib/memory.sh, lib/template.sh
```

Internal dependencies are resolved via `EUXIS_HOME`:

```bash
source "${EUXIS_HOME}/euxis-core/lib/common.sh"
```

## How euxis.sh Sources Libraries

```bash
EUXIS_HOME="${HOME}/.euxis"

source "${EUXIS_HOME}/euxis-core/lib/common.sh"
source "${EUXIS_HOME}/euxis-core/lib/providers.sh"
# ... etc
```

All paths are anchored to `EUXIS_HOME` for portability. This works correctly whether `euxis` is invoked via `~/.euxis/euxis-cli/bin/euxis.sh`, a hardlink at `~/.euxis/euxis-cli/bin/euxis`, or any other location.

## Extension Guidelines

### Adding a New Library Module

1. Create `core/lib/<name>.sh` with the guard pattern:
   ```bash
   #!/usr/bin/env bash
   [[ -n "${_EUXIS_LIB_<NAME>:-}" ]] && return; _EUXIS_LIB_<NAME>=1
   EUXIS_HOME="${EUXIS_HOME:-${HOME}/.euxis}"
   ```

2. Source any dependencies at the top:
   ```bash
   source "${EUXIS_HOME}/euxis-core/lib/common.sh"
   ```

3. Add a `source` line in `euxis-cli/bin/euxis.sh` (after existing sources).

4. Set permissions: `chmod 755 core/lib/<name>.sh`

5. Run `bash -n euxis-cli/bin/euxis.sh` to verify syntax.

### Rules

- **One domain per module.** Don't add provider logic to `memory.sh`.
- **No circular dependencies.** If A sources B, B must not source A.
- **Always use the guard.** Other scripts (dispatch, loop, combo) may source the same libs.
- **Keep the interface stable.** Function signatures are the API — changing them breaks callers.
- **Export nothing.** Libraries define functions, not exported variables. Callers set their own state.

---

*Euxis v0.0.2 · Build something that matters.*
