# Euxis User Guide

**Enterprise Unified eXecution Intelligence System**

Version v0.1.2

## Getting Started

```bash
euxis triage .                              # Quick triage (~45s, flash mode)
euxis check .                               # Standard verification (~3 min)
euxis review . --forensic                   # Forensic-depth verification
euxis certify-readiness . --framework soc2  # Certification readiness
euxis compare .                             # Compare triage vs standard
euxis stats --last 5                        # Recent validation metrics
euxis doctor                                # Environment diagnostics
euxis policy show                           # Inspect active policy
```

## Core Commands

The primary user-facing commands. These wrap the underlying playbook engine.

| Command | Description | Default Mode |
|---------|-------------|-------------|
| `euxis check [target]` | Verify a repository or target | standard |
| `euxis triage [target]` | Fast bounded triage scan | flash |
| `euxis review [target]` | Deep verification | standard |
| `euxis review [target] --forensic` | Forensic-depth verification | forensic |
| `euxis certify-readiness [target]` | Certification readiness assessment (18 domains) | general |
| `euxis compare <target>` | Compare triage vs deep verification | flash+standard |
| `euxis stats [--since] [--last]` | Validation metrics and drift history | — |
| `euxis policy <sub>` | Policy inspection and enforcement | — |
| `euxis doctor` | Environment diagnostics | — |

### Modes

| Mode | Agents | SLA | Use Case |
|------|--------|-----|----------|
| **flash** | 2 (librarian, reviewer) | 45-75s | Quick screening, CI gates |
| **standard** | 5 (librarian, architect, optimizer, sentinel, reviewer) | 3-5 min | Pre-merge verification |
| **forensic** | 11 (all agents) | 10-15 min | Release audits, deep analysis |

### Aliases

| Alias | Resolves to |
|-------|-------------|
| `quick` | `triage` |
| `deep` | `review` |
| `diag` | `doctor` |
| `metrics` | `stats` |
| `pb` | `playbook` |

## Lifecycle Commands

| Command | Description |
|---------|-------------|
| `euxis install` | Bootstrap local Euxis installation |
| `euxis update` | Refresh metadata and registry |
| `euxis upgrade` | Upgrade binary (pull + rebuild) |
| `euxis uninstall` | Remove Euxis from this machine |
| `euxis self` | Installation introspection |

## System Commands

| Command | Description |
|---------|-------------|
| `euxis doctor` | Environment diagnostics (providers, dirs, tools) |
| `euxis fix` | Autonomous environment self-repair |
| `euxis health` | Fleet integrity check (10-point) |
| `euxis verify` | Verify agent prompt integrity |
| `euxis lint` | Lint agent prompts and configs |
| `euxis shell-lint` | Lint shell scripts (shellcheck) |
| `euxis verify-all` | Run all verification checks |
| `euxis cross-platform-verify` | Cross-platform compatibility check |

## Advanced Commands

### Fleet Orchestration

| Command | Description |
|---------|-------------|
| `euxis playbook <manifest> [target]` | Execute a playbook manifest |
| `euxis agent list\|info\|register` | Manage agents |
| `euxis combo list\|run <id> "<task>"` | Run agent combo (sequential pipeline) |
| `euxis squad list\|deploy\|info` | Squad orchestration |
| `euxis council "<topic>"` | Multi-agent council deliberation |
| `euxis loop <agent> <task> <verify>` | Agent feedback loop |
| `euxis dispatch <manifest.json>` | Dispatch agents from manifest |
| `euxis synthesize` | Synthesize outputs from multiple agents |
| `euxis ci` | CI-ready repo verdict (deterministic JSON) |

### Knowledge & Memory

| Command | Description |
|---------|-------------|
| `euxis cortex remember\|recall\|forget` | Semantic memory |
| `euxis graph` | Knowledge graph operations |
| `euxis codex list\|render\|validate` | Template codex |

### Development & Quality

| Command | Description |
|---------|-------------|
| `euxis bench` | Performance benchmarks |
| `euxis audit` | Security and compliance audit |
| `euxis audit-run` | Execute audit run with evidence |
| `euxis certify` | Certify agent readiness (5-point check) |
| `euxis certify-readiness` | Certification readiness (18-domain assessment) |
| `euxis evidence-verify` | Verify audit evidence artifacts |
| `euxis gym <agent> <test>` | Agent training gym |
| `euxis hooks` | Manage git hooks |
| `euxis license-check` | Check license compliance |
| `euxis docs-test` | Test documentation examples |
| `euxis sync-docs` | Sync docs to latest code state |

### Infrastructure

| Command | Description |
|---------|-------------|
| `euxis gateway` | Start HTTP/WS gateway server |
| `euxis bus` | Message bus operations |
| `euxis daemon` | Background daemon management |
| `euxis deploy` | Deploy agent configurations |
| `euxis optimize` | Optimize runtime performance |

