# Euxis User Guide

Enterprise Unified eXecution Intelligence System

Version 6.0

## CLI Tools

### Core

| Command | Description |
|---------|-------------|
| `euxis <agent> <task> [provider]` | Route a task to any agent. Provider auto-selected via intelligence tiering if omitted. |
| `euxis-ui` | Interactive Mission Control TUI for deploying agents and browsing logs. |

### Quality & Certification

| Command | Description |
|---------|-------------|
| `euxis-lint` | Static analysis: registry integrity, protocol compliance, version sync. |
| `euxis-test-infra` | Infrastructure unit tests: argument validation, provider routing, space handling. |
| `euxis-certify` | Full certification pipeline (lint + infra tests + semantic verification). |
| `euxis-health` | Fleet health check: naming, hardening, orphans, headers, doc drift, providers. |

### Orchestration & Autonomy

| Command | Description |
|---------|-------------|
| `euxis-council "<topic>"` | 3-round adversarial debate (architect, edge-hunter, perf-optimizer). |
| `euxis-loop <agent> <task> <verify_cmd> [retries]` | Autonomous retry loop with reflexion and checkpoint support. |
| `euxis-dispatch [--mode MODE] <manifest.json>` | Batch task dispatcher for parallel agent execution. Supports `hierarchical`, `mesh`, and `federated` modes. |
| `euxis-kaizen` | 4-gate continuous self-improvement cycle. |
| `euxis-daemon [interval]` | Periodic kaizen loop with fail-safe halting. Default: 30 min. |

### Performance & Audit

| Command | Description |
|---------|-------------|
| `euxis-bench` | Performance benchmarking: health, lint, cortex recall, provider latency. |
| `euxis-audit-run` | Deep-dive audit: benchmarks, logic integrity, security probes, readiness report. |
| `euxis-gym <agent> <test_case> [provider]` | Agent evaluation and A/B testing against golden datasets. |

### Memory & Knowledge

| Command | Description |
|---------|-------------|
| `euxis-cortex remember "<fact>" "<agent>" --type <episodic\|semantic\|procedural>` | Store a typed fact in the vector memory. |
| `euxis-cortex recall "<query>" [n] [--type <type>]` | Semantic recall from the Cortex, optionally filtered by memory type. |
| `euxis-cortex stats` | Show database statistics. |
| `euxis-cortex forget "<text>"` | Remove a specific memory entry. |

### Documentation & Governance

| Command | Description |
|---------|-------------|
| `euxis-sync-docs` | Librarian-powered doc sync with human-in-the-loop approval. |
| `euxis-optimize` | Prompt optimization and compression. |

### Infrastructure

| Command | Description |
|---------|-------------|
| `euxis-deploy` | Launch the enterprise fleet via Docker Compose. |
| `euxis-voice` | Voice interface: record, transcribe (Whisper), orchestrate, speak (Piper TTS). |

## Agent Fleet

### Core (3)

| Agent | Role |
|-------|------|
| `orchestrator` | Task decomposition, delegation, synthesis across the fleet |
| `architect` | Software architecture, patterns, ADRs, refactoring |
| `librarian` | Context architect, compliance custodian, memory optimizer |

### Fleet (21)

| Agent | Role |
|-------|------|
| `automation-engineer` | CI/CD pipelines, IaC, Docker, Terraform |
| `bug-fixer` | Debugging, root cause analysis, surgical fixes |
| `butler` | TTS-optimized summarization, spoken English output |
| `compliance-officer` | PII scanning, license auditing, GDPR/CCPA compliance |
| `data-steward` | Observability, telemetry, structured logging |
| `deep-researcher` | Iterative multi-pass research with cross-validation |
| `devrel-advocate` | Developer relations, tutorials, demos, conferences |
| `edge-hunter` | Security analysis, boundary testing, vulnerability assessment |
| `globalization-lead` | i18n, l10n, RTL support, Unicode validation |
| `growth-marketer` | SEO, AARRR funnel, CRO, GTM strategy |
| `incident-commander` | Incident response, root cause analysis, post-mortems |
| `legacy-maintainer` | Legacy code documentation, non-breaking upgrades |
| `perf-optimizer` | Latency, throughput, memory profiling |
| `product-manager` | Requirements, user stories, MoSCoW prioritization |
| `qa-coordinator` | E2E testing coordination, quality gate enforcement |
| `release-manager` | Changelogs, semantic versioning, release coordination |
| `reviewer` | Quality gate, output validation, completeness checking |
| `social-manager` | Platform-native content, calendars, community engagement |
| `tech-writer` | Documentation, tutorials, API reference, glossary |
| `unit-tester` | Test coverage, reliability, regression prevention |
| `ux-sentinel` | Accessibility (WCAG 2.1 AA), design system, responsive testing |

## Glossary

