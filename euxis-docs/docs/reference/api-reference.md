# Euxis API Reference

Complete function reference for all 8 library modules in `core/lib/`.

---

## `common.sh` — Logging, UI & Performance

| Function | Signature | Description |
|----------|-----------|-------------|
| `log_info` | `log_info "message"` | Log informational message to stderr |
| `log_error` | `log_error "message"` | Log error message to stderr |
| `log_debug` | `log_debug "message"` | Log debug message (only when `EUXIS_DEBUG=1`) |
| `log_warn` | `log_warn "message"` | Log warning message to stderr |
| `_euxis_now_ns` | `_euxis_now_ns` | Returns current time in nanoseconds (falls back to seconds on macOS) |
| `_perf_start` | `local t; t=$(_perf_start)` | Start a latency timer |
| `_perf_elapsed_ms` | `_perf_elapsed_ms "$t"` | Returns elapsed milliseconds since timer start |
| `_perf_check_budget` | `_perf_check_budget "$ms" "$budget" "op"` | Returns 0 if within budget, 1 if exceeded; logs warning on exceed |
| `_perf_record` | `_perf_record "op" "$ms" "agent" "ok"` | Append JSONL metric to `$EUXIS_HOME/$EUXIS_HOME/metrics/events.jsonl` |
| `start_spinner` | `start_spinner "Thinking..."` | Start background spinner for LLM wait |
| `stop_spinner` | `stop_spinner` | Stop and clear spinner |

**Environment Variables:**
- `EUXIS_DEBUG` — Set to `1` to enable debug logging
- `EUXIS_METRICS_LOG` — Path to JSONL performance metrics file (default: `$EUXIS_HOME/$EUXIS_HOME/metrics/events.jsonl`)

---

## `providers.sh` — Provider Routing & Execution

Depends on: `common.sh`

| Function | Signature | Description |
|----------|-----------|-------------|
| `resolve_tiered_provider` | `resolve_tiered_provider "agent" ["priority"]` | Returns optimal provider for agent based on intelligence tiering. P0 priority always returns `claude`. |
| `resolve_provider_config` | `resolve_provider_config "provider"` | Sets `PROVIDER_MODEL` and `PROVIDER_FLAGS` globals for the given provider |
| `execute_provider` | `execute_provider "provider" "prompt"` | Resolves config and dispatches prompt to the named provider CLI |
| `run_claude` | `run_claude "prompt"` | Execute via Claude CLI with tools, streaming, and prompt caching |
| `run_gemini` | `run_gemini "prompt"` | Execute via Gemini CLI in plain-text mode |
| `run_openai` | `run_openai "prompt"` | Execute via OpenAI Codex CLI |
| `run_ollama` | `run_ollama "prompt"` | Execute via Ollama (local) |
| `run_qwen` | `run_qwen "prompt"` | Execute via Qwen CLI |
| `run_crush` | `run_crush "prompt"` | Execute via Charm Crush TUI |
| `run_kiro_cli` | `run_kiro_cli "prompt"` | Execute via Kiro CLI (kiro-cli) |
| `run_goose` | `run_goose "prompt"` | Execute via Block Goose |

**Intelligence Tiers:**
| Tier | Provider | Agents |
|------|----------|--------|
| S-Tier (Strategic) | `claude` | orchestrator, architect, planner, reviewer, critic, auditor, arbiter, historian |
| A-Tier (Research) | `gemini` | researcher |
| A-Tier (Enterprise) | `amazon-q` | responder |
| A-Tier (Domain) | `claude` | cryptographer, ledger |
| B-Tier (Coding) | `goose` | debugger, tester, automaton |
| B-Tier (Systems) | `goose` | conduit, custodian |
| B-Tier (Math/Logic) | `qwen` | optimizer, telemetrist |
| B-Tier (Coding) | `goose` | maintainer |
| C-Tier (Utility) | `ollama` | butler, librarian, writer |

---

## `agents.sh` — Agent Discovery, Lifecycle & Plugins

