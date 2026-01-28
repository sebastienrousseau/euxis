# Euxis

Enterprise multi-provider AI agent framework. 19 specialized agents with persistent audit logs, structured memory, and automated evaluation.

## Directory Structure

```
~/.euxis/
  prompts/              Agent definitions and shared protocol
    _protocol.txt       Mandatory protocol (appended at runtime to all agents)
    architect.txt       ...one file per agent...
  scripts/              Executable tools (symlinked to ~/bin/)
    euxis.sh            Core CLI — agent routing, prompt assembly, output capture
    euxis-ui            Mission Control TUI
    euxis-gym           Automated agent evaluation
    euxis-council       Multi-agent consensus protocol
    euxis-loop          Autonomous try-catch-retry
    euxis-lint          Static analysis for agent fleet
    euxis-test-infra    Infrastructure unit tests
    euxis-certify       Full certification pipeline
  tests/                Golden datasets for agent evaluation
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

### Growth & Community

| Agent | Role |
|-------|------|
| `growth-marketer` | SEO, AARRR funnel, CRO, GTM strategy |
| `social-manager` | Platform-native content, content calendars, community engagement |
| `devrel-advocate` | Developer relations, tutorials, demos, conferences |

## Installation

### Prerequisites

- Bash 4.0+
- One or more AI providers installed:
  - [Claude CLI](https://docs.anthropic.com/en/docs/claude-cli) (`claude`)
  - [Gemini CLI](https://github.com/google-gemini/gemini-cli) (`gemini`)
  - [shell-gpt](https://github.com/TheR1D/shell_gpt) (`sgpt`)

### Setup

```bash
# Clone the repository
git clone https://github.com/sebastienrousseau/euxis.git ~/.euxis

# Create ~/bin if it does not exist
mkdir -p ~/bin

# Symlink all tools
ln -sf ~/.euxis/scripts/euxis.sh     ~/bin/euxis
ln -sf ~/.euxis/scripts/euxis-ui     ~/bin/euxis-ui
ln -sf ~/.euxis/scripts/euxis-gym    ~/bin/euxis-gym
ln -sf ~/.euxis/scripts/euxis-council ~/bin/euxis-council
ln -sf ~/.euxis/scripts/euxis-loop   ~/bin/euxis-loop
ln -sf ~/.euxis/scripts/euxis-lint   ~/bin/euxis-lint
ln -sf ~/.euxis/scripts/euxis-test-infra ~/bin/euxis-test-infra
ln -sf ~/.euxis/scripts/euxis-certify ~/bin/euxis-certify

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
| `agent` | Agent name (matches filename in `prompts/`) | required |
| `task` | Task description or instruction | required |
| `provider` | AI provider: `claude`, `gemini`, `openai` | `claude` |

```bash
euxis architect "Review the authentication module"
euxis bug-fixer "Fix the null pointer in user.py" gemini
euxis delegate perf-optimizer "Profile the API layer"
```

### `euxis-ui` — Mission Control TUI

Interactive terminal interface for deploying agents, switching providers, and browsing audit logs.

```bash
euxis-ui
```

### `euxis-gym` — Agent Evaluation

Runs an agent against a golden test case and scores the output 0-100 using a judge model.

```bash
euxis-gym <agent> <test_case_path>
```

```bash
euxis-gym bug-fixer ~/.euxis/tests/bug-fixer-01.txt
```

Test case format:
```
INPUT: <task for the agent>
EXPECTED_OUTPUT: <criteria the output must satisfy>
```

### `euxis-council` — Multi-Agent Consensus

Convenes architect, edge-hunter, and perf-optimizer to debate a topic. The orchestrator synthesizes a final binding decision.

```bash
euxis-council "<topic>"
```

```bash
euxis-council "Should we migrate from Postgres to MongoDB for logging?"
```

### `euxis-loop` — Autonomous Self-Correction

Runs an agent, executes a verification command, and retries on failure — feeding errors back as context.

```bash
euxis-loop <agent> <task> <verification_command> [max_retries]
```

```bash
euxis-loop bug-fixer "Fix parser.py" "pytest tests/test_parser.py" 3
```

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

## License

Copyright (c) 2026 Sebastien Rousseau. All rights reserved.
