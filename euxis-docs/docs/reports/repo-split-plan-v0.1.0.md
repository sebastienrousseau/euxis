# Repository Split — Monolith to Modular (v0.0.1)

## Executive Summary
Split Euxis into a small set of purpose-built repos that mirror today’s module boundaries: core runtime, gateway+adapters, CLI tools, TUI, agents/prompts, security policy, runtime/memory/metrics data tooling, and docs. This structure preserves developer velocity by keeping tightly-coupled surfaces together (CLI + core libs, Gateway + adapters) while enabling independent releases for UI, gateway, and tooling.

---

## Phase 1 — Forensic Analysis

### Dependency Mapping (evidence-based)

**Core gravity center (shell libs):**
- `cli/bin/euxis.sh` sources `core/lib/*.sh` (`core/lib/common.sh`, `providers.sh`, `agents.sh`, `memory.sh`, `session.sh`, `template.sh`, `skill-detector.sh`, `prompt.sh`, `cli.sh`, `dispatch.sh`).
  - Evidence: `cli/bin/euxis.sh:74-100`.
- Nearly every CLI script sources `core/lib/ui.sh`.
  - Evidence: `cli/bin/euxis-*.sh` list in `rg` results.

**Gateway depends on adapters and gateway utils:**
- `api/src/gateway/server.py` imports `adapters.registry` and `gateway.utils`.
  - Evidence: `api/src/gateway/server.py:30-31`.
- Adapters depend on Gateway utilities:
  - `adapters/src/adapters/slack_adapter.py` imports `gateway.utils` for `persist_message` and `timestamp`.
  - `adapters/src/adapters/telegram_adapter.py` imports `gateway.utils` for `persist_message` and `timestamp`.

**TUI depends on crypto/src/crypto_lib and its own core:**
- `ui/src/tui/core/services.py` imports `crypto/src/crypto_lib` encryption helpers.
  - Evidence: `ui/src/tui/core/services.py:21-23`.
- TUI screens import `tui.core` and `tui.widgets` internally (self-contained).

**Metrics are self-contained but used by tooling:**
- `metrics/src/metrics/metrics_cli.py` imports `metrics/src/metrics/aggregators/performance_analyzer`.
  - Evidence: `metrics/src/metrics/metrics_cli.py:19`.
- Verification components import metrics analyzers.
  - Evidence: `metrics/src/metrics/verification/evidence_framework.py:27`.

**Security policy is read by Gateway:**
- Gateway config defaults load from `~/.euxis/security/gateway.json`.
  - Evidence: `api/src/gateway/server.py:133`.

### Circular Dependencies
No module-level circular imports were detected in Python packages (Gateway, Adapters, TUI, Metrics, crypto/src/crypto_lib). Shell scripts depend on `core/lib` but `core/lib` does not import from CLI. This yields a directed dependency graph with `core/` as the root.

### Shared Utilities & Types
- `core/lib/*` provides shared shell utilities and validation (used by CLI).
- `security/gateway.json` defines approval policy defaults (consumed by Gateway).
- `agents/registry.json` and `agents/squads.json` provide agent metadata (consumed by CLI and TUI).

### Data Flow (runtime)
- Gateway writes session, approvals, audit, transcripts to `~/.euxis/runtime/data/gateway/*` via `api/src/gateway/utils.py`.
- Cortex memory stored under `~/.euxis/runtime/memory/cortex` and accessed by `cli/bin/euxis-cortex` and `cli/bin/euxis-graph`.
- Metrics recorded under `~/.euxis/metrics/events.jsonl` and consumed by metrics tooling.

### Tests & Coverage
- Shell tests and infra validation live under `tests/`.
- Gateway protocol tests live under `api/src/gateway/protocol_test.py`.
- TUI tests in `tests/unit/test_*` reference `ui/src/tui/` screens.

### Configuration & Secrets
- Gateway policy defaults in `security/gateway.json`.
- Other configuration in `config/` (playbooks, schemas, codex).
- Provider credentials are environment variables (documented in `README.md`).

### Git/Change Coupling (observed by structure)
- CLI and `core/lib` change together (sourced scripts).
- Gateway and adapters change together (import dependencies).
- TUI changes mostly isolated except for shared agent registry.

---

