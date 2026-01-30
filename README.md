# Euxis

Enterprise multi-provider AI agent framework. 22 specialized agents with persistent audit logs, Cortex vector memory, ReAct reasoning loops, voice interface, and automated evaluation.

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
    euxis-voice         Voice interface (Whisper STT + OpenAI TTS)
    euxis-health        Fleet integrity and configuration health check
  tests/
    golden/             Golden datasets for agent evaluation
    coverage/           Test coverage reports (future)
  registry.json         Master agent registry with metadata
  cortex_db/            ChromaDB vector database (auto-created, git-ignored)
  projects/             Per-project audit logs and memory (git-ignored, local only)
    <project>/
      <agent>/
        audit.md        Immutable session log
        memory.md       Persistent knowledge store
        output/         Session output files
```

## Agent Fleet

### Orchestration

| Agent | Role |
|-------|------|
| `orchestrator` | Task decomposition, delegation, synthesis across the fleet |

### Engineering & Quality

| Agent | Role |
|-------|------|
| `architect` | Software architecture, patterns, ADRs, refactoring |
| `bug-fixer` | Debugging, root cause analysis, surgical fixes |
| `legacy-maintainer` | Legacy code documentation, non-breaking upgrades, migrations |
| `release-manager` | Changelogs, semantic versioning, release coordination |
| `automation-engineer` | CI/CD pipelines, IaC, Docker, Terraform |
| `perf-optimizer` | Latency, throughput, memory profiling, performance budgets |
| `unit-tester` | Test coverage, test reliability, regression prevention |
| `reviewer` | Quality gate, output validation, completeness checking |
| `librarian` | Memory optimization, knowledge compression, deduplication |

### Security & Compliance

| Agent | Role |
|-------|------|
| `edge-hunter` | Security analysis, boundary testing, vulnerability assessment |
| `compliance-officer` | PII scanning, license auditing, GDPR/CCPA compliance |

### Content & Documentation

| Agent | Role |
|-------|------|
| `tech-writer` | Documentation, tutorials, API reference, glossary |
| `globalization-lead` | i18n, l10n, RTL support, Unicode validation |
| `data-steward` | Observability, telemetry, structured logging |
| `ux-sentinel` | Accessibility (WCAG 2.1 AA), design system, responsive testing |
| `product-manager` | Requirements, user stories, MoSCoW prioritization |

### Research

| Agent | Role |
|-------|------|
| `deep-researcher` | Iterative multi-pass research with cross-validation and citation |

### Voice Interface

| Agent | Role |
|-------|------|
| `butler` | TTS-optimized summarization, spoken English output |

### Growth & Community

| Agent | Role |
|-------|------|
| `growth-marketer` | SEO, AARRR funnel, CRO, GTM strategy |
| `social-manager` | Platform-native content, content calendars, community engagement |
| `devrel-advocate` | Developer relations, tutorials, demos, conferences |

## Installation

### Prerequisites

- Bash 4.0+
- Python 3.8+
- One or more AI providers installed:
  - [Claude CLI](https://docs.anthropic.com/en/docs/claude-cli) (`claude`)
  - [Gemini CLI](https://github.com/google-gemini/gemini-cli) (`gemini`)
  - [shell-gpt](https://github.com/TheR1D/shell_gpt) (`sgpt`)
- Cortex dependencies (optional, for semantic memory):
  - macOS: `pip install chromadb sentence-transformers`
  - Linux/WSL: `sudo apt-get install -y build-essential python3-dev && pip install chromadb sentence-transformers`

### Setup

```bash
# Clone the repository
git clone https://github.com/sebastienrousseau/euxis.git ~/.euxis

# Create ~/bin if it does not exist
mkdir -p ~/bin

# Symlink all tools
ln -sf ~/.euxis/bin/euxis.sh         ~/bin/euxis
ln -sf ~/.euxis/bin/euxis-ui         ~/bin/euxis-ui
ln -sf ~/.euxis/bin/euxis-gym        ~/bin/euxis-gym
ln -sf ~/.euxis/bin/euxis-council    ~/bin/euxis-council
ln -sf ~/.euxis/bin/euxis-loop       ~/bin/euxis-loop
ln -sf ~/.euxis/bin/euxis-lint       ~/bin/euxis-lint
ln -sf ~/.euxis/bin/euxis-test-infra ~/bin/euxis-test-infra
ln -sf ~/.euxis/bin/euxis-certify    ~/bin/euxis-certify
ln -sf ~/.euxis/bin/euxis-cortex     ~/bin/euxis-cortex
ln -sf ~/.euxis/bin/euxis-voice      ~/bin/euxis-voice
ln -sf ~/.euxis/bin/euxis-health     ~/bin/euxis-health

# Add ~/bin to PATH (if not already present)
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

## CLI Tools

### `euxis` — Core Agent Runner

```bash
euxis <agent> <task> [provider]
euxis delegate <agent> <task> [provider]
```

| Argument | Description | Default |
|----------|-------------|---------|
| `agent` | Agent name (matches filename in `prompts/{core,fleet}/`) | required |
| `task` | Task description or instruction | required |
| `provider` | AI provider: `claude`, `gemini`, `openai`, `ollama`, `opencode` | auto-tiered |

When `provider` is omitted, **Intelligence Tiering** auto-selects the optimal provider:

| Tier | Agents | Provider | Reason |
|------|--------|----------|--------|
| Strategic | orchestrator, architect, product-manager, reviewer | `claude` | Best reasoning and tool use |
| Research | deep-researcher | `gemini` | 2M context window for massive analysis |
| Utility | butler, librarian | `ollama` | Zero latency, no cost for summaries |
| Coding | bug-fixer, legacy-maintainer | `opencode` | Fast local code models for diffs |
| Default | all others | `claude` | General-purpose fallback |

