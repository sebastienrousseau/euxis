# Euxis Monorepo Reorg Plan (v0.0.2)

## Scope
Reorganize the monorepo into modular, self-contained top-level folders with explicit boundaries, co-located tests, and module READMEs. No repo splitting or build tool changes.

---

## Phase 1 — Current Structure Map

### Top-level directories and purpose
- `euxis-cli/bin/`: CLI entrypoints and shell/Python tooling (`euxis-*`, lint, certify, hooks, lib/).
- `config/`: config schemas, templates, branding, playbooks, patterns.
- `crypto/src/crypto_lib/`: Python crypto library code.
- `euxis-runtime/data/`: runtime data (bus, lifecycle, perf, registry pool).
- `deploy/`: Dockerfiles and deployment compose files.
- `docs/`: documentation, reference, ADRs, guides, reports.
- `metrics/src/metrics/`: metrics collection, aggregation, verification, dashboard schemas.
- `packages/`: sub-packages (`crypto-lib`, `crypto-server`).
- `agents/prompts/`: agent prompts and protocols.
- `euxis-ops/`: gateway server + adapters + tooling scripts.
- `euxis-policy/`: approval policies, allowlists, audit policy defaults.
- `tests/`: test suites for infra, TUI, CLI, gateway, etc.
- `ui/src/tui/`: Textual-based ETX TUI app.

### Multipurpose directories
- `euxis-ops/` mixes Gateway runtime, adapters, demos, and tooling.
- `euxis-cli/bin/` mixes user-facing commands with internal helpers.
- `packages/` overlaps with `crypto/src/crypto_lib/` (duplication risk).

---

## Phase 2 — Proposed Modular Structure

```
core/        # Shared types, constants, utilities
agents/      # Agent prompts, squads, combos, playbooks
api/src/gateway/     # Gateway server, adapters, protocol utilities
cli/         # Command-line entrypoints and shell libs
ui/src/tui/         # ETX Textual UI
adapters/src/adapters/    # Channel adapters (Slack, Telegram, WebChat)
euxis-runtime/memory/      # Cortex + memory persistence
euxis-policy/    # Approvals, allowlists, audit, Guard logic
metrics/src/metrics/     # Metrics collection, aggregation, dashboards
config/      # Config schemas and defaults
docs/        # Docs (kept as-is)
euxis-ops/     # Build/dev tooling (non-runtime)
tests/       # Cross-module integration tests only
```

---

## Phase 3 — Module Definitions

### `core/`
- **Purpose**: Shared primitives and helpers used across modules.
- **Contains**: `core/lib/`, common validation utils, shared constants.
- **Depends on**: none.
- **Exports**: helpers, types.
- **Independent**: not yet (depends on repo layout).

### `agents/`
- **Purpose**: Agent definitions, prompts, squads, combos.
- **Contains**: `agents/prompts/`, `agents/squads.json`, `agents/registry.json`.
- **Depends on**: `core/`, `config/`.

### `api/src/gateway/`
- **Purpose**: WebSocket control plane and HTTP endpoints.
- **Contains**: `api/src/gateway/server.py`, `api/src/gateway/utils.py`, gateway schemas, `api/src/gateway/webchat/`.
- **Depends on**: `core/`, `euxis-policy/`, `config/`.

### `cli/`
- **Purpose**: User entrypoints and shell tooling.
- **Contains**: `euxis-cli/bin/euxis*`, `core/lib/`.
- **Depends on**: `core/`, `agents/`, `api/src/gateway/`.

### `ui/src/tui/`
- **Purpose**: ETX Textual UI.
- **Contains**: `ui/src/tui/` app and screens.
- **Depends on**: `core/`, `agents/`, `metrics/src/metrics/`.

### `adapters/src/adapters/`
- **Purpose**: Channel adapter implementations.
- **Contains**: `adapters/src/adapters/*`.
- **Depends on**: `api/src/gateway/`, `core/`.

### `euxis-runtime/memory/`
- **Purpose**: Cortex storage and memory flows.
- **Contains**: `euxis-runtime/memory/cortex/` schema + related tools.
- **Depends on**: `core/`.

### `euxis-policy/`
- **Purpose**: approvals, allowlists, audit trail.
- **Contains**: approval persistence, allowlist configs.
- **Depends on**: `core/`, `config/`.

### `metrics/src/metrics/`
- **Purpose**: Metrics pipeline.
- **Contains**: `metrics/src/metrics/`.
- **Depends on**: `core/`.

---

## Phase 4 — Move Manifest (High-Level)

- `euxis-cli/bin/*` → `euxis-cli/bin/*`
- `core/lib/*` → `core/lib/*`
- `prompts/*` → `agents/prompts/*`
- `agents/registry.json`, `agents/registry.db`, `agents/squads.json` → `agents/`
- `euxis-ops/gateway_*` → `api/src/gateway/`
- `adapters/src/adapters/*` → `adapters/src/adapters/`
- `config/gateway.json` → `euxis-policy/gateway.json`
- `euxis-runtime/data/cortex/*` → `euxis-runtime/memory/cortex/*`
- `api/src/gateway/webchat/*` → `api/src/gateway/webchat/*`
- `api/src/gateway/utils.py` → `api/src/gateway/utils.py`
- `api/src/gateway/server.py` → `api/src/gateway/server.py`
- `ui/src/tui/*` → `ui/src/tui/*` (unchanged but becomes top-level module)
- `metrics/src/metrics/*` → `metrics/src/metrics/*` (unchanged but becomes top-level module)

---

## Phase 5 — Import Update List (Representative)

- `api/src/gateway/server.py` → `api/src/gateway/server.py`
  - Update `from gateway_utils import ...` → `from gateway.utils import ...`
- `euxis-cli/bin/euxis-gateway` → `euxis-cli/bin/euxis-gateway`
  - Update paths to `api/src/gateway/server.py` and new adapter locations
- `ui/src/tui/*` imports pointing at `core/lib` → `core/lib`

---

## Phase 6 — README Drafts (Templates)

Each module gets a README following the same structure. Example stub for `api/src/gateway/`:

```markdown
# Gateway

The Gateway is the WebSocket control plane for sessions, routing, and streaming events.

## Purpose
Owns connection handling, session routing, event streaming, and HTTP endpoints. It does not execute agents directly.

## Structure
- `server.py` — Gateway runtime
- `utils.py` — shared Gateway helpers
- `webchat/` — WebChat UI

## Dependencies
- `core/`
- `config/`
- `euxis-policy/`

## Usage
```bash
python3 api/src/gateway/server.py --bind 127.0.0.1 --port 18789
```

## Development
```bash
pip install -r requirements.txt
pytest tests/gateway
```

## Testing
See `tests/` for integration and protocol tests.

## API / Exports
Gateway exposes the WebSocket protocol documented in `docs/reference/gateway.md`.
```

---

## Phase 7 — Root README Update
Add a “Module Map” section and boundary rules. List each top-level module with its purpose and README.

---

## Phase 8 — Verification Plan
After each move:
1. `euxis-lint`
2. `euxis-test-infra`
3. `pytest`
4. `python -m tui --help`
5. `python api/src/gateway/smoke_test.py --url http://127.0.0.1:18789/health`

---

## Commit Strategy
One commit per move group:
1. Core
2. Config
3. Leaf modules (tui, metrics, adapters)
4. Domain modules (agents, memory, security)
5. Gateway
6. CLI + scripts