## Phase 2 — Target Repo Structure

### Proposed Repositories (minimal set)

1. **`euxis-core`**
   - **Purpose:** shared runtime utilities and contracts.
   - **Contents:** `core/`, `agents/` (registry + prompts), `config/` (schemas).
   - **Public interface:** shared shell libs, agent registry, config schemas.
   - **Dependencies:** none.
   - **Build/deploy:** N/A (library + data). CI: lint + schema validation.
   - **Migration complexity:** Medium (many dependents).

2. **`euxis-cli`**
   - **Purpose:** CLI entrypoints and tooling.
   - **Contents:** `cli/`.
   - **Public interface:** `euxis-*` commands.
   - **Dependencies:** `euxis-core`.
   - **Build/deploy:** shell lint + certify subset.
   - **Migration complexity:** Medium.

3. **`euxis-gateway`**
   - **Purpose:** WebSocket control plane.
   - **Contents:** `api/src/gateway/`.
   - **Public interface:** WebSocket protocol, HTTP endpoints.
   - **Dependencies:** `euxis-core`, `euxis-security`, `euxis-adapters`.
   - **Build/deploy:** Docker image + health check.
   - **Migration complexity:** High.

4. **`euxis-adapters`**
   - **Purpose:** channel adapters.
   - **Contents:** `adapters/src/adapters/`.
   - **Public interface:** adapter SDK + registry.
   - **Dependencies:** `euxis-gateway` (utils/contract).
   - **Build/deploy:** pip/py module release.
   - **Migration complexity:** Medium.

5. **`euxis-tui`**
   - **Purpose:** Textual UI app.
   - **Contents:** `ui/src/tui/`.
   - **Public interface:** `euxis-tui` UI.
   - **Dependencies:** `euxis-core`, `crypto/src/crypto_lib`.
   - **Build/deploy:** pip distribution or standalone script.
   - **Migration complexity:** Medium.

6. **`euxis-security`**
   - **Purpose:** security policy defaults.
   - **Contents:** `security/`.
   - **Public interface:** `security/gateway.json`.
   - **Dependencies:** none.
   - **Build/deploy:** static config distribution.
   - **Migration complexity:** Low.

7. **`euxis-metrics`**
   - **Purpose:** observability tooling.
   - **Contents:** `metrics/src/metrics/`.
   - **Public interface:** `metrics_cli.py`, metrics schemas.
   - **Dependencies:** `euxis-core` (optional) for shared schemas.
   - **Build/deploy:** Python tool distribution.
   - **Migration complexity:** Medium.

8. **`euxis-docs`**
   - **Purpose:** documentation + ADRs.
   - **Contents:** `docs/`, `mkdocs.yml`.
   - **Public interface:** docs site.
   - **Dependencies:** none.
   - **Migration complexity:** Low.

9. **`euxis-crypto-lib`**
   - **Purpose:** Python crypto helpers.
   - **Contents:** `crypto/src/crypto_lib/`.
   - **Public interface:** Python package exports.
   - **Dependencies:** none.
   - **Migration complexity:** Low.

10. **`euxis-crypto-packages`**
   - **Purpose:** TypeScript crypto packages.
   - **Contents:** `packages/crypto-lib`, `packages/crypto-server`.
   - **Public interface:** npm packages.
   - **Dependencies:** none.
   - **Migration complexity:** Low.

### Shared Packages

- **`euxis-core`** contains shared shells + agent registry. Versioned with semver; breaking change when registry schema changes or CLI API changes.
- **`euxis-security`** publishes policy defaults; changes are backward-compatible unless keys removed.
- **`euxis-metrics`** publishes metrics schemas; versioned independently.

### What Stays Together
- **Gateway + Adapters** should stay close because adapters import `gateway.utils` and rely on gateway session persistence.
- **CLI + core/lib** should stay together because the CLI sources core libraries directly.

---

## Phase 3 — Interface Contracts

### API Contracts (Gateway)
- WebSocket event schema: `docs/reference/gateway.md`.
- HTTP endpoints: `api/src/gateway/server.py` (`/health`, `/approvals/*`).
- Auth and policy defaults: `~/.euxis/security/gateway.json`.

### Event Contracts
- Gateway emits `agent`, `presence`, `tick`, `shutdown` events (documented in gateway docs).

