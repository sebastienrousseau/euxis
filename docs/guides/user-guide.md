# (c) 2026 Euxis Fleet. All rights reserved.
# Euxis User Guide

**Enterprise Unified eXecution Intelligence System**

Version 0.0.7

## CLI Tools

Complete command-line toolkit. Each tool serves one function.

### Core

| Command | Function |
|---------|----------|
| `euxis <agent> <task> [provider]` | Deploy agent with automatic intelligence routing |
| `euxis-tui` | Modern Terminal UI for fleet management (Python Textual) |
| `euxis-ui` | Legacy menu-based interface (Bash) |

**See also**: [UI Guide](ui-guide.md)

### Quality & Certification

Verify fleet integrity before and after changes.

| Command | Description |
|---------|-------------|
| `euxis-lint` | Static analysis: registry integrity, protocol compliance, version sync |
| `euxis-test-infra` | Infrastructure unit tests: validation, routing, space handling |
| `euxis-certify` | 6-gate certification: lint, tests, semantic, branding, documentation governance |
| `euxis-health` | 8-point fleet health check: naming, hardening, orphans, headers, doc drift, certification, providers, codex |
| `euxis-git-guard` | Pre-commit safety checks |
| `euxis-verify` | Output verification |
| `euxis-polish` | Prompt polishing |

### Orchestration & Autonomy

Coordinate multiple agents or run autonomous workflows.

| Command | Description |
|---------|-------------|
| `euxis-council "<topic>"` | 3-round adversarial debate between architect, pentester, optimizer |
| `euxis-loop <agent> <task> <verify_cmd> [retries]` | Autonomous retry with reflexion and checkpoints |
| `euxis-dispatch [--mode MODE] <manifest.json>` | Parallel agent execution across hierarchical, mesh, or federated modes |
| `euxis-kaizen` | 4-gate self-improvement cycle |
| `euxis-daemon [interval]` | Periodic kaizen with fail-safe halting (default: 30 min) |

### Squads, Playbooks & Combos

Activate teams, run phased workflows, or chain agents in sequence.

| Command | Description |
|---------|-------------|
| `euxis-squad list` | All squads with member counts |
| `euxis-squad info <id>` | Squad details: lead, members, purpose |
| `euxis-squad deploy <id> "<task>"` | Deploy a full squad via dispatch manifest |
| `euxis-squad members <id>` | Member list with registry info |
| `euxis-squad validate` | Cross-check all squad members against registry |
| `euxis-playbook list` | Available playbooks |
| `euxis-playbook info <id>` | Phase breakdown with squads and checkpoints |
| `euxis-playbook run <id> "<goal>" [--dry-run]` | Execute phases sequentially (or preview with `--dry-run`) |
| `euxis-playbook status [session-id]` | Session log |
| `euxis-combo list` | Available combos with chains |
| `euxis-combo info <id>` | Chain detail |
| `euxis-combo run <id> "<task>" [--provider P]` | Execute sequential agent chain |
| `euxis-codex list` | All prompt templates with categories |
| `euxis-codex info <id>` | Template details, variables, and target agents |
| `euxis-codex show <id>` | Print raw template content |
| `euxis-codex run <id> VAR=value [--provider P]` | Substitute variables and execute via target agent |
| `euxis-hooks install [--repo PATH]` | Install Euxis Git hooks into a repository |
| `euxis-hooks uninstall [--repo PATH]` | Remove Euxis hook symlinks |
| `euxis-hooks status [--repo PATH]` | Show which Euxis hooks are installed |
| `euxis-hooks pr "<title>" [--body "<text>"]` | Create a PR with branding signature auto-appended |
| `euxis-hooks check-pr [PR_NUMBER]` | Verify open PR descriptions carry the branding signature |

### Performance & Audit

Benchmark your fleet and run deep security audits.

| Command | Description |
|---------|-------------|
| `euxis-bench` | Performance benchmarks: health, lint, cortex recall, provider latency |
| `euxis-audit-run` | Deep audit: benchmarks, logic integrity, security probes, readiness |
| `euxis-gym <agent> <test_case> [provider]` | Agent evaluation and A/B testing against golden datasets |

