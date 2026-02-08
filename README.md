# Euxis

**41 AI specialists. Deploy in seconds.**

[![Version][version-badge]][version-url]
[![License][license-badge]][license-url]
[![Platform][platform-badge]][platform-url]
[![Agents][agents-badge]][agents-url]

---

## What Euxis Delivers

41 specialist AI agents coordinate automatically on engineering tasks. Each agent masters one domain.

**Ship features faster.** Break down complex goals. Delegate to specialists. Deploy solutions.

**Catch bugs before production.** Security agents audit code. Testing agents verify logic. Quality gates block broken builds.

**Learn from every interaction.** The Cortex stores decisions, fixes, and lessons. Agents improve over time.

---

## Deploy Your First Agent

Complete deployment in 2 minutes.

### Install

```bash
git clone https://github.com/sebastienrousseau/euxis.git ~/.euxis
~/.euxis/setup.sh
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.zshrc && source ~/.zshrc
```

### Verify

```bash
euxis-health
```

Confirm 8 green checkmarks appear. Run `euxis-certify` to resolve issues.

### Deploy

```bash
euxis architect "What are the key components of a well-designed REST API?"
```

Output displays:
- **Provider selection:** Euxis selects optimal AI (`claude` for strategic work)
- **Reasoning steps:** Agent analyzes problems systematically
- **Structured output:** Clear, actionable recommendations

✅ **Success:** Agent deployment complete. Architecture analysis delivered.

---

## Core Capabilities

Three patterns handle most engineering tasks.

### Single Agent

One agent, one task, one answer.

```bash
euxis <agent> "<task>" [provider]

# Examples:
euxis debugger "Why does this function return null when the input is empty?"
euxis researcher "Compare the top 3 Python testing frameworks"
euxis writer "Document this API endpoint for developers"
euxis architect "Design the API" gemini  # Override provider
```

**Use for:** Quick questions, focused analysis, single-domain work.

### Combo Chain

Agents execute in sequence. Each agent builds on previous output.

```bash
euxis-combo run steve-jobs "Design a user onboarding flow"
```

Steve Jobs combo: `planner → architect → evangelist → reviewer`

**Use for:** Tasks requiring multiple perspectives in order.

### Squad Deploy

Specialists work simultaneously.

```bash
euxis-squad deploy quality "Audit the authentication module"
```

Quality squad: `reviewer`, `inspector`, `pentester`, `auditor`, `optimizer`, `watchdog`, `polyglot`, `arbiter`

**Use for:** Full analysis, security audits, pre-release checks.

---

## How It Works

```
Your Goal → Orchestrator → Specialists → Verified Output
                ↓              ↓              ↓
           Breaks down    Execute in     Quality gates
           the work       parallel       validate results
```

**Smart provider selection.** Euxis routes each agent to optimal AI:
- Strategic agents (orchestrator, architect) → `claude` for reasoning
- Research agents → `gemini` for large context
- Coding agents → `goose` for tool use
- Utility agents → `ollama` for speed

Provider selection appears in output. Override with third argument:
```bash
euxis architect "Design the API" gemini  # Override default
```

**Persistent memory.** Cortex stores three knowledge types:
- **Episodic:** Events ("Fixed bug #42 with null check")
- **Semantic:** Facts ("Auth module uses JWT with RS256")
- **Procedural:** Workflows ("Deploy sequence: test → build → push")

**Quality assurance.** Every output passes verification:
1. Internal consistency check
2. Cross-reference against stored knowledge
3. Reviewer validation for multi-agent work

---

## Common Tasks

<details>
<summary><strong>Fix a bug</strong></summary>

```bash
# Quick diagnosis
euxis debugger "Trace why login fails for users with special characters in passwords"

# Deep investigation with forensics
euxis-combo run fort-knox "Investigate the authentication bypass vulnerability"
```

**Related:** [Debugging Workflow](docs/guides/workflows/debugging-workflow.md)

</details>

<details>
<summary><strong>Build a new feature</strong></summary>

```bash
# Architecture design
euxis architect "Design a notification system that supports email, SMS, and push"

# Feature development
euxis-combo run steve-jobs "Build user preferences for notification channels"
```

**Related:** [Feature Development Workflow](docs/guides/workflows/feature-development-workflow.md)

</details>

<details>
<summary><strong>Audit for security</strong></summary>

```bash
# Quick scan
euxis pentester "Check for SQL injection in the search endpoint"

# Full audit
euxis-squad deploy quality "Complete security audit of the payment module"
```

**Related:** [Security Audit Workflow](docs/guides/workflows/security-audit-workflow.md)

</details>

<details>
<summary><strong>Ship a release</strong></summary>

```bash
# Pre-release validation
euxis-playbook run verify-everything "Prepare v2.0 release" --dry-run

# Release execution
euxis-playbook run zero-to-one "Launch v2.0 with new auth system"
```