### Package Contracts
- `euxis-core` exports agent registry and shell libs (public API is filesystem-based entrypoints).

### Data Ownership
- Gateway owns `~/.euxis/runtime/data/gateway/*`.
- Memory owns `~/.euxis/runtime/memory/cortex/*`.
- Metrics owns `~/.euxis/metrics/events.jsonl`.

---

## Phase 4 — Migration Plan

### Pre-migration Checklist
1. Break any circular dependencies (none detected in python modules).
2. Extract shared types/schemas into `euxis-core`.
3. Add contract tests for gateway config and adapter interface.
4. Document existing CI/CD flows.
5. Create empty repos with CI skeletons.

### Migration Sequence

1. **Extract `euxis-core`**
   - Move `core/`, `agents/`, `config/`.
   - Update consumers (CLI, Gateway, TUI) to pull from released package or submodule.

2. **Extract `euxis-cli`**
   - Move `cli/`.
   - Ensure core/lib is a dependency via checkout or package.

3. **Extract `euxis-security`**
   - Move `security/` and update Gateway config default path.

4. **Extract `euxis-gateway` + `euxis-adapters`**
   - Move `api/src/gateway/` and `adapters/src/adapters/` into separate repos or a single repo with packages.

5. **Extract `euxis-tui`**
   - Move `ui/src/tui/` and rewire `crypto/src/crypto_lib` dependency.

6. **Extract `euxis-metrics`**
   - Move `metrics/src/metrics/` and keep optional core schema references.

7. **Extract `euxis-docs`**
   - Move `docs/` and doc tooling config.

8. **Extract crypto packages**
   - Split `crypto/src/crypto_lib/` and `packages/*` into their dedicated repos.

### Git History Preservation
Use `git filter-repo --path` per repo to preserve history. Example:
- `git filter-repo --path gateway --path adapters --force`

---

## Phase 5 — Risk Register (Top 10)

1. **Import path breakage** — mitigate with contract tests and staged extraction.
2. **Gateway/adapters coupling** — keep together initially, split later.
3. **CLI/core coupling** — use packaged core with release gating.
4. **Docs drift** — centralize docs in `euxis-docs` and automate sync.
5. **CI fragmentation** — replicate certify gates per repo.
6. **Version drift** — use Renovate/Dependabot for cross-repo bumps.
7. **Secrets handling** — define standard env var contract for all repos.
8. **Loss of atomic changes** — use coordinated release branches.
9. **Contract mismatch** — add schema validation tests.
10. **Developer friction** — publish internal dev kit for local multi-repo workflows.

---

## Decision Log