### Memory & Knowledge

Store and recall typed memories across sessions.

| Command | Description |
|---------|-------------|
| `euxis-cortex remember "<fact>" "<agent>" --type <episodic\|semantic\|procedural>` | Store typed facts in vector memory |
| `euxis-cortex recall "<query>" [n] [--type <type>]` | Semantic recall from Cortex with type filtering |
| `euxis-cortex stats` | Database statistics |
| `euxis-cortex forget "<text>"` | Remove memory entry |

### Documentation & Governance

| Command | Description |
|---------|-------------|
| `euxis-sync-docs` | Doc sync with human approval |
| `euxis-optimize` | Prompt optimization and compression |

**UI Documentation**: [UI Guide](ui-guide.md) | [Fleet Constitution](../CONSTITUTION.md)

### Infrastructure

| Command | Description |
|---------|-------------|
| `euxis-deploy` | Launch enterprise fleet via Docker Compose |
| `euxis-voice` | Voice interface: record, transcribe, orchestrate, speak |

## Agent Fleet

Euxis provides 42 specialist agents organized into four tiers with distinct operational rules. Each tier serves a specific purpose in the fleet's hierarchy and authority model.

### Core (9) — Authority-bearing, always present

Core agents block progress when necessary. System requires all nine.

| Agent | Owns |
|-------|------|
| `orchestrator` | Coordination, synthesis, final assembly |
| `architect` | Structure, patterns, design decisions |
| `planner` | Intent, scope, prioritization |
| `reviewer` | Truth & quality gate |
| `librarian` | Memory, knowledge continuity, documentation governance |
| `auditor` | Legal, privacy, regulatory authority |
| `critic` | Risk, pre-mortems, counter-bias |
| `arbiter` | Conflict resolution, final decisions when agents disagree |
| `historian` | Long-term memory, temporal patterns, institutional knowledge |

### Default (21) — Auto-available, task-triggered

Execute and advise within scope. Direction comes from core agents.

| Agent | Role |
|-------|------|
| `accountant` | Cost analysis, budget management, resource efficiency |
| `animator` | Theming, color systems, dark mode, animation |
| `automaton` | CI/CD pipelines, IaC, Docker, Terraform |
| `debugger` | Debugging, root cause analysis, surgical fixes |
| `designer` | Web UI components, design systems, responsive layouts |
| `gatekeeper` | Changelogs, semantic versioning, release coordination |
| `inspector` | E2E testing coordination, quality gate enforcement |
| `interactor` | Keyboard navigation, input handling, accessibility |
| `investigator` | Post-incident crash forensics, stack trace analysis |
| `maintainer` | Non-breaking upgrades, migrations |
| `optimizer` | Latency, throughput, memory profiling |
| `pentester` | Security analysis, boundary testing, vulnerability assessment |
| `polyglot` | Language-specific pattern detection, idiom enforcement |
| `repairer` | Automated known-fix application, self-healing infrastructure |
| `responder` | Incident response, root cause analysis, post-mortems |
| `sentinel` | Security policy, threat governance |
| `tactician` | Terminal UI design, keyboard navigation, TUI interaction |
| `telemetrist` | Observability, telemetry, structured logging |
| `tester` | Test coverage, reliability, regression prevention |
| `watchdog` | Pre-merge exhaustive regression analysis |
| `writer` | Documentation, tutorials, API reference, glossary |

### On-Demand (8) — Explicit invocation only

Add capability without blocking. Do not override core authority.

| Agent | Role |
|-------|------|
| `ambassador` | Developer relations, tutorials, demos, conferences |
| `butler` | TTS-optimized summarization, spoken English output |
| `evangelist` | Brand voice, visual identity, content design |
| `localizer` | i18n, l10n, RTL support, Unicode validation |
| `marketer` | SEO, AARRR funnel, CRO, GTM strategy |
| `deep-researcher` | CTO-grade strategic research briefs with citations |
| `researcher` | Iterative research with cross-validation |
| `strategist` | Platform-native content, calendars, community engagement |

### Specialist (4) — Domain-specific expertise

Activated for domain-scoped tasks only. Deep expertise in vertical domains.

