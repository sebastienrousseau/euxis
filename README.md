# Euxis

**Your AI engineering team, ready in seconds.**

[![Version][version-badge]][version-url]
[![License][license-badge]][license-url]
[![Platform][platform-badge]][platform-url]
[![Agents][agents-badge]][agents-url]

---

## What You Can Build

Euxis gives you 41 specialist AI agents that work together on real engineering tasks. Each agent has deep expertise in one domain and they coordinate automatically.

**Ship features faster.** The orchestrator breaks down complex goals and delegates to specialists. You describe what you want, Euxis figures out how to build it.

**Catch bugs before users do.** Security agents audit your code. Testing agents verify your logic. Quality gates block broken builds.

**Never repeat yourself.** The Cortex remembers every decision, every fix, every lesson learned. Your agents get smarter over time.

---

## Get Your First Win

This takes about 2 minutes. By the end, you'll have deployed your first agent.

### Step 1: Install

```bash
git clone https://github.com/sebastienrousseau/euxis.git ~/.euxis
~/.euxis/setup.sh
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.zshrc && source ~/.zshrc
```

### Step 2: Verify

```bash
euxis-health
```

You should see 8 green checkmarks. If not, run `euxis-certify` to fix issues.

### Step 3: Deploy Your First Agent

```bash
euxis architect "What are the key components of a well-designed REST API?"
```

Watch the output. You'll see:
- **Provider selection:** Euxis picks the best AI (`claude` for strategic work)
- **Reasoning steps:** The agent thinks through the problem step by step
- **Structured output:** Clear, actionable recommendations

✅ **Success checkpoint:** You just deployed an agent that reasoned about software architecture.

---

## Core Capabilities

Start here. These three patterns handle most engineering tasks.

### Ask a Single Agent

The simplest pattern. One agent, one task, one answer.

```bash
euxis debugger "Why does this function return null when the input is empty?"
euxis researcher "Compare the top 3 Python testing frameworks"
euxis writer "Document this API endpoint for developers"
```

**When to use:** Quick questions, focused analysis, single-domain work.

### Run a Combo

Chain agents in sequence. Each agent builds on the previous output.

```bash
euxis-combo run steve-jobs "Design a user onboarding flow"
```

The "Steve Jobs" combo runs: `planner → architect → evangelist → reviewer`

**When to use:** Tasks that need multiple perspectives in order.

### Deploy a Squad

Parallel execution. Multiple specialists work simultaneously.

```bash
euxis-squad deploy quality "Audit the authentication module"
```

The Quality squad includes: `reviewer`, `inspector`, `pentester`, `auditor`, `optimizer`, `watchdog`, `polyglot`, `arbiter`

**When to use:** Full analysis, security audits, pre-release checks.

---

## How It Works

```
Your Goal → Orchestrator → Specialists → Verified Output
                ↓              ↓              ↓
           Breaks down    Execute in     Quality gates
           the work       parallel       validate results
```

**Smart provider selection.** Euxis routes each agent to the best AI:
- Strategic agents (orchestrator, architect) → `claude` for reasoning
- Research agents → `gemini` for large context
- Coding agents → `goose` for tool use
- Utility agents → `ollama` for speed

**Persistent memory.** The Cortex stores three types of knowledge:
- **Episodic:** What happened ("Fixed bug #42 with null check")
- **Semantic:** What's true ("Auth module uses JWT with RS256")
- **Procedural:** How to do things ("Deploy sequence: test → build → push")

**Quality assurance.** Every output passes through verification:
1. Internal consistency check
2. Cross-reference against stored knowledge
3. Reviewer validation for multi-agent work

---

## Choose Your Path

<details>
<summary><strong>I want to fix a bug</strong></summary>

```bash
# Quick diagnosis
euxis debugger "Trace why login fails for users with special characters in passwords"

# Deep investigation with forensics
euxis-combo run fort-knox "Investigate the authentication bypass vulnerability"
```

**Related:** [Debugging Workflow](docs/workflows/debugging-workflow.md)

</details>

<details>
<summary><strong>I want to build a new feature</strong></summary>