## Agent Fleet

Euxis provides 53 agents organized into two tiers with distinct operational rules. The core tier carries authority; the fleet tier is task-triggered.

### Core (12) — Authority-bearing, always present

Core agents block progress when necessary. System requires all twelve.

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
| `route` | Session routing and agent selection |
| `pair` | Channel onboarding and authentication |
| `guard` | Execution approval enforcement |

### Fleet (41) — Task-triggered specialists

Fleet agents execute work within scope. They do not override core authority. For the current list and descriptions, use:

- `euxis agents`
- [Fleet Guide](fleet-guide.md)

**For the complete agent registry, governance document, and detailed authority model, see [CONSTITUTION.md](../policies/CONSTITUTION.md).**

## Glossary

| Term | Definition |
|------|-----------|
| **Euxis** | **E**nterprise **U**nified e**X**ecution **I**ntelligence **S**ystem. Local-first AI agent orchestration for autonomous engineering. |
| **Cortex** | Tri-typed semantic memory (episodic, semantic, procedural) backed by ChromaDB |
| **Dispatch** | Parallel agent deployment engine for mission manifests |
| **Kaizen** | Continuous self-improvement loop for fleet auditing and upgrades |
| **Fleet** | 53 agents: 12 core + 41 fleet |
| **Squad** | Cross-functional agent team with a lead and shared purpose (Vision, Build, Quality, Growth, Experience, Specialist) |
| **Playbook** | Phased sequence of squad activations for repeatable workflows |
| **Combo** | Lightweight sequential chain of agents where each receives the previous output as context |
| **Codex** | Prompt template library with battle-tested templates for structured agent output |

## AI Provider Matrix

Euxis supports 8 provider CLIs. Install at least one and set any required API keys for that provider.

| Provider | CLI | Notes |
|----------|-----|-------|
| `claude` (Anthropic) | `claude` | Default provider. Strong reasoning and tool use. |
| `openai` | `codex` | OpenAI models via Codex CLI. |
| `gemini` (Google) | `gemini` | Large context research. |
| `kiro-cli` | `kiro-cli` | Provider-specific coding workflows. |
| `qwen` | `qwen` | Strong coding and logic tasks. |
| `goose` | `goose` | Agent-native tool use. |
| `crush` (Charm) | `crush` | Multi-model TUI workflows. |
| `ollama` | `ollama` | Local inference, zero cost. |

## Provider Routing

When no provider is specified, Euxis uses the default provider and a fallback chain:

- `EUXIS_DEFAULT_PROVIDER` (default: `claude`)
- `EUXIS_FALLBACK_CHAIN` (default: `claude:gemini:openai:kiro-cli:qwen:crush:goose:ollama`)

You can override routing per command by passing a provider as the last argument:

```bash
euxis architect "Review the auth module" ollama
```

You can also override for a whole session using `EUXIS_SESSION_PROVIDER`.


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

## Provider Strategy

Euxis routes tasks to the optimal AI provider based on semantic classification.

| Task Class | Primary Provider | Fallback |
|-----------|-----------------|----------|
| Research / synthesis | OpenAI | Gemini, Claude |
| Coding / architecture / audit | Claude | Gemini, Ollama |
| Deep research / security | Gemini | OpenAI, Claude |
| Private / local | Ollama | — |
| Surgical edits | Aider | Claude, Ollama |
| Terminal automation | Kiro | ShellGPT, Claude |

Configuration: [`data/config/provider_strategy.json`](../../config/provider_strategy.json).

Override per task class with environment variables:
- `EUXIS_DEFAULT_RESEARCH_PROVIDER`
- `EUXIS_DEFAULT_CODING_PROVIDER`
- `EUXIS_DEFAULT_SECURITY_PROVIDER`
- `EUXIS_LOCAL_ONLY=true` forces all tasks to Ollama

## Quick Reference

```bash
# Core verification commands
euxis triage .                              # Quick triage (~45s)
euxis check .                               # Standard (~3 min)
euxis review . --forensic                   # Forensic depth
euxis certify-readiness . --framework soc2  # Certification readiness

# Fleet orchestration
euxis playbook verify-everything .          # Full playbook
euxis combo run envision "Design X"         # Multi-agent pipeline
euxis squad deploy quality "Audit auth"     # Squad deployment
euxis council "Should we adopt gRPC?"       # Council deliberation

# Knowledge & memory
euxis cortex remember "auth uses JWT"       # Store memory
euxis cortex recall "authentication"        # Recall knowledge

# Infrastructure
euxis gateway                               # Start HTTP/WS gateway
euxis bench                                 # Performance benchmarks
euxis audit                                 # Security audit

# Diagnostics
euxis doctor                                # Environment check
euxis health                                # Fleet integrity
```

---

Euxis v0.1.2 · [euxis.co](https://euxis.co)