| Agent | Role |
|-------|------|
| `cryptographer` | Cryptographic correctness, PQC, hashing |
| `ledger` | ISO 20022, payments compliance, financial schemas |
| `conduit` | Real-time audio, latency, DSP, voice pipelines |
| `custodian` | Rust crate publishing, MSRV, docs |

**For the complete agent registry, governance document, and detailed authority model, see [CONSTITUTION.md](../CONSTITUTION.md).**

## Glossary

| Term | Definition |
|------|-----------|
| **Euxis** | **E**nterprise **U**nified e**X**ecution **I**ntelligence **S**ystem. Local-first AI agent orchestration for autonomous engineering. |
| **Cortex** | Tri-typed semantic memory (episodic, semantic, procedural) backed by ChromaDB |
| **Dispatch** | Parallel agent deployment engine for mission manifests |
| **Kaizen** | Continuous self-improvement loop for fleet auditing and upgrades |
| **Fleet** | 42 agents: 9 core + 21 default + 8 on-demand + 4 specialist |
| **Squad** | Cross-functional agent team with a lead and shared purpose (Vision, Build, Quality, Growth, Experience, Specialist) |
| **Playbook** | Phased sequence of squad activations for repeatable workflows |
| **Combo** | Lightweight sequential chain of agents where each receives the previous output as context |
| **Codex** | Prompt template library with battle-tested templates for structured agent output |

## AI Provider Matrix

Euxis supports 10 providers, tiered by capability. Choose the right provider for each task, or let automatic routing handle it.

### S-Tier: Strategic (High Reasoning)

Use for complex planning, difficult refactors, and mission dispatching.

| Provider | CLI | Best For |
|----------|-----|---------|
| `claude` (Anthropic) | `claude` | Default brain. Best multi-step reasoning and tool use |
| `openai` (GPT-4o) | `codex` | Strong structured output (JSON), reliable fallback |

### A-Tier: Research and Enterprise (High Context)

Use for ingesting massive files or enterprise-specific queries.

| Provider | CLI | Best For |
|----------|-----|---------|
| `gemini` (Google) | `gemini` | 2M+ token context. Use for researcher or massive codebases |
| `amazon-q` (AWS) | `kiro-cli` | AWS infrastructure, enterprise security contexts |

### B-Tier: Specialist (Coding and Logic)

Use for specific engineering tasks.

| Provider | CLI | Best For |
|----------|-----|---------|
| `goose` (Block) | `goose` | Agent-native tool use for developer workflows |
| `qwen` (Alibaba) | `qwen` | Mathematical logic and dense algorithmic problems |

### C-Tier: Utility (Speed and Local)

Use for summaries, linting, and formatting to save costs and latency.

| Provider | CLI | Best For |
|----------|-----|---------|
| `ollama` | `ollama` | Local standard. Runs Llama 3, Phi, etc. |
| `crush` (Charm) | `crush` | High-performance multi-model TUI agent |

## Intelligence Tiering

When no provider is specified, Euxis routes agents by task complexity:

| Tier | Agents | Provider | Reason |
|------|--------|----------|--------|
| S-Tier: Strategic | orchestrator, architect, planner, reviewer, critic, auditor, arbiter, historian | `claude` | Best reasoning and tool use |
| A-Tier: Research | researcher | `gemini` | 2M context window for massive analysis |
| A-Tier: Enterprise | responder | `amazon-q` | AWS-native developer agent |
| A-Tier: Domain | cryptographer, ledger | `claude` | Deep domain reasoning |
| B-Tier: Coding | debugger, tester, automaton | `goose` | Agent-native tool use |
| B-Tier: Systems | conduit, custodian | `goose` | Systems-level tool use |
| B-Tier: Coding | maintainer | `goose` | Agent-native tool use |
| B-Tier: Math/Logic | optimizer, telemetrist | `qwen` | Algorithmic optimization |
| C-Tier: Utility | butler, librarian, writer | `ollama` | Zero latency, no cost |
| Standard | all others | `claude` | General-purpose fallback |

**P0 Override:** Critical tasks always route to S-Tier (claude).

Explicit provider argument overrides tiering.

## Hybrid Dispatch Architecture