1. Keep **Gateway + Adapters** together for initial split due to direct imports in `adapters/src/adapters/*` -> `api/src/gateway/utils.py`.
2. Keep **CLI + core/lib** closely coupled due to direct `source` usage.
3. Introduce **security repo** to isolate policy defaults from gateway runtime.
4. Extract **metrics** to allow independent release cadence.
5. Split **crypto/src/crypto_lib** and **packages/** into dedicated repos due to different languages/toolchains.

---

## Repository Map (ASCII)

```
[euxis-core]
   |\
   | \--> [euxis-cli]
   | \--> [euxis-tui]
   | \--> [euxis-metrics]
   | \--> [euxis-gateway]
   |
[euxis-security] ----> [euxis-gateway]
[euxis-adapters] ----> [euxis-gateway]
[euxis-docs]
[euxis-crypto-lib]
[euxis-crypto-packages]
```


## Extraction Commands

Use `git filter-repo` to preserve history. Example commands:

```bash
# Core
mkdir -p ${TMPDIR:-/tmp}/euxis-core && cd ${TMPDIR:-/tmp}/euxis-core
cp -R ${EUXIS_HOME}/.git .
git filter-repo --path core --path agents --path config --force

# CLI
mkdir -p ${TMPDIR:-/tmp}/euxis-cli && cd ${TMPDIR:-/tmp}/euxis-cli
cp -R ${EUXIS_HOME}/.git .
git filter-repo --path cli --force

# Gateway
mkdir -p ${TMPDIR:-/tmp}/euxis-gateway && cd ${TMPDIR:-/tmp}/euxis-gateway
cp -R ${EUXIS_HOME}/.git .
git filter-repo --path gateway --force

# Adapters
mkdir -p ${TMPDIR:-/tmp}/euxis-adapters && cd ${TMPDIR:-/tmp}/euxis-adapters
cp -R ${EUXIS_HOME}/.git .
git filter-repo --path adapters --force

# TUI
mkdir -p ${TMPDIR:-/tmp}/euxis-tui && cd ${TMPDIR:-/tmp}/euxis-tui
cp -R ${EUXIS_HOME}/.git .
git filter-repo --path tui --force

# Metrics
mkdir -p ${TMPDIR:-/tmp}/euxis-metrics && cd ${TMPDIR:-/tmp}/euxis-metrics
cp -R ${EUXIS_HOME}/.git .
git filter-repo --path metrics --force

# Security
mkdir -p ${TMPDIR:-/tmp}/euxis-security && cd ${TMPDIR:-/tmp}/euxis-security
cp -R ${EUXIS_HOME}/.git .
git filter-repo --path security --force

# Docs
mkdir -p ${TMPDIR:-/tmp}/euxis-docs && cd ${TMPDIR:-/tmp}/euxis-docs
cp -R ${EUXIS_HOME}/.git .
git filter-repo --path docs --path mkdocs.yml --force

# Crypto (Python)
mkdir -p ${TMPDIR:-/tmp}/euxis-crypto-lib && cd ${TMPDIR:-/tmp}/euxis-crypto-lib
cp -R ${EUXIS_HOME}/.git .
git filter-repo --path crypto/src/crypto_lib --force

# Crypto packages (TS)
mkdir -p ${TMPDIR:-/tmp}/euxis-crypto-packages && cd ${TMPDIR:-/tmp}/euxis-crypto-packages
cp -R ${EUXIS_HOME}/.git .
git filter-repo --path packages/crypto-lib --path packages/crypto-server --force
```


## Contract Tests (Recommendations)

- **Gateway ↔ Adapters**
  - Validate adapter contract (`connect/receive/send/ack/disconnect`).
  - Ensure adapter emits normalized message schema used by Gateway.
- **CLI ↔ Core**
  - CLI scripts should validate `core/lib` functions via smoke tests.
- **TUI ↔ Core**
  - Mock `agents/registry.json` and ensure TUI screens load registry.
- **Metrics ↔ Core**
  - Validate metrics schema and event shape stability.


## CI Templates (Minimal)

- **Python repos**: lint + unit tests + packaging check
- **Shell repos**: `shellcheck`, `shfmt`, `euxis-certify` (subset)
- **Gateway repo**: unit tests + protocol test + health check
- **Docs repo**: mkdocs build + link check

Example GitHub Actions skeleton (Python):

```yaml
name: ci
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.11'
      - run: pip install -r requirements.txt
      - run: pytest -q
```


## Local Multi-Repo Workflow

Recommended patterns for developers:

- **git worktree**: keep `euxis-core`, `euxis-cli`, `euxis-gateway` checked out side-by-side.
- **Path deps**: point `euxis-cli` to `../euxis-core` during development.
- **Feature flags**: coordinate breaking changes with toggles.

Scaffold (monorepo-friendly) assets:

- `docs/workflows/multi-repo-workflow.md` for day-to-day dev flow.
- `config/multi-repo.example.json` to define local repo URLs.
- `scripts/setup/multi-repo-dev.sh` to clone a local multi-repo layout.

Example worktree setup:

```bash
git clone git@github.com:org/euxis-core.git ~/dev/euxis-core
git -C ~/dev/euxis-core worktree add ../euxis-cli
```


## Repo README Drafts (short)

- **euxis-core** — shared registry, schemas, and shell libs for Euxis.
- **euxis-cli** — command entrypoints and automation tooling.
- **euxis-gateway** — WebSocket control plane with HTTP health endpoints.
- **euxis-adapters** — channel adapters and adapter SDK.
- **euxis-tui** — Textual UI application for fleet management.
- **euxis-security** — approval and allowlist policy defaults.
- **euxis-metrics** — observability tooling and fleet metrics schemas.
- **euxis-docs** — documentation and ADRs.
- **euxis-crypto-lib** — Python crypto utilities.
- **euxis-crypto-packages** — TypeScript crypto packages.
