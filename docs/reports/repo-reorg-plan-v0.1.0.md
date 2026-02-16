# Euxis Monorepo Reorg Plan (v0.1.0)

## Scope
Reorganize the monorepo into modular, self-contained top-level folders with explicit boundaries, co-located tests, and module READMEs. No repo splitting or build tool changes.

---

## Phase 1 — Current Structure Map

### Top-level directories and purpose
- `bin/`: CLI entrypoints and shell/Python tooling (`euxis-*`, lint, certify, hooks, lib/).
- `config/`: config schemas, templates, branding, playbooks, patterns.
- `crypto_lib/`: Python crypto library code.
- `data/`: runtime data (bus, cortex, perf, registry pool).
- `deploy/`: Dockerfiles and deployment compose files.
- `docs/`: documentation, reference, ADRs, guides, reports.
- `metrics/`: metrics collection, aggregation, verification, dashboard schemas.
- `packages/`: sub-packages (`crypto-lib`, `crypto-server`).
- `prompts/`: agent prompts and protocols.
- `scripts/`: gateway server + adapters + tooling scripts.
- `tests/`: test suites for infra, TUI, CLI, gateway, etc.
- `tui/`: Textual-based ETX TUI app.

### Multipurpose directories
- `scripts/` mixes Gateway runtime, adapters, demos, and tooling.
- `bin/` mixes user-facing commands with internal helpers.
- `packages/` overlaps with `crypto_lib/` (duplication risk).

---

## Phase 2 — Proposed Modular Structure

```
core/        # Shared types, constants, utilities
agents/      # Agent prompts, squads, combos, playbooks
gateway/     # Gateway server, adapters, protocol utilities
cli/         # Command-line entrypoints and shell libs
tui/         # ETX Textual UI
adapters/    # Channel adapters (Slack, Telegram, WebChat)
memory/      # Cortex + memory persistence
security/    # Approvals, allowlists, audit, Guard logic
metrics/     # Metrics collection, aggregation, dashboards
config/      # Config schemas and defaults
docs/        # Docs (kept as-is)
scripts/     # Build/dev tooling (non-runtime)
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
- **Contains**: `prompts/`, `squads.json`, `registry.json`.
- **Depends on**: `core/`, `config/`.

### `gateway/`
- **Purpose**: WebSocket control plane and HTTP endpoints.
- **Contains**: `scripts/gateway_server.py`, `scripts/gateway_utils.py`, gateway schemas, `gateway_webchat/`.
- **Depends on**: `core/`, `security/`, `config/`.

### `cli/`
- **Purpose**: User entrypoints and shell tooling.
- **Contains**: `bin/euxis*`, `core/lib/`.
- **Depends on**: `core/`, `agents/`, `gateway/`.

### `tui/`
- **Purpose**: ETX Textual UI.
- **Contains**: `tui/` app and screens.
- **Depends on**: `core/`, `agents/`, `metrics/`.

### `adapters/`
- **Purpose**: Channel adapter implementations.
- **Contains**: `scripts/gateway_adapters/*`.
- **Depends on**: `gateway/`, `core/`.

### `memory/`
- **Purpose**: Cortex storage and memory flows.
- **Contains**: `data/cortex/` schema + related tools.
- **Depends on**: `core/`.

### `security/`
- **Purpose**: approvals, allowlists, audit trail.
- **Contains**: approval persistence, allowlist configs.
- **Depends on**: `core/`, `config/`.

### `metrics/`
- **Purpose**: Metrics pipeline.
- **Contains**: `metrics/`.
- **Depends on**: `core/`.

---

## Phase 4 — Move Manifest (High-Level)

- `bin/*` → `cli/bin/*`
- `bin/lib/*` → `core/lib/*`
- `prompts/*` → `agents/prompts/*`
- `registry.json`, `registry.db`, `squads.json` → `agents/`
- `scripts/gateway_*` → `gateway/`
- `scripts/gateway_adapters/*` → `adapters/`
- `scripts/gateway_webchat/*` → `gateway/webchat/*`
- `scripts/gateway_utils.py` → `gateway/utils.py`
- `scripts/gateway_server.py` → `gateway/server.py`
- `tui/*` → `tui/*` (unchanged but becomes top-level module)
- `metrics/*` → `metrics/*` (unchanged but becomes top-level module)

---

## Phase 5 — Import Update List (Representative)

- `scripts/gateway_server.py` → `gateway/server.py`
  - Update `from scripts.gateway_utils import ...` → `from gateway.utils import ...`
- `bin/euxis-gateway` → `cli/bin/euxis-gateway`
  - Update paths to `gateway/server.py` and new adapter locations
- `tui/*` imports pointing at `core/lib` → `core/lib`

---

## Phase 6 — README Drafts (Templates)

Each module gets a README following the same structure. Example stub for `gateway/`:

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
- `security/`

## Usage
```bash
python3 gateway/server.py --bind 127.0.0.1 --port 18789
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
5. `python scripts/gateway_smoke_test.py --url http://127.0.0.1:18789/health`

---

## Commit Strategy
One commit per move group:
1. Core
2. Config
3. Leaf modules (tui, metrics, adapters)
4. Domain modules (agents, memory, security)
5. Gateway
6. CLI + scripts