| Term | Definition |
|------|-----------|
| **Euxis** | **E**nterprise **U**nified e**X**ecution **I**ntelligence **S**ystem — A local-first, privacy-centric operating system for AI agents that orchestrates specialized personas for autonomous engineering tasks. |
| **Cortex** | The tri-typed semantic memory layer (episodic, semantic, procedural) backed by ChromaDB. |
| **Dispatch** | The parallel agent deployment engine that executes mission manifests. |
| **Kaizen** | The continuous self-improvement loop that audits, documents, and upgrades the fleet. |
| **Fleet** | The full collection of 24 specialized agent personas. |

## Intelligence Tiering (5-Tier Cost-Optimized Routing)

When no provider is specified, agents are auto-routed based on task complexity:

| Tier | Agents | Provider | Reason |
|------|--------|----------|--------|
| Strategic | orchestrator, architect, product-manager, reviewer | `claude` | Best reasoning and tool use |
| Research | deep-researcher | `gemini` | 2M context window for massive analysis |
| Coding | bug-fixer, legacy-maintainer | `opencode` | Fast local code models for diffs |
| Utility | butler, librarian | `ollama` | Zero latency, no cost for summaries |
| Standard | all others | `claude` | General-purpose fallback |

**P0 Override:** Tasks marked P0 (CRITICAL) always route to the Strategic tier regardless of agent.

An explicit provider argument always overrides tiering.

## Hybrid Dispatch Architecture (v5.0)

The fleet supports three dispatch modes via `euxis-dispatch --mode <mode>`:

| Mode | Description | Use When |
|------|-------------|----------|
| `hierarchical` (default) | All agents report to orchestrator. Centralized coordination. | Standard multi-agent tasks |
| `mesh` | Agents with dispatch authority coordinate sub-workflows directly. | Specialists can manage focused sub-workflows (e.g., `qa-coordinator` dispatching `unit-tester`) |
| `federated` | Agents operate autonomously across project boundaries. | Cross-project tasks |

**Dispatch-authority agents (mesh mode):** `architect`, `qa-coordinator`, `incident-commander`, `release-manager`

## Tri-Typed Memory System (v5.0)

The Cortex classifies all memories into three types:

| Type | Description | Example |
|------|-------------|---------|
| `episodic` | Specific events and outcomes from a session | `"Bug #42 fixed by null-check in auth.py line 89"` |
| `semantic` | General facts and relationships that persist across sessions | `"The auth module uses JWT tokens with RS256 signing"` |
| `procedural` | Reusable workflows, patterns, and contraindications | `"To deploy: run tests -> build -> tag -> push -> verify health"` |

```bash
# Store typed memories
euxis-cortex remember "Fixed null pointer in auth.py" "bug-fixer" --type episodic
euxis-cortex remember "API uses OAuth 2.0 with PKCE" "architect" --type semantic
euxis-cortex remember "CONTRAINDICATION: Never retry with expired tokens" "bug-fixer" --type procedural

# Recall with type filter
euxis-cortex recall "authentication" --type procedural
```

## 3-Layer Self-Correction (v5.0)

Every agent applies layered verification before producing output:

1. **Layer 1 — Internal Consistency:** Every claim is supported by a ReAct OBSERVATION. No fabricated paths. Output format matches the agent's declared format.
2. **Layer 2 — Cross-Reference:** Key findings are cross-referenced against Cortex memories. Contradictions are flagged and investigated.
3. **Layer 3 — Evaluator Checkpoint:** The `reviewer` agent validates synthesized outputs before user delivery. If rejected, the orchestrator replans the failing phase.

**Reflexion Protocol:** On WARNING or FAILURE, agents generate a structured analysis (root cause, evidence, strategy, contraindication) and store it as a PROCEDURAL memory to prevent repeating failed approaches.

## Conflict Resolution Protocol (v5.0)

When multiple agents produce conflicting outputs, resolution follows this hierarchy:

1. **Domain Priority** — The agent with primary scope wins (e.g., security conflicts → `edge-hunter`).
2. **Evidence Weight** — Verified data > inference > heuristic.
3. **Negotiation Round** — Each agent produces a `CONFLICT_RESPONSE` with position, evidence, confidence, and acceptable compromise. Maximum 1 round.
4. **Human Escalation** — If negotiation fails, both positions are presented to the user.

## ReAct Reasoning Loop (v5.0)

All agents use the ReAct (Reasoning + Acting) loop for non-trivial tasks:

```
THOUGHT 1: <What I know, what I need to find out>
ACTION 1: <Specific tool/command to execute>
OBSERVATION 1: <What the result tells me — facts only>

THOUGHT 2: <Updated understanding based on Observation 1>
ACTION 2: <Next tool/command>
OBSERVATION 2: <Result>

FINAL ANSWER: <Synthesized deliverable citing supporting OBSERVATIONs>
```

Minimum 2 cycles before FINAL ANSWER. Every claim must cite which OBSERVATION supports it.

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
```