| Function | Signature | Description |
|----------|-----------|-------------|
| `resolve_agent_path` | `resolve_agent_path "name"` | Returns absolute path to agent prompt file (searches core/ then fleet/). Returns empty + exit 1 if not found. |
| `list_agents` | `list_agents` | Lists all agent IDs (excludes `_*` protocol files) |
| `agent_lifecycle_transition` | `agent_lifecycle_transition "agent" "state" ["session"]` | Records state transition (idle/active/completed/failed/timeout). Writes atomic state file + JSONL log. |
| `agent_get_state` | `agent_get_state "agent"` | Returns current state string (default: `idle`) |
| `list_active_agents` | `list_active_agents` | Lists agent IDs currently in `active` state |
| `count_active_agents` | `count_active_agents` | Returns count of active agents (for coordination overhead measurement) |
| `cleanup_stale_agents` | `cleanup_stale_agents [timeout_seconds]` | Transitions agents stuck in `active` beyond timeout (default: 1800s) to `timeout` state |
| `register_agent_plugin` | `register_agent_plugin "manifest.json"` | Registers third-party agent from JSON manifest. Symlinks prompt into fleet/, stores manifest in config/plugins/. Requires `jq`. |
| `unregister_agent_plugin` | `unregister_agent_plugin "agent_id"` | Removes plugin agent prompt and manifest |
| `list_plugins` | `list_plugins` | Lists registered plugin agent IDs |
| `agent_probe_liveness` | `agent_probe_liveness "agent"` | Returns `live` if prompt exists and is readable, `dead` otherwise |
| `agent_probe_readiness` | `agent_probe_readiness "agent"` | Returns `ready` if prompt has frontmatter, mandate, output format, and provider available. Returns `not_ready:reason` otherwise. |
| `agent_health_report` | `agent_health_report` | Prints table of all agents with liveness/readiness status |

**Lifecycle States:** `idle` -> `active` -> `completed` | `failed` | `timeout`

**Plugin Manifest Format:**
```json
{
  "agent_id": "my-agent",
  "role": "specialist",
  "prompt_file": "/path/to/prompt.txt",
  "tier": "standard",
  "tags": ["custom"]
}
```

---

## `memory.sh` — Tiered Memory, Pruning & Drift Detection

| Function | Signature | Description |
|----------|-----------|-------------|
| `get_hot_memory` | `get_hot_memory "file"` | Returns last 20 lines of memory file (Tier 1) |
| `get_relevant_memory` | `get_relevant_memory "file" "task"` | Keyword-searches full memory file, returns up to 10 unique matches (Tier 2) |
| `get_cross_agent_memory` | `get_cross_agent_memory "project_dir" "agent" "task"` | Searches sibling agent memories for relevant context (Tier 3, read-only) |
| `build_tiered_memory` | `build_tiered_memory "file" "task" "project_dir" "agent"` | Assembles all 3 tiers into formatted context block |
| `_extract_keywords` | `_extract_keywords "text"` | Pure-bash keyword extraction: words >= 5 chars, deduplicated, max 10, pipe-separated |
| `prune_memory` | `prune_memory "file" [max_lines] [recent_keep]` | Reduces memory file size. Preserves REFLECTION/CONTRAINDICATION entries permanently. Keeps most recent N entries. |
| `prune_project_memory` | `prune_project_memory "project_dir"` | Prunes all memory files in a project |
| `detect_semantic_drift` | `detect_semantic_drift "file" "new_fact"` | Detects contradictions between new fact and existing memories via negation and value-change heuristics |
| `resolve_memory_contradiction` | `resolve_memory_contradiction "file" "fact" "agent" ["resolution"]` | Resolves detected drift. Modes: `supersede` (default), `keep_both`, `reject`. Maintains audit trail. |
| `auto_evolve_graph` | `auto_evolve_graph "file" "agent"` | Extracts entities from recent memories and creates knowledge graph edges via `euxis-graph` |

**Environment Variables:**
- `EUXIS_MEMORY_MAX_LINES` — Pruning threshold (default: 500)
- `EUXIS_MEMORY_RECENT_KEEP` — Lines to retain after pruning (default: 100)

**Memory Entry Types:**
| Type | Prefix | Pruning Behavior |
|------|--------|-----------------|
| Episodic | `EPISODIC:` | Prunable (oldest first) |
| Semantic | `SEMANTIC:` | Prunable (oldest first) |
| Procedural | `PROCEDURAL:` | Prunable unless contains CONTRAINDICATION |
| Reflection | `REFLECTION:` | **Permanent** — never pruned |
| Contraindication | `CONTRAINDICATION:` | **Permanent** — never pruned |

---

## `session.sh` — Project & Session Management

| Function | Signature | Description |
|----------|-----------|-------------|
| `get_project_name` | `get_project_name` | Returns `$EUXIS_PROJECT` or current directory name |
| `get_session_id` | `get_session_id` | Returns `$EUXIS_SESSION_ID` or timestamp-based ID |
| `ensure_project_dirs` | `ensure_project_dirs "project" "agent"` | Creates project/agent directory structure with audit.md and memory.md |
| `get_memory_context` | `get_memory_context "file" [lines]` | Returns last N lines of memory file (default: 50) |

