# Euxis-OpenClaw Bridge Sprint (5-Hour Plan + Implementation Spec)

## Objective

Deliver OpenClaw-compatible skill/runtime behavior inside Euxis without inheriting permissive security tradeoffs.

## Hour 1: Structural Audit + Skill Mapping

### OpenClaw skill model findings

- OpenClaw skills are markdown bundles anchored by `SKILL.md`.
- Skill frontmatter metadata is used for discovery and runtime loading.
- Skills can be loaded from local workspace and global user directories.
- ClawHub is the distribution channel for user-facing skill discovery/install.

### Top-20 skill mapping (OpenClaw -> Euxis Python runtime)

The table below maps high-use OpenClaw skills to Euxis-native Python implementations or wrappers. These are prioritized by operational relevance (filesystem, browser, comms, code, memory, infra) based on OpenClaw ecosystem listings and docs.

| OpenClaw skill | OpenClaw role | Euxis parity target | Python library / stack |
|---|---|---|---|
| `browser-use` | web browsing + extraction | `euxis-web-agent` skill adapter | `playwright`, `httpx`, `selectolax` |
| `fetch-website` | HTTP fetch + summarize | `euxis-fetch` tool | `httpx`, `readability-lxml` |
| `markdownify` | HTML -> markdown | `euxis-normalize` formatter | `markdownify`, `beautifulsoup4` |
| `github` | repo actions / PR flows | `euxis-gitops` adapter | `PyGithub`, `gitpython` |
| `check-logs` | logs analysis | `euxis-observability` analyzer | `orjson`, `pydantic`, `rich` |
| `memory` | local memory persistence | `euxis-memory` compatibility layer | `sqlite3`, `orjson`, `msgpack` |
| `letta-memory` | agent memory graph | `euxis-memory-graph` plugin | `networkx`, `sqlite3` |
| `sequential-thinking` | chain planning | `euxis-planner` runtime mode | `pydantic`, `tenacity` |
| `taskmaster-ai` | task decomposition | `euxis-orchestrator` task plan | `pydantic`, `anyio` |
| `telegram` | remote bot channel | `euxis-bridge-telegram` daemon route | `python-telegram-bot`, `fastapi` |
| `slack` | channel bridge | `euxis-bridge-slack` route | `slack-sdk`, `fastapi` |
| `supabase` | cloud DB actions | `euxis-supabase` adapter | `supabase-py` |
| `neon` | postgres workflows | `euxis-neon` adapter | `psycopg`, `sqlalchemy` |
| `cloudflared` | tunnel/webhook exposure | `euxis-tunnel` ops wrapper | subprocess wrapper + policy gate |
| `ask-perplexity` | retrieval tool | `euxis-ask` external retriever | provider API wrapper |
| `youtube-summarizer` | transcript + summary | `euxis-video-brief` tool | `yt-dlp`, `youtube-transcript-api` |
| `generate-image` | image generation | `euxis-image` provider adapter | provider SDK wrappers |
| `maya-storybook` | narrative generation | `euxis-creative` playbook | internal prompt packs |
| `create-skill` | scaffolding | `euxis-skill-template` generator | cookiecutter/jinja + schema checks |
| `edgeone-pages` | deploy static pages | `euxis-deploy-static` adapter | provider API + signed deploy plan |

### Persistent memory differences

OpenClaw pattern:
- Skill-local and runtime-local files often persisted in JSON/Markdown conventions under user home and workspace.
- Installation/config/memory pathways are highly filesystem-centric for local-first operation.

Euxis pattern:
- Canonical runtime persistence under `~/.euxis/data/runtime` and `~/.euxis/data` with stricter policy separation.
- Gateway session artifacts and identities persist under `~/.euxis/data/gateway/...`.

Bridge decision:
- Import OpenClaw JSON/JSONL/Markdown memory into Euxis canonical stores with provenance metadata and hash verification.

---

## Hour 2: Crypto-Engine Compatibility (Migration Tool Spec)

### Goal

Allow users to point Euxis at an OpenClaw home directory and import identity/memory safely.

### Tool name

`euxis bridge import-openclaw --source ~/.openclaw --dry-run`

### Inputs

- OpenClaw skill and memory roots (`credentials`, `skills`, `agents`, known JSON/JSONL/MD stores).
- Optional key material for decrypt-then-import when encrypted payloads are detected.

### Outputs

- Migrated identity and session data in Euxis gateway/runtime stores.
- `import-manifest.json` including file hashes, timestamps, source paths, and transformation report.