An explicit provider argument always overrides tiering.

```bash
euxis architect "Review the authentication module"          # auto: claude
euxis bug-fixer "Fix the null pointer in user.py"           # auto: opencode
euxis bug-fixer "Fix the null pointer in user.py" gemini    # explicit override
euxis delegate perf-optimizer "Profile the API layer"
```

### `euxis-cortex` — Cross-Platform Semantic Memory

Vector-based memory store using ChromaDB and sentence-transformers. Works on macOS, Linux, and WSL.

```bash
euxis-cortex remember "fact to store" [source_agent]
euxis-cortex recall   "query keywords" [n_results]
euxis-cortex stats
euxis-cortex forget   "exact document text"
```

```bash
# Store a fact from the architect agent
euxis-cortex remember "Project uses hexagonal architecture with ports/adapters" "architect"

# Semantic recall before starting a task
euxis-cortex recall "authentication module architecture"

# Check database stats
euxis-cortex stats
```

### `euxis-ui` — Mission Control TUI

Interactive terminal interface for deploying agents, switching providers, and browsing audit logs.

```bash
euxis-ui
```

### `euxis-gym` — Agent Evaluation & A/B Testing

Runs an agent against a golden test case and scores the output 0-100 using a judge model. Supports A/B testing across providers.

```bash
euxis-gym <agent> <test_case_path> [provider]
euxis-gym --ab <agent> <test_case_path> <provider_a> <provider_b>
```

```bash
# Single evaluation
euxis-gym bug-fixer ~/.euxis/tests/golden/bug-fixer-01.txt

# A/B test: compare Claude vs Gemini on the same task
euxis-gym --ab architect ~/.euxis/tests/golden/architect-01.txt claude gemini
```

Test case format:
```
INPUT: <task for the agent>
EXPECTED_OUTPUT: <criteria the output must satisfy>
```

### `euxis-council` — Multi-Agent Debate & Consensus

Convenes architect, edge-hunter, and perf-optimizer for a 3-round adversarial debate. Round 1 gathers independent opinions, Round 2 has each expert rebut the others, and Round 3 has the orchestrator synthesize a final binding decision.

```bash
euxis-council "<topic>"
```

```bash
euxis-council "Should we migrate from Postgres to MongoDB for logging?"
```

### `euxis-loop` — Autonomous Self-Correction with Reflexion

Runs an agent, executes verification checkpoints, and retries on failure — injecting both error logs and the agent's own REFLECTION entries from memory into the retry prompt.

```bash
# Single verification command
euxis-loop <agent> <task> <verification_command> [max_retries]

# Multi-checkpoint mode (catches errors at intermediate steps)
euxis-loop --checkpoints <agent> <task> <max_retries> <cmd1> <cmd2> ...

# Human-in-the-loop (pauses for confirmation before each retry)
euxis-loop --confirm <agent> <task> <verify_cmd> [max_retries]
```

```bash
euxis-loop bug-fixer "Fix parser.py" "pytest tests/test_parser.py" 3

euxis-loop --checkpoints bug-fixer "Fix parser.py" 3 \
  "python -c 'import parser'" \
  "pytest tests/test_parser.py" \
  "mypy parser.py"
```

### `euxis-voice` — Voice Interface

Records microphone input, transcribes via Whisper, routes through the orchestrator, summarizes with the butler agent, and speaks the result via OpenAI TTS.

```bash
euxis-voice
```

Prerequisites:
- macOS: `brew install sox && pip install openai sounddevice numpy scipy`
- Linux/WSL: `sudo apt install libportaudio2 sox mpg123 && pip install openai sounddevice numpy scipy`
- `OPENAI_API_KEY` environment variable must be set.

### `euxis-lint` — Fleet Static Analysis

Validates agent registry integrity, protocol compliance, and version synchronization.

```bash
euxis-lint
```

### `euxis-test-infra` — Infrastructure Unit Tests

Tests argument validation, provider routing, protocol injection, and space handling.

```bash
euxis-test-infra
```

### `euxis-certify` — Full Certification Pipeline

Runs all toll gates (lint, infra tests, semantic verification) in sequence. Green across the board means the fleet is production-ready.

```bash
euxis-certify
```

### `euxis-health` — Fleet Health Check

Validates fleet integrity: naming consistency, script hardening, orphan detection, header schema validation, and certification readiness.

```bash
euxis-health
```

Checks performed:
1. Filename vs `agent_id` consistency
2. Bash script hardening (`set -uo pipefail`)
3. Orphan agents not in `registry.json`
4. Required header fields (`agent_id`, `role`, `version`, `tags`, `last_updated`)
5. Certification tool availability

### `euxis-kaizen` — Continuous Self-Improvement

Runs a 4-gate Kaizen cycle: certification, architectural self-reflection, documentation sync, and strategic upgrade proposal.

```bash
euxis-kaizen
```

Gate sequence:
1. Certification suite (all toll gates)
2. Architect meta-audit (weak language, inconsistencies, registry sync)
3. Tech-writer documentation verification
4. Orchestrator upgrade plan proposal

### `euxis-daemon` — Safe Periodic Kaizen Loop

Runs `euxis-kaizen` on a configurable interval with fail-safe halting. If any cycle fails certification, the daemon stops immediately.

```bash
euxis-daemon           # Default: every 30 minutes
euxis-daemon 3600      # Custom: every 60 minutes (seconds)
```

Run in a dedicated terminal tab or tmux session. Review logs at:
```bash
cat $(ls -t ~/.euxis/projects/euxis-internal/daemon-logs/*.log | head -n 1)
```

## License

Copyright (c) 2026 Sebastien Rousseau. All rights reserved.