---

## `template.sh` — Variable Substitution

| Function | Signature | Description |
|----------|-----------|-------------|
| `template_substitute` | `template_substitute "$text" "$audit" "$memory" "$session" "$model"` | Pure-bash `{{VAR}}` expansion. Zero subprocess forks. |
| `estimate_tokens` | `estimate_tokens "$text"` | Returns approximate token count (1 token ~ 4 chars) |

**Supported Variables:**
`{{AUDIT_FILE_PATH}}`, `{{MEMORY_FILE_PATH}}`, `{{SESSION_ID}}`, `{{MODEL_NAME}}`, `{{EUXIS_HOME}}`, `{{PROMPTS_DIR}}`, `{{PROJECTS_DIR}}`

---

## `skill-detector.sh` — Project Auto-Detection

| Function | Signature | Description |
|----------|-----------|-------------|
| `detect_project_domain` | `detect_project_domain [dir]` | Scans directory for project indicators and returns space-separated recommended agent IDs |
| `detect_project_type` | `detect_project_type [dir]` | Returns project type labels (e.g., `node python docker`) |

**Detection Rules:**
| Indicator | Detected Agents |
|-----------|----------------|
| `package.json` / `node_modules` | designer, tactician |
| `tsconfig.json` | tester |
| `pyproject.toml` / `setup.py` | tester, automaton |
| `Dockerfile` / `docker-compose.yml` | automaton, optimizer |
| `.env` / `certs/` | sentinel, pentester |
| `.github/workflows` | automaton, gatekeeper |
| `Cargo.toml` | optimizer, debugger, custodian |
| `Gemfile` / `.ruby-version` | maintainer |

---

## `prompt.sh` — Prompt Assembly & Caching

Depends on: `agents.sh`, `memory.sh`, `template.sh`

| Function | Signature | Description |
|----------|-----------|-------------|
| `resolve_protocols` | `resolve_protocols "task"` | Loads mandatory protocols (_common.txt, _protocol.txt) plus conditional protocols based on task keywords. Token-budgeted and cached by keyword fingerprint. |
| `prepare_prompt` | `prepare_prompt "agent" "task" "audit" "memory" "session" "model"` | Full prompt assembly: protocols + agent prompt + template substitution + tiered memory + fleet roster + dispatch directive |
| `_proto_fingerprint` | `_proto_fingerprint "task"` | Generates deterministic cache key from task keyword categories |
| `_get_fleet_roster` | `_get_fleet_roster` | Returns comma-separated agent IDs from agents/registry.json (memoized by file mtime) |

**Environment Variables:**
- `EUXIS_PROTOCOL_TOKEN_BUDGET` — Max tokens for optional protocol loading (default: 12000)
- `EUXIS_DISPATCH` — Set to `true` to inject structured JSON intermediate output directive

**Conditional Protocol Loading:**
| Keywords | Protocol File |
|----------|---------------|
| security, auth, cred, secret, pii | `_security-boundaries.txt` |
| version, release, bump, semver | `_versioning.txt` |
| escalat, incident, sev, outage | `_escalation-severity.txt` |
| evidence, citation, source, fact | `_evidence-citation.txt` |
| handoff, artifact, schema, json | `_artifact-contract.txt` |
| brand, signature, logo | `_docs-branding.txt` |
| synth, aggregate, merge | `_synthesis.txt` |
| bus, pubsub, message, topic | `_bus.txt` |
| graph, entity, edge, knowledge | `_graph-memory.txt` |

---

## CLI Tools

| Tool | Description |
|------|-------------|
| `cli/bin/euxis.sh` | Main entry point — agent dispatch with latency budgets |
| `cli/bin/euxis-lint` | 8-check static analysis (registry, protocol, versions, permissions, headers, patterns, compliance, deps) |
| `cli/bin/euxis-bench` | 7-benchmark performance suite (health, lint, recall, provider, coordination, assembly, memory) |
| `cli/bin/euxis-health` | System health check with `--silent` mode |
| `cli/bin/euxis-dispatch` | Multi-agent pipeline orchestration (mesh/federated/sequential) |
| `cli/bin/euxis-loop` | Iterative agent execution with convergence |
| `cli/bin/euxis-council` | Multi-agent deliberation and voting |
| `cli/bin/euxis-graph` | Knowledge graph operations (add-edge, query) |
| `cli/bin/euxis-kaizen` | Automated maintenance (memory pruning, stale cleanup) |

---

*Euxis v0.1.0 · Build something that matters.*