### Validation pipeline

1. Detect source file type (`json`, `jsonl`, `md`).
2. Validate parseability and schema shape.
3. Decrypt if tagged/encrypted.
4. Normalize to Euxis schema.
5. Sign import manifest (crypto-lib key).
6. Emit audit event to security log.

### Security controls

- Fail-closed on malformed encryption metadata.
- Refuse import when signature or checksum mismatch.
- Require explicit `--allow-plaintext` for unencrypted secrets payloads.

---

## Hour 3: TUI vs Messaging Flow (Bridge Daemon)

### Goal

Provide OpenClaw-like always-on messaging ingress (Telegram/Signal) while keeping Euxis TUI/gateway as source of truth.

### Python daemon design

Component: `euxis-bridge-daemon`

Flow:
1. Receive webhook (`/webhook/telegram` or `/webhook/signal`) via FastAPI.
2. Validate signature/token and channel allowlist.
3. Transform inbound payload to Euxis gateway frame.
4. Forward to `ws://127.0.0.1:18789/ws` with gateway token.
5. Persist inbound/outbound event hashes to audit log.
6. Surface event in TUI session timeline.

### Reliability targets

- Backoff + retry on gateway disconnect.
- Idempotency key per inbound message.
- Dead-letter queue for malformed events.

### Cross-platform

- macOS/Linux/WSL systemd-launchd compatible entrypoints.
- No hardcoded shell assumptions.

---

## Hour 4: Security + Sandboxing Layer

### Problem

Permissive shell execution models are high-risk for agentic toolchains.

### Euxis policy layer (mandatory)

- Script/package signatures required before execution.
- Detached signature verification (`ed25519`) for tool scripts.
- Policy checks for:
  - shell subprocess
  - filesystem writes
  - network egress
- Approval gates for privileged actions.

### Verify-before-run contract

1. Resolve tool binary/script path.
2. Load `.sig` and trusted public key.
3. Verify digest/signature.
4. Enforce policy constraints (allowlist + scope).
5. Run in sandbox profile (`strict`).
6. Record signed execution receipt.

### Why this beats permissive models

- Strong provenance for executed code.
- Reduced remote command abuse risk.
- Better forensic traceability via signed audit chain.

---

## Hour 5: Agentic Protocol Documentation

### Deliverables created

- Root runtime bridge guide: `/README_BRIDGE.md`
- Runtime bridge profile: `/bridge_config.yaml`
- Runtime instruction set: `/euxis-ops/bridge/system_prompt_openclaw_bridge.txt`

### Protocol behavior for Euxis bridge mode

- Interpret OpenClaw `SKILL.md` metadata and translate to Euxis tool invocation plans.
- Prefer Python adapters first, Node wrappers second.
- Never bypass signature/policy enforcement for compatibility.
- Map channel-origin identities to Euxis session IDs with deterministic derivation.

---

## Patent + Whitepaper Inputs (2025-2026 standards context)

This sprint aligns with practical 2026 posture by combining:

- OpenClaw native docs on skill lifecycle and channel integration.
- MCP-first interop direction for tool protocols.
- OWASP LLM threat taxonomy for prompt/tooling abuse control.
- NIST AI risk-management framing for policy and governance controls.

Patent landscape note:
- Public patent search for "agent orchestration + secure tool execution" yields a broad and fast-moving set of applications across major vendors; the actionable architecture choice here is implementation-neutral: enforce signed artifacts + least privilege + auditable execution.
- For production legal/commercialization decisions, perform dedicated counsel-led prior-art review before claiming novelty.

---

## Sources

- OpenClaw skills docs: https://docs.openclaw.ai/skills/overview
- OpenClaw advanced skills config: https://docs.openclaw.ai/skills/advanced
- OpenClaw ClawHub docs: https://docs.openclaw.ai/tools/clawhub
- OpenClaw gateway channels: https://docs.openclaw.ai/gateway/channels
- OpenClaw security docs: https://docs.openclaw.ai/troubleshooting/security
- OpenClaw repository: https://github.com/openclaw/openclaw
- OpenClaw ecosystem skill index (operational listing): https://agent-skills.md/authors/openclaw
- MCP specification: https://modelcontextprotocol.io/specification/2025-06-18
- OWASP Top 10 for LLM Applications: https://genai.owasp.org/llm-top-10/
- NIST AI Risk Management Framework resources: https://www.nist.gov/itl/ai-risk-management-framework