Three modes via `euxis-dispatch --mode <mode>`:

| Mode | Description | Use When |
|------|-------------|----------|
| `hierarchical` (default) | All agents report to orchestrator. Centralized coordination. | Standard multi-agent tasks |
| `mesh` | Agents with dispatch authority coordinate sub-workflows directly | Specialists manage focused sub-workflows |
| `federated` | Agents operate autonomously across project boundaries | Cross-project tasks |

**Dispatch-authority agents (mesh mode):** `architect`, `inspector`, `responder`, `gatekeeper`

## Tri-Typed Memory System

The Cortex classifies memories into three types:

| Type | Description | Example |
|------|-------------|---------|
| `episodic` | Specific events and outcomes from sessions | `"Bug #42 fixed by null-check in auth.py line 89"` |
| `semantic` | General facts and relationships across sessions | `"The auth module uses JWT tokens with RS256 signing"` |
| `procedural` | Reusable workflows, patterns, and contraindications | `"To deploy: run tests -> build -> tag -> push -> verify health"` |

```bash
# Store typed memories
euxis-cortex remember "Fixed null pointer in auth.py" "debugger" --type episodic
euxis-cortex remember "API uses OAuth 2.0 with PKCE" "architect" --type semantic
euxis-cortex remember "CONTRAINDICATION: Never retry with expired tokens" "debugger" --type procedural

# Recall with type filter
euxis-cortex recall "authentication" --type procedural
```

## 3-Layer Self-Correction

Every agent applies layered verification before delivering output.

1. **Internal Consistency** — Every claim supported by ReAct OBSERVATION. No fabricated paths. Output format matches agent specification.
2. **Cross-Reference** — Key findings cross-referenced against Cortex memories. Contradictions flagged and investigated.
3. **Evaluator Checkpoint** — Reviewer agent validates synthesized outputs before user delivery. Rejection triggers orchestrator replanning.

**Reflexion Protocol:** On WARNING or FAILURE, agents generate structured analysis (root cause, evidence, strategy, contraindication) stored as PROCEDURAL memory to prevent repeating failed approaches.

## Conflict Resolution

When multiple agents produce conflicting outputs, Euxis resolves them systematically:

1. **Domain Priority** — Agent with primary scope wins (e.g., security conflicts go to `pentester`)
2. **Evidence Weight** — Verified data > inference > heuristic
3. **Negotiation Round** — Each agent produces `CONFLICT_RESPONSE` with position, evidence, confidence, and acceptable compromise. Maximum 1 round.
4. **Human Escalation** — Failed negotiation presents both positions to user

## ReAct Reasoning Loop

All agents use ReAct (Reasoning + Acting) for non-trivial tasks:

```
THOUGHT 1: <What I know, what I need to find out>
ACTION 1: <Specific tool/command to execute>
OBSERVATION 1: <What the result tells me — facts only>

THOUGHT 2: <Updated understanding based on Observation 1>
ACTION 2: <Next tool/command>
OBSERVATION 2: <Result>

FINAL ANSWER: <Synthesized deliverable citing supporting OBSERVATIONs>
```

Minimum 2 cycles before FINAL ANSWER. Every claim must cite supporting OBSERVATION.

## Quick Start

```bash
# Run an agent
euxis architect "Review the auth module"

# Check fleet health
euxis-health

# Full certification
euxis-certify

# Store a typed memory
euxis-cortex remember "Project uses hexagonal architecture" "architect" --type semantic

# Recall procedural knowledge
euxis-cortex recall "deployment workflow" --type procedural

# Dispatch with mesh mode
euxis-dispatch --mode mesh manifest.json

# Performance benchmark
euxis-bench

# Deep audit
euxis-audit-run

# Sync documentation (with approval)
euxis-sync-docs

# Start the gateway (local)
euxis-gateway run

# Check gateway status
euxis-gateway status

# Deploy a squad
euxis-squad deploy build "Fix auth module"

# Run a playbook (dry run first)
euxis-playbook run zero-to-one "Launch auth service" --dry-run

# Execute a combo chain
euxis-combo run envision "Design new onboarding flow"
```

---

*Euxis v0.0.7 · Build something that matters.*
