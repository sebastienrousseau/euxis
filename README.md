# Euxis Fleet: The Autonomous Agent OS

![Version](https://img.shields.io/badge/version-v6.0--STABLE-blue.svg) ![Coverage](https://img.shields.io/badge/coverage-100%25-brightgreen) ![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20WSL-lightgrey)

> **"Not just a tool, but a workforce."**

Euxis is a local-first, privacy-centric **Operating System for AI Agents**. It orchestrates a fleet of 24 specialized personas (Architect, Researcher, Engineer) to plan, execute, and verify complex engineering tasks autonomously.

Unlike standard "Chat with PDF" tools, Euxis features **Infinite Context (Vector Memory)**, **Parallel Dispatch**, and **Self-Healing Infrastructure**.

---

## System Architecture

Euxis operates on a "Plan-Decompose-Delegate" architecture:

1. **The Cortex (Memory):** A local Vector Database (ChromaDB) that gives agents "Photographic Memory" across sessions.
2. **The Dispatcher (Control):** The Architect breaks tasks into a JSON Manifest; the Dispatcher executes them in parallel background loops.
3. **The Link (Interface):** Connects to your terminal (CLI), your voice (Local Whisper/Piper), and your IDE (MCP).

```mermaid
flowchart LR
  U[User Goal] --> O[Orchestrator]
  O --> A[Architect]
  A --> M[Manifest.json]
  M --> D[Dispatcher]
  D --> F[Fleet Agents]
  F --> C[Cortex Memory]
  C --> O
```

### The Fleet (Key Agents)

| Agent | Role | Capability |
| :--- | :--- | :--- |
| **Orchestrator** | Strategy | The "CEO" that breaks down high-level goals. |
| **Architect** | Design | Analyzes codebases and generates `manifest.json` for dispatch. |
| **Deep Researcher** | R&D | Autonomous web scraping & citation-backed reporting. |
| **Bug Fixer** | Engineering | Writes code with a self-correcting `pytest` loop. |
| **Librarian** | Governance | Enforces copyright, versioning, and documentation standards. |

### Full Agent Fleet

#### Core (3)

| Agent | Role |
|-------|------|
| `orchestrator` | Task decomposition, delegation, synthesis across the fleet |
| `architect` | Software architecture, patterns, ADRs, refactoring |
| `librarian` | Context architect, compliance custodian, memory optimizer |

#### Fleet (21)

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

---

## Directory Structure

```
~/.euxis/
  prompts/
    core/               Core agents (orchestrator, architect, librarian)
    fleet/              All 21 specialist agents
    protocols/          Shared protocol and common instructions
      _protocol.txt     Mandatory protocol (appended at runtime to all agents)
      _common.txt       Shared agent instructions (deduplication snippet)
  bin/                  Executable tools (symlinked to ~/bin/)
    euxis.sh            Core CLI — agent routing, prompt assembly, output capture
    euxis-ui            Mission Control TUI
    euxis-gym           Automated agent evaluation
    euxis-council       Multi-agent consensus protocol
    euxis-loop          Autonomous try-catch-retry
    euxis-lint          Static analysis for agent fleet
    euxis-test-infra    Infrastructure unit tests
    euxis-certify       Full certification pipeline
    euxis-cortex        Cross-platform semantic memory (vector store)
    euxis-voice         Voice interface (Whisper STT + Piper TTS)
    euxis-health        Fleet integrity and configuration health check
    euxis-bench         Performance benchmarking
    euxis-audit-run     Deep-dive audit runner
    euxis-sync-docs     Librarian-powered documentation sync
    euxis-kaizen        Continuous self-improvement cycle
    euxis-daemon        Periodic kaizen with fail-safe halting
    euxis-deploy        Docker Compose enterprise deployment
    euxis-dispatch      Parallel agent execution from manifest
    euxis-optimize      System-wide tune-up (certify, GC, compress, QA)
  tests/
    golden/             Golden datasets for agent evaluation
    coverage/           Test coverage reports (future)
  registry.json         Master agent registry with metadata
  USER_GUIDE.md         Complete CLI and agent reference
  cortex_db/            ChromaDB vector database (auto-created, git-ignored)
  projects/             Per-project audit logs and memory (git-ignored)
```

---

## Installation & Setup

### Prerequisites

- **Bash 4.0+** and **Python 3.8+**
- One or more AI providers installed:
  - [Claude CLI](https://docs.anthropic.com/en/docs/claude-cli) (`claude`) — Primary
  - [Gemini CLI](https://github.com/google-gemini/gemini-cli) (`gemini`) — Research
  - [Ollama](https://ollama.com/) (`ollama`) — Local, zero-cost
  - [shell-gpt](https://github.com/TheR1D/shell_gpt) (`sgpt`) — OpenAI
  - [OpenCode](https://github.com/opencode-ai/opencode) (`opencode`) — Local coding

### 1. Bootstrap

```bash
# Clone the core
git clone https://github.com/sebastienrousseau/euxis.git ~/.euxis

# One-line install
~/.euxis/setup.sh

# Add ~/bin to PATH (if not already present)
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.profile
source ~/.profile
```

### 2. Verify Integrity

Euxis includes a BIOS-level health check.

```bash
euxis-health
# [1/7] Naming Consistency...       All agent filenames match their agent_id.
# [2/7] Script Hardening...         All bash scripts have hardening flags.
# [3/7] Orphan Detection...         All agents are registered. No orphans or ghosts.
# [4/7] Header Schema Validation... All agents have complete headers.
# [5/7] Documentation Drift...      USER_GUIDE.md is current.
# [6/7] Certification Readiness...  All tools present and executable.
# [7/7] Provider Connectivity...    3 of 5 providers available.
# HEALTH CHECK PASSED. Fleet integrity: 100%.
```

### 3. Full Certification

```bash
euxis-certify
# [GATE 1+2] Static Analysis (Lint)...  PASSED
# [GATE 3]   Infrastructure Tests...    7/7 PASSED
# [GATE 4]   Semantic Verification...   PASSED
# EUXIS IS CERTIFIED.
```

---

## Usage

### Level 1: Single-Shot Command

Ask the fleet to handle a specific task.

```bash
euxis orchestrator "Refactor the login module to use JWT instead of Sessions."
euxis architect "Review the authentication module"
euxis bug-fixer "Fix the null pointer in user.py"
```

### Level 2: Deep Research (The "Scientist" Mode)

Gather intelligence before coding.

```bash
euxis deep-researcher "Analyze the top 3 Python PDF parsing libraries in 2026. Compare benchmarks."
```

### Level 3: Enterprise Dispatch (The "Manager" Mode)

For massive refactors, generate a plan and fire agents in parallel. Supports three dispatch modes.

```bash
# 1. Plan
euxis architect "Audit this repo for security gaps. Output a MISSION MANIFEST." > plan.json

# 2. Execute (3 modes available)
euxis-dispatch plan.json                          # hierarchical (default)
euxis-dispatch --mode mesh plan.json              # peer-to-peer sub-workflows
euxis-dispatch --mode federated plan.json         # cross-project autonomous
```

### Level 4: Multi-Agent Debate

Convene experts for adversarial consensus on architectural decisions.

```bash
euxis-council "Should we migrate from Postgres to MongoDB for logging?"
```

### Level 5: Autonomous Self-Correction

Run an agent in a retry loop with verification checkpoints.

```bash
euxis-loop bug-fixer "Fix parser.py" "pytest tests/test_parser.py" 3

euxis-loop --checkpoints bug-fixer "Fix parser.py" 3 \
  "python -c 'import parser'" \
  "pytest tests/test_parser.py" \
  "mypy parser.py"
```

---

## v5.0 Architecture Features

### Hybrid Dispatch Architecture

The fleet supports three dispatch modes via `euxis-dispatch --mode <mode>`:

| Mode | Description | Use When |
|------|-------------|----------|
| `hierarchical` (default) | All agents report to orchestrator | Standard coordination |
| `mesh` | Dispatch-authority agents coordinate sub-workflows directly | Specialists can manage focused sub-workflows |
| `federated` | Agents operate autonomously across project boundaries | Cross-project tasks |

**Dispatch-authority agents (mesh mode):** `architect`, `qa-coordinator`, `incident-commander`, `release-manager`

```bash
euxis-dispatch manifest.json                      # hierarchical (default)
euxis-dispatch --mode mesh manifest.json           # peer-to-peer
euxis-dispatch --mode federated manifest.json      # cross-project
```

### Tri-Typed Memory System

The Cortex classifies all memories into three types:

| Type | Description | Example |
|------|-------------|---------|
| `episodic` | Specific events and outcomes from a session | `"Bug #42 fixed by null-check in auth.py line 89"` |
| `semantic` | General facts that persist across sessions | `"The auth module uses JWT tokens with RS256 signing"` |
| `procedural` | Reusable workflows and contraindications | `"CONTRAINDICATION: Never retry with expired tokens"` |

```bash
euxis-cortex remember "Fixed null pointer in auth.py" "bug-fixer" --type episodic
euxis-cortex recall "authentication" --type procedural
```

### 3-Layer Self-Correction

Every agent applies layered verification before producing output:

1. **Layer 1 — Internal Consistency:** Every claim supported by a ReAct OBSERVATION. No fabricated paths.
2. **Layer 2 — Cross-Reference:** Key findings cross-referenced against Cortex memories. Contradictions flagged.
3. **Layer 3 — Evaluator Checkpoint:** The `reviewer` validates synthesized outputs before user delivery.

### Reflexion Protocol

On WARNING or FAILURE, agents generate structured analysis stored as PROCEDURAL memory:

```
REFLEXION: <Root cause — not the symptom>
EVIDENCE: <Specific OBSERVATION that revealed the failure>
STRATEGY: <Concrete alternative approach>
CONTRAINDICATION: <What approach to NEVER repeat>
```

### Conflict Resolution

When multiple agents produce conflicting outputs, resolution follows:

1. **Domain Priority** — Primary scope agent wins (security → `edge-hunter`, architecture → `architect`)
2. **Evidence Weight** — Verified data > inference > heuristic
3. **Negotiation Round** — Agents produce `CONFLICT_RESPONSE` blocks (max 1 round)
4. **Human Escalation** — Both positions presented to user if unresolved

### ReAct Reasoning Loop

All agents use the ReAct (Reasoning + Acting) loop for non-trivial tasks, iterating through THOUGHT → ACTION → OBSERVATION cycles (minimum 2) before producing a FINAL ANSWER that cites supporting observations.

---

## Intelligence Tiering (5-Tier Cost-Optimized)

When no provider is specified, agents are auto-routed to the optimal provider:

| Tier | Agents | Provider | Reason |
|------|--------|----------|--------|
| Strategic | orchestrator, architect, product-manager, reviewer | `claude` | Best reasoning and tool use |
| Research | deep-researcher | `gemini` | 2M context window for massive analysis |
| Coding | bug-fixer, legacy-maintainer | `opencode` | Fast local code models for diffs |
| Utility | butler, librarian | `ollama` | Zero latency, no cost for summaries |
| Standard | all others | `claude` | General-purpose fallback |

**P0 Override:** Tasks marked P0 (CRITICAL) always route to the Strategic tier regardless of agent.

An explicit provider argument always overrides tiering.

```bash
euxis architect "Review the auth module"          # auto: claude
euxis bug-fixer "Fix user.py" gemini              # explicit override
```

---

## Security & Governance

Euxis is designed for Zero-Trust environments:

- **Input Sanitization:** All agent names are validated against `[a-zA-Z0-9_-]`. Shell metacharacters are rejected.
- **Script Hardening:** Every bash script enforces `set -uo pipefail`.
- **Audit Trails:** Every action is logged to `~/.euxis/projects/<project>/<agent>/audit.md`.
- **Human-in-the-Loop:** `euxis-sync-docs` validates LLM output against forbidden patterns and requires explicit user approval before execution.
- **Security Probes:** `euxis-audit-run` performs shell injection, path traversal, and null byte injection tests.

---

## Advanced Configuration

### The Cortex (Tri-Typed Memory)

Manage the vector database with typed memories:

```bash
# Store with type classification
euxis-cortex remember "Project uses hexagonal architecture" "architect" --type semantic
euxis-cortex remember "Deployed v5.0, all checks passed" "release-manager" --type episodic
euxis-cortex remember "To fix flaky tests: isolate state → add retry → check ordering" "unit-tester" --type procedural

# Recall with optional type filter
euxis-cortex recall "authentication module architecture"
euxis-cortex recall "deployment workflow" --type procedural

# Database management
euxis-cortex stats
euxis-cortex forget "obsolete fact"
```

### Voice Mode (Offline)

Run completely offline using Piper TTS and Faster-Whisper.

```bash
euxis-voice
```

Prerequisites:
- macOS: `brew install sox && pip install openai sounddevice numpy scipy`
- Linux/WSL: `sudo apt install libportaudio2 sox mpg123 && pip install openai sounddevice numpy scipy`

### Continuous Self-Improvement

```bash
euxis-kaizen                # 4-gate improvement cycle
euxis-daemon                # Periodic kaizen every 30 minutes
euxis-daemon 3600           # Custom interval (seconds)
```

### Performance Benchmarking

```bash
euxis-bench                 # Health, lint, cortex, provider latency
euxis-audit-run             # Full deep-dive audit with readiness report
```

---

## CLI Reference

| Command | Description |
|---------|-------------|
| `euxis <agent> <task> [provider]` | Route a task to any agent |
| `euxis-health` | Fleet health check (7 checks) |
| `euxis-lint` | Static analysis (registry, protocol, versions, permissions) |
| `euxis-certify` | Full certification pipeline (4 gates) |
| `euxis-test-infra` | Infrastructure unit tests |
| `euxis-bench` | Performance benchmarking |
| `euxis-audit-run` | Deep-dive audit with security probes |
| `euxis-council "<topic>"` | Multi-agent adversarial debate |
| `euxis-loop <agent> <task> <verify> [retries]` | Autonomous retry loop |
| `euxis-dispatch [--mode MODE] <manifest>` | Parallel agent execution (hierarchical/mesh/federated) |
| `euxis-cortex <cmd> [args] [--type TYPE]` | Tri-typed vector memory operations |
| `euxis-sync-docs` | Librarian-powered doc sync (HITL) |
| `euxis-voice` | Voice interface (Whisper + Piper) |
| `euxis-ui` | Mission Control TUI |
| `euxis-gym <agent> <test> [provider]` | Agent evaluation & A/B testing |
| `euxis-kaizen` | Continuous self-improvement cycle |
| `euxis-daemon [interval]` | Periodic kaizen with fail-safe |
| `euxis-deploy` | Docker Compose enterprise deployment |
| `euxis-optimize` | System-wide tune-up (certify, GC, compress, QA) |

---

## License & Compliance

Copyright (c) 2026 Sebastien Rousseau. All rights reserved.

Governed by the `librarian` agent. Documentation auto-synced via `euxis-sync-docs`.
