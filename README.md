# Euxis

**Your Local-First AI Operating System**

![Banner][banner]

[![Version][version-badge]][version-url]
[![License][license-badge]][license-url]
[![Platform][platform-badge]][platform-url]
[![Agents][agents-badge]][agents-url]

## Overview

Deploy 24 specialist AI agents to plan, execute, and verify engineering tasks autonomously. From architecture to security to release management — every decision tracked, every outcome verified.

Build faster. Ship with confidence.

## Table of Contents

- [Overview](#overview)
- [Get Started](#get-started)
- [Architecture](#architecture)
- [The Fleet](#the-fleet)
- [Usage](#usage)
- [Memory](#memory)
- [Dispatch](#dispatch)
- [Self-Correction](#self-correction)
- [Intelligence Tiering](#intelligence-tiering)
- [Security](#security)
- [Squads](#squads)
- [Playbooks](#playbooks)
- [Combos](#combos)
- [CLI Reference](#cli-reference)
- [Directory Structure](#directory-structure)
- [Advanced](#advanced)
- [License](#license)

---

## Get Started

### Prerequisites

- Bash 4.0+ and Python 3.8+
- One or more AI providers:

| Provider | CLI | Best For |
|:---------|:----|:---------|
| [Claude][claude-url] | `claude` | Reasoning, architecture, strategy |
| [Gemini][gemini-url] | `gemini` | Research with large context |
| [Ollama][ollama-url] | `ollama` | Local inference, zero cost |
| [Codex CLI][codex-url] | `codex` | OpenAI models |
| [OpenCode][opencode-url] | `opencode` | Local code generation |

### Install

```bash
git clone https://github.com/sebastienrousseau/euxis.git ~/.euxis
~/.euxis/setup.sh
```

Add `~/bin` to your `PATH`:

```bash
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.profile && source ~/.profile
```

### Verify

```bash
euxis-health      # 7-point fleet integrity check
euxis-certify     # Full 4-gate certification pipeline
```

---

## Architecture

Euxis follows **Plan — Decompose — Delegate**.

| Layer | Component | Purpose |
|:------|:----------|:--------|
| Memory | **Cortex** | Tri-typed vector store with persistent semantic recall |
| Control | **Dispatcher** | Parallel execution of agent mission manifests |
| Interface | **Link** | Terminal, voice, and IDE integration |

```mermaid
flowchart LR
  U[User Goal] --> O[Orchestrator]
  O --> A[Architect]
  A --> M[Manifest]
  M --> D[Dispatcher]
  D --> F[Fleet]
  F --> C[Cortex]
  C --> O
```

---

## The Fleet

Three core agents govern. Twenty-one specialists execute.

### Core

| Agent | Domain |
|:------|:-------|
| `orchestrator` | Task decomposition, delegation, and synthesis |
| `architect` | Software architecture, patterns, and fleet dispatching |
| `librarian` | Memory optimization, documentation governance, compliance |

### Specialists

| Agent | Domain |
|:------|:-------|
| `automation-engineer` | CI/CD, IaC, Docker, Terraform |
| `bug-fixer` | Debugging, root cause analysis, surgical fixes |
| `butler` | TTS-optimized summarization for spoken output |
| `compliance-officer` | PII scanning, license auditing, GDPR/CCPA |
| `data-steward` | Observability, telemetry, structured logging |
| `deep-researcher` | Iterative multi-pass research with cross-validation |
| `devrel-advocate` | Developer relations, tutorials, demos |
| `edge-hunter` | Security analysis, boundary testing, vulnerability assessment |
| `globalization-lead` | i18n, l10n, RTL support, Unicode validation |
| `growth-marketer` | SEO, funnel optimization, go-to-market strategy |
| `incident-commander` | Incident response, root cause analysis, post-mortems |
| `legacy-maintainer` | Legacy code documentation, non-breaking upgrades |
| `perf-optimizer` | Latency, throughput, and memory profiling |
| `product-manager` | Requirements, user stories, prioritization |
| `qa-coordinator` | End-to-end testing coordination, quality gates |
| `release-manager` | Changelogs, semantic versioning, release coordination |
| `reviewer` | Quality gate validation and completeness checking |
| `social-manager` | Platform-native content and community engagement |
| `tech-writer` | Documentation, tutorials, API reference |
| `unit-tester` | Test coverage, reliability, regression prevention |
| `ux-sentinel` | Accessibility (WCAG 2.1 AA), responsive testing |

See the [Fleet Guide](FLEET_GUIDE.md) for delegation patterns, provider tiering, and workflow examples.

---

## Usage

### Route a task

```bash
euxis orchestrator "Refactor the login module to use JWT."
euxis architect "Review the authentication module."
euxis bug-fixer "Fix the null pointer in user.py."
```

### Research first, build second

```bash
euxis deep-researcher "Compare the top 3 Python PDF parsing libraries. Include benchmarks."
```

### Deploy the fleet

Generate a plan, then execute agents in parallel.

```bash
euxis architect "Audit this repo for security gaps. Output a MISSION MANIFEST." > plan.json
euxis-dispatch plan.json
```

### Convene experts

Run a structured 3-round adversarial debate.

```bash
euxis-council "Should we migrate from Postgres to MongoDB for logging?"
```

### Self-correcting loops

Run an agent with verification checkpoints that retry on failure.

```bash
euxis-loop bug-fixer "Fix parser.py" "pytest tests/test_parser.py" 3
```

---

## Memory

The Cortex provides persistent semantic memory across sessions using three types.

| Type | Purpose | Example |
|:-----|:--------|:--------|
| `episodic` | Specific events and outcomes | "Bug #42 fixed by null-check in auth.py" |
| `semantic` | Persistent facts and relationships | "The auth module uses JWT with RS256" |
| `procedural` | Reusable workflows and contraindications | "Never retry with expired tokens" |

```bash
euxis-cortex remember "Project uses hexagonal architecture" "architect" --type semantic
euxis-cortex recall "authentication" --type procedural
euxis-cortex stats
```

---

## Dispatch

The fleet supports three coordination modes.

| Mode | Behavior |
|:-----|:----------|
| `hierarchical` | All agents report to the orchestrator. Default. |
| `mesh` | Agents with dispatch authority coordinate sub-workflows directly. |
| `federated` | Agents operate autonomously across project boundaries. |

```bash
euxis-dispatch plan.json                      # hierarchical
euxis-dispatch --mode mesh plan.json          # peer-to-peer
euxis-dispatch --mode federated plan.json     # cross-project
```

Dispatch-authority agents: `architect`, `qa-coordinator`, `incident-commander`, `release-manager`.

---

## Self-Correction

Every agent applies three verification layers before delivering output.

1. **Internal Consistency.** Every claim is supported by a ReAct observation. No fabricated paths.
2. **Cross-Reference.** Key findings are checked against Cortex memories. Contradictions are flagged.
3. **Evaluator Checkpoint.** The `reviewer` validates synthesized outputs before delivery.

On failure, agents generate structured reflections stored as procedural memory:

```
REFLEXION:          Root cause — not the symptom
EVIDENCE:           Specific observation that revealed the failure
STRATEGY:           Concrete alternative approach
CONTRAINDICATION:   What to never repeat
```

---

## Intelligence Tiering

Agents route automatically to the optimal tier.

| Tier | Agents | Provider | Rationale |
|:-----|:-------|:---------|:----------|
| Strategic | orchestrator, architect, product-manager, reviewer | `claude` | Strongest reasoning |
| Research | deep-researcher | `gemini` | 2M token context window |
| Coding | bug-fixer, legacy-maintainer | `opencode` | Fast local code models |
| Utility | butler, librarian | `ollama` | Zero latency, no cost |
| Standard | All others | `claude` | General-purpose fallback |

An explicit provider argument always overrides tiering. Tasks marked P0 always route to the Strategic tier.

```bash
euxis architect "Review the auth module"          # auto → claude
euxis bug-fixer "Fix user.py" gemini              # explicit override
```

---

## Security

Euxis is built for zero-trust environments.

- **Input Validation.** Agent names validated against `[a-zA-Z0-9_-]`. Shell metacharacters rejected.
- **Script Hardening.** Every bash script enforces `set -euo pipefail`.
- **Audit Trails.** Every action logged to `~/.euxis/projects/<project>/<agent>/audit.md`.
- **Human-in-the-Loop.** `euxis-sync-docs` validates AI output against forbidden patterns and requires explicit approval.
- **Security Probes.** `euxis-audit-run` tests for shell injection, path traversal, and null byte injection.

---

## Squads

Four cross-functional teams. Each with a lead and a clear purpose.

| Squad | Purpose | Lead | Members |
|:------|:--------|:-----|:--------|
| Vision | Strategy | `orchestrator` | orchestrator, architect, product-manager, deep-researcher |
| Build | Execution | `bug-fixer` | bug-fixer, legacy-maintainer, automation-engineer, unit-tester |
| Quality | Assurance | `reviewer` | reviewer, qa-coordinator, edge-hunter, compliance-officer, perf-optimizer |
| Growth | Amplification | `tech-writer` | tech-writer, brand-evangelist, social-manager, devrel-advocate, growth-marketer, ux-sentinel, globalization-lead |

```bash
euxis-squad list                            # All squads
euxis-squad info vision                     # Details
euxis-squad deploy build "Fix auth module"  # Deploy
euxis-squad validate                        # Cross-check against registry
```

---

## Playbooks

Phased squad activations for repeatable workflows. Each phase generates a dispatch manifest with checkpoints.

| Playbook | Sequence | Use Case |
|:---------|:---------|:---------|
| Zero to One | Vision → Build → Quality → Growth | Full product launch |
| Legacy Overhaul | Build → Quality → Vision | Modernize legacy systems |
| Red Alert | Quality → Build → Vision | Emergency incident response |

```bash
euxis-playbook list                                             # Available playbooks
euxis-playbook info zero-to-one                                 # Phase breakdown
euxis-playbook run zero-to-one "Launch auth service" --dry-run  # Preview manifests
euxis-playbook run zero-to-one "Launch auth service"            # Execute all phases
euxis-playbook status                                           # Session history
```

---

## Combos

Sequential agent chains. Each agent receives the previous agent's output as context. Fast, focused, no manifest overhead.

| Combo | Chain | Use Case |
|:------|:------|:---------|
| Steve Jobs | product-manager → architect → brand-evangelist → reviewer | Vision to polished review |
| Fort Knox | edge-hunter → compliance-officer → qa-coordinator → reviewer | Maximum security assurance |
| Content Factory | tech-writer → brand-evangelist → social-manager → reviewer | End-to-end content production |

```bash
euxis-combo list                                      # Available combos
euxis-combo info steve-jobs                           # Chain detail
euxis-combo run steve-jobs "Design a new onboarding"  # Execute chain
```

---

## CLI Reference

### Core

| Command | Description |
|:--------|:------------|
| `euxis <agent> <task> [provider]` | Route a task to any agent |
| `euxis-ui` | Interactive Mission Control TUI |

### Quality

| Command | Description |
|:--------|:------------|
| `euxis-health` | Fleet integrity check |
| `euxis-lint` | Static analysis |
| `euxis-certify` | Full certification pipeline |
| `euxis-test-infra` | Infrastructure unit tests |
| `euxis-bench` | Performance benchmarking |
| `euxis-audit-run` | Security audit with probes |

### Orchestration

| Command | Description |
|:--------|:------------|
| `euxis-dispatch [--mode MODE] <manifest>` | Parallel agent execution |
| `euxis-loop <agent> <task> <verify> [retries]` | Autonomous retry loop |
| `euxis-council "<topic>"` | Multi-agent adversarial debate |
| `euxis-bus <cmd> [args]` | Async pub/sub message bus |
| `euxis-graph <cmd> [args]` | GraphRAG knowledge graph |
| `euxis-squad <cmd> [args]` | Squad management and deployment |
| `euxis-playbook <cmd> [args]` | Phased squad execution via playbooks |
| `euxis-combo <cmd> [args]` | Sequential agent chain execution |
| `euxis-synthesize <cmd> [args]` | Dynamic agent composition |

### Memory

| Command | Description |
|:--------|:------------|
| `euxis-cortex <cmd> [args] [--type TYPE]` | Tri-typed vector memory |

### Maintenance

| Command | Description |
|:--------|:------------|
| `euxis-kaizen` | Continuous self-improvement cycle |
| `euxis-daemon [interval]` | Periodic kaizen with fail-safe |
| `euxis-optimize` | System-wide tune-up |
| `euxis-sync-docs` | Documentation sync with approval gate |
| `euxis-deploy` | Docker Compose enterprise deployment |

### Interface

| Command | Description |
|:--------|:------------|
| `euxis-voice` | Offline voice interface (Whisper + Piper) |
| `euxis-gym <agent> <test> [provider]` | Agent evaluation and A/B testing |

---

## Directory Structure

```
~/.euxis/
├── bin/                    Executable tools (symlinked to ~/bin/)
│   ├── euxis.sh            Core CLI — routing, prompt assembly, output capture
│   ├── euxis-ui            Mission Control TUI
│   ├── euxis-cortex        Semantic memory (vector store)
│   ├── euxis-dispatch      Parallel agent execution
│   ├── euxis-loop          Autonomous retry loop
│   ├── euxis-council       Multi-agent consensus
│   ├── euxis-bus           Async message bus
│   ├── euxis-graph         GraphRAG knowledge graph
│   ├── euxis-synthesize    Dynamic agent composition
│   ├── euxis-squad         Squad management and deployment
│   ├── euxis-playbook      Phased squad execution
│   ├── euxis-combo         Sequential agent chains
│   └── ...                 23 tools total
├── squads.json             Squad and combo registry
├── playbooks/              Phased squad activation definitions
│   ├── zero-to-one.json    Vision → Build → Quality → Growth
│   ├── legacy-overhaul.json Build → Quality → Vision
│   └── red-alert.json      Quality → Build → Vision
├── prompts/
│   ├── core/               Orchestrator, Architect, Librarian
│   ├── fleet/              21 specialist agents
│   └── protocols/          Shared protocol and common instructions
├── tests/
│   └── golden/             Golden datasets for evaluation
├── registry.json           Master agent registry
├── USER_GUIDE.md           Complete CLI and agent reference
├── FLEET_GUIDE.md          Delegation patterns and workflows
└── cortex_db/              ChromaDB vector database (git-ignored)
```

---

## Advanced

### Voice Mode

Run completely offline with Piper TTS and Faster-Whisper.

```bash
euxis-voice
```

### Continuous Improvement

```bash
euxis-kaizen              # 4-gate improvement cycle
euxis-daemon              # Periodic kaizen (default: 30 min)
euxis-daemon 3600         # Custom interval in seconds
```

### Conflict Resolution

When agents produce conflicting outputs:

1. **Domain Priority.** Primary scope agent wins.
2. **Evidence Weight.** Verified data over inference over heuristic.
3. **Negotiation Round.** Agents produce conflict responses (max 1 round).
4. **Human Escalation.** Both positions presented if unresolved.

---

## License

Copyright (c) 2026 Sebastien Rousseau. All rights reserved.

Governed by the `librarian` agent. Documentation auto-synced via `euxis-sync-docs`.

<!-- Reference Links -->

[banner]: https://raw.githubusercontent.com/sebastienrousseau/euxis/main/.github/assets/banner.svg "Euxis — Your Local-First AI Operating System"

[version-badge]: https://img.shields.io/badge/version-6.0-blue?style=for-the-badge
[version-url]: https://github.com/sebastienrousseau/euxis/releases

[license-badge]: https://img.shields.io/badge/license-proprietary-green?style=for-the-badge
[license-url]: https://github.com/sebastienrousseau/euxis/blob/main/LICENSE

[platform-badge]: https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20WSL-lightgrey?style=for-the-badge
[platform-url]: https://github.com/sebastienrousseau/euxis

[agents-badge]: https://img.shields.io/badge/agents-24-blueviolet?style=for-the-badge
[agents-url]: https://github.com/sebastienrousseau/euxis/blob/main/FLEET_GUIDE.md

[claude-url]: https://docs.anthropic.com/en/docs/claude-cli
[gemini-url]: https://github.com/google-gemini/gemini-cli
[ollama-url]: https://ollama.com/
[codex-url]: https://github.com/openai/codex
[opencode-url]: https://github.com/opencode-ai/opencode