```bash
# Start with architecture
euxis architect "Design a notification system that supports email, SMS, and push"

# Full feature development
euxis-combo run steve-jobs "Build user preferences for notification channels"
```

**Related:** [Feature Development Workflow](docs/workflows/feature-development-workflow.md)

</details>

<details>
<summary><strong>I want to audit for security</strong></summary>

```bash
# Quick scan
euxis pentester "Check for SQL injection in the search endpoint"

# Full audit
euxis-squad deploy quality "Complete security audit of the payment module"
```

**Related:** [Security Audit Workflow](docs/workflows/security-audit-workflow.md)

</details>

<details>
<summary><strong>I want to ship a release</strong></summary>

```bash
# Pre-release checklist
euxis-playbook run verify-everything "Prepare v2.0 release" --dry-run

# Execute release pipeline
euxis-playbook run zero-to-one "Launch v2.0 with new auth system"
```

**Related:** [Fleet Guide: Playbooks](docs/fleet-guide.md#playbooks)

</details>

---

## The Agent Fleet

41 specialists organized by authority level.

### Core Agents (9)

These agents define direction. They can block progress when something is wrong.

| Agent | What It Does |
|-------|--------------|
| `orchestrator` | Breaks down goals, coordinates specialists, synthesizes results |
| `architect` | Designs systems, defines patterns, reviews structure |
| `planner` | Scopes work, prioritizes tasks, manages intent |
| `reviewer` | Quality gate for all output |
| `critic` | Challenges assumptions, surfaces risks |
| `auditor` | Legal, privacy, and compliance authority |
| `arbiter` | Resolves conflicts between agents |
| `librarian` | Knowledge governance and documentation |
| `historian` | Long-term memory and patterns |

### Default Agents (21)

Execute domain work when called. [See full list →](docs/fleet-guide.md#default-21--auto-available-task-triggered)

`accountant` · `animator` · `automaton` · `debugger` · `designer` · `gatekeeper` · `inspector` · `interactor` · `investigator` · `maintainer` · `optimizer` · `pentester` · `polyglot` · `repairer` · `responder` · `sentinel` · `tactician` · `telemetrist` · `tester` · `watchdog` · `writer`

### On-Demand Agents (7)

For growth tasks. [See full list →](docs/fleet-guide.md#on-demand-7--explicit-invocation-only)

`ambassador` · `butler` · `evangelist` · `localizer` · `marketer` · `researcher` · `strategist`

### Specialist Agents (4)

Deep domain expertise. [See full list →](docs/fleet-guide.md#specialist-4--domain-specific-expertise)

`cryptographer` · `ledger` · `conduit` · `custodian`

---

## Learn More

| Guide | What You'll Learn |
|-------|-------------------|
| [Quick Start](docs/quick-start.md) | Step-by-step first deployment |
| [Concepts](docs/concepts/) | When to use agents vs squads vs playbooks |
| [Workflows](docs/workflows/) | Problem-to-solution tutorials |
| [Fleet Guide](docs/fleet-guide.md) | All 41 agents in detail |
| [User Guide](docs/user-guide.md) | Complete CLI reference |
| [API Reference](docs/api-reference.md) | Build custom integrations |

---

## Requirements

- **Bash 4.0+** and **Python 3.8+**
- **At least one AI provider** (Claude recommended)

<details>
<summary><strong>Supported Providers</strong></summary>

| Provider | CLI | Best For |
|:---------|:----|:---------|
| [Claude][claude-url] | `claude` | Reasoning, architecture, strategy |
| [Gemini][gemini-url] | `gemini` | Research with large context |
| [Ollama][ollama-url] | `ollama` | Local inference, zero cost |
| [Codex CLI][codex-url] | `codex` | OpenAI models |
| [Qwen Code][qwen-url] | `qwen` | Open-source coding (256K context) |
| [Crush][crush-url] | `crush` | Multi-model TUI |
| [Kiro CLI][kiro-cli-url] | `kiro-cli` | AI coding assistant |
| [Goose][goose-url] | `goose` | MCP-native agent |

</details>

---

## License

Copyright (c) 2026 Sebastien Rousseau. All rights reserved.

---

*Euxis v0.0.7 · Build something that matters.*

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