**Related:** [Fleet Guide: Playbooks](docs/fleet-guide.md#playbooks)

</details>

---

## Agent Fleet

41 specialists organized by authority level.

### Core Agents (9)

Define direction. Block progress when necessary.

| Agent | Function |
|-------|----------|
| `orchestrator` | Break down goals, coordinate specialists, synthesize results |
| `architect` | Design systems, define patterns, review structure |
| `planner` | Scope work, prioritize tasks, manage intent |
| `reviewer` | Quality gate for all output |
| `critic` | Challenge assumptions, surface risks |
| `auditor` | Legal, privacy, and compliance authority |
| `arbiter` | Resolve conflicts between agents |
| `librarian` | Knowledge governance and documentation |
| `historian` | Long-term memory and patterns |

### Default Agents (21)

Execute domain work. [See full list →](docs/guides/fleet-guide.md#default-21--auto-available-task-triggered)

`accountant` · `animator` · `automaton` · `debugger` · `designer` · `gatekeeper` · `inspector` · `interactor` · `investigator` · `maintainer` · `optimizer` · `pentester` · `polyglot` · `repairer` · `responder` · `sentinel` · `tactician` · `telemetrist` · `tester` · `watchdog` · `writer`

### On-Demand Agents (7)

Growth and communication tasks. [See full list →](docs/guides/fleet-guide.md#on-demand-7--explicit-invocation-only)

`ambassador` · `butler` · `evangelist` · `localizer` · `marketer` · `researcher` · `strategist`

### Specialist Agents (4)

Deep domain expertise. [See full list →](docs/guides/fleet-guide.md#specialist-4--domain-specific-expertise)

`cryptographer` · `ledger` · `conduit` · `custodian`

---

## Learn More

| Guide | What You'll Learn |
|-------|-------------------|
| [Quick Start](docs/essentials/quick-start.md) | Step-by-step first deployment |
| [Concepts](docs/essentials/) | When to use agents vs squads vs playbooks |
| [Workflows](docs/guides/workflows/) | Problem-to-solution tutorials |
| [Fleet Guide](docs/guides/fleet-guide.md) | All 41 agents in detail |
| [User Guide](docs/guides/user-guide.md) | Complete CLI reference |
| [API Reference](docs/reference/api-reference.md) | Build custom integrations |

---

## Requirements

- **Bash 4.0+** and **Python 3.8+**
- **At least one AI provider** (Claude recommended)

<details>
<summary><strong>Supported Providers</strong></summary>

| Provider | CLI | Function |
|:---------|:----|:---------|
| [Claude][claude-url] | `claude` | Reasoning, architecture, strategy |
| [Gemini][gemini-url] | `gemini` | Research with large context |
| [Ollama][ollama-url] | `ollama` | Local inference, zero cost |
| [Codex CLI][codex-url] | `codex` | OpenAI models |
| [Qwen Code][qwen-url] | `qwen` | Open-source coding (256K context) |
| [Crush][crush-url] | `crush` | Multi-model TUI |
| [Kiro CLI][kiro-cli-url] | `kiro-cli` | AI coding assistant |
| [Goose][goose-url] | `goose` | MCP-native agent |

**Provider selection:**

Euxis selects optimal provider for each agent. Provider appears in output:

```
[euxis] Provider: claude
```

Override with third argument:

```bash
euxis architect "Design the API" gemini    # Use Gemini
euxis debugger "Fix the bug" ollama        # Use local Ollama
```

**Learn more:** [Provider Routing](docs/essentials/core-concepts/provider-routing.md)

</details>

---

## License

Copyright (c) 2026 Sebastien Rousseau. All rights reserved.

---

🎨 Designed by **[Sebastien Rousseau](https://sebastienrousseau.com/)**
🚀 Engineered with **[Euxis](https://euxis.co/)** — Enterprise Unified eXecution Intelligence System

*Euxis Version 0.0.7 · Build something that matters.*

<!-- Reference Links -->

[version-badge]: https://img.shields.io/badge/version-0.0.7-blue?style=for-the-badge
[version-url]: https://github.com/sebastienrousseau/euxis/releases

[license-badge]: https://img.shields.io/badge/license-proprietary-green?style=for-the-badge
[license-url]: https://github.com/sebastienrousseau/euxis/blob/main/LICENSE

[platform-badge]: https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20WSL-lightgrey?style=for-the-badge
[platform-url]: https://github.com/sebastienrousseau/euxis

[agents-badge]: https://img.shields.io/badge/agents-41-blueviolet?style=for-the-badge
[agents-url]: https://github.com/sebastienrousseau/euxis/blob/main/docs/fleet-guide.md

[claude-url]: https://docs.anthropic.com/en/docs/claude-cli
[gemini-url]: https://github.com/google-gemini/gemini-cli
[ollama-url]: https://ollama.com/
[codex-url]: https://github.com/openai/codex
[qwen-url]: https://github.com/QwenLM/qwen-code
[crush-url]: https://github.com/charmbracelet/crush
[kiro-cli-url]: https://kiro.dev
[goose-url]: https://github.com/block/goose
