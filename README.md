# Euxis Fleet: The Autonomous Agent OS

![Version](https://img.shields.io/badge/version-v4.5--STABLE-blue.svg) ![Coverage](https://img.shields.io/badge/coverage-100%25-brightgreen) ![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20WSL-lightgrey)

> **"Not just a tool, but a workforce."**

Euxis is a local-first, privacy-centric **Operating System for AI Agents**. It orchestrates a fleet of 22 specialized personas (Architect, Researcher, Engineer) to plan, execute, and verify complex engineering tasks autonomously.

Unlike standard "Chat with PDF" tools, Euxis features **Infinite Context (Vector Memory)**, **Parallel Dispatch**, and **Self-Healing Infrastructure**.

---

## System Architecture

Euxis operates on a "Plan-Decompose-Delegate" architecture:

1. **The Cortex (Memory):** A local Vector Database (ChromaDB) that gives agents "Photographic Memory" across sessions.
2. **The Dispatcher (Control):** The Architect breaks tasks into a JSON Manifest; the Dispatcher executes them in parallel background loops.
3. **The Link (Interface):** Connects to your terminal (CLI), your voice (Local Whisper/Piper), and your IDE (MCP).

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

#### Fleet (19)

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
| `legacy-maintainer` | Legacy code documentation, non-breaking upgrades |
| `perf-optimizer` | Latency, throughput, memory profiling |
| `product-manager` | Requirements, user stories, MoSCoW prioritization |
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
    fleet/              All 19 specialist agents
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

# Create ~/bin if it does not exist
mkdir -p ~/bin

# Symlink all tools
for script in ~/.euxis/bin/*; do
    name=$(basename "$script" .sh)
    ln -sf "$script" ~/bin/"$name"
done

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

For massive refactors, generate a plan and fire agents in parallel.

```bash
# 1. Plan
euxis architect "Audit this repo for security gaps. Output a MISSION MANIFEST." > plan.json

# 2. Execute
euxis-dispatch plan.json
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

## Intelligence Tiering

When no provider is specified, agents are auto-routed to the optimal provider:

| Tier | Agents | Provider | Reason |
|------|--------|----------|--------|
| Strategic | orchestrator, architect, product-manager, reviewer | `claude` | Best reasoning and tool use |
| Research | deep-researcher | `gemini` | 2M context window for massive analysis |
| Utility | butler, librarian | `ollama` | Zero latency, no cost for summaries |
| Coding | bug-fixer, legacy-maintainer | `opencode` | Fast local code models for diffs |
| Default | all others | `claude` | General-purpose fallback |

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

### The Cortex (Memory)

Manage the vector database manually if needed:

```bash
euxis-cortex remember "Project uses hexagonal architecture" "architect"
euxis-cortex recall "authentication module architecture"
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
| `euxis-dispatch` | Parallel agent execution |
| `euxis-cortex <cmd> [args]` | Vector memory operations |
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
