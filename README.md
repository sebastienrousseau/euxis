# Euxis

**42 AI specialists. Deploy in seconds.**
Version 0.0.7

[![Version][version-badge]][version-url]
[![License][license-badge]][license-url]
[![Platform][platform-badge]][platform-url]
[![Agents][agents-badge]][agents-url]

---

## What Euxis Does

Euxis coordinates 42 specialist AI agents to handle engineering tasks. Each agent masters one domain. You describe what you need. Euxis routes work to the right specialists and delivers verified results.

**Ship features faster.** Break complex goals into tasks. Delegate to specialists. Get working solutions.

```bash
euxis architect "Design a caching layer for our API"
```

**Catch bugs before production.** Security agents audit code. Testing agents verify logic. Quality gates block broken builds.

```bash
euxis-squad deploy quality "Audit the authentication module"
```

**Learn from every interaction.** The Cortex stores decisions, fixes, and lessons. Agents improve over time.

```bash
euxis-cortex remember "Fixed race condition with mutex lock" "debugger" --type procedural
```

---

## Quick Start

Get your first agent running in 2 minutes.

### 1. Install

```bash
git clone https://github.com/sebastienrousseau/euxis.git ~/.euxis
~/.euxis/setup.sh
```

### 2. Add to PATH

```bash
# For Zsh (macOS default)
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.zshrc && source ~/.zshrc

# For Bash
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc && source ~/.bashrc
```

### 3. Configure a Provider

Set one API key or run local with Ollama. This prevents first‑run auth errors.

```bash
# Claude
export ANTHROPIC_API_KEY="sk-ant-..."

# Gemini
export GOOGLE_API_KEY="AIza..."

# OpenAI
export OPENAI_API_KEY="sk-..."

# Optional default
export EUXIS_PROVIDER="claude"
```

Prefer local? No key needed:

```bash
ollama pull llama2
```

### 4. Verify Installation

```bash
euxis-health
```

**Expected output:**
```
✅ Agent Naming Convention
✅ Script Hardening
✅ Orphaned Executables
✅ Protocol Headers
✅ Documentation Drift
✅ Certification Status
✅ Provider Dependencies
✅ Codex Integration

Fleet Status: HEALTHY (8/8 checks passed)
```

### 5. Run Your First Agent

```bash
euxis architect "What makes a good REST API?"
```

**Expected output:**
```
[euxis] Agent: architect
[euxis] Provider: claude
[euxis] ⠋ architect (claude)...

THOUGHT 1: I need to outline the key principles that make REST APIs well-designed...

FINAL ANSWER:
A well-designed REST API follows these principles:

1. **Resource-Oriented Design**: URLs represent resources, not actions
   - Good: GET /users/123
   - Avoid: GET /getUser?id=123

2. **Proper HTTP Methods**: Use verbs correctly
   - GET for retrieval, POST for creation, PUT for updates, DELETE for removal

3. **Consistent Response Format**: JSON with predictable structure
   - Always include status, data, and error fields

[... continues with detailed recommendations ...]
```

---

## How to Use Euxis

Three patterns cover most engineering tasks.

### Pattern 1: Single Agent

One agent, one task, one answer. Use for focused questions.

```bash
euxis <agent> "<task>" [provider]
```

**Examples:**

```bash
# Debug a problem
euxis debugger "Why does login return null when password contains special characters?"

# Research options
euxis researcher "Compare PostgreSQL vs MySQL for a write-heavy workload"

# Write documentation
euxis writer "Create API documentation for the /users endpoint"

# Override provider
euxis architect "Design the API" gemini
```

### Pattern 2: Combo Chain

Agents execute in sequence. Each builds on previous output.

```bash
euxis-combo run <combo-name> "<task>"
```

**Available Combos:**

| Combo | Chain | Best For |
|-------|-------|----------|
| `envision` | planner → architect → evangelist → reviewer | Product design, feature specs |
| `protect` | pentester → auditor → inspector → reviewer | Security reviews |
| `refine` | designer → animator → interactor → reviewer | UI/UX design |

**Example:**

```bash
euxis-combo run envision "Design a user onboarding flow"
```

**What happens:**
1. **Planner** scopes requirements
2. **Architect** designs the structure
3. **Evangelist** refines the narrative
4. **Reviewer** validates the output

### Pattern 3: Squad Deploy

Multiple specialists work simultaneously.

```bash
euxis-squad deploy <squad> "<task>"
```

**Available Squads:**

| Squad | Members | Best For |
|-------|---------|----------|
| `quality` | reviewer, inspector, pentester, auditor + 4 more | Full audits |
| `build` | debugger, maintainer, automaton, tester + 2 more | Implementation |
| `vision` | orchestrator, architect, planner, researcher + 2 more | Strategy |

**Example:**

```bash
euxis-squad deploy quality "Complete security audit of the payment module"
```

**What happens:**
- All squad members run in parallel
- Lead agent (reviewer) coordinates output
- Results synthesized into comprehensive report

---

## Common Tasks

<details>
<summary><strong>Fix a Bug</strong></summary>

**Quick diagnosis:**
```bash
euxis debugger "Trace why login fails for users with special characters in passwords"
```

**Deep investigation:**
```bash
euxis-combo run protect "Investigate the session timeout issue"
```

**What you get:**
- Root cause analysis
- Fix recommendations
- Test suggestions

**Related:** [Debugging Workflow](docs/guides/workflows/debugging-workflow.md)

</details>

<details>
<summary><strong>Build a Feature</strong></summary>

**Design first:**
```bash
euxis architect "Design a notification system supporting email, SMS, and push"
```

**Then implement:**
```bash
euxis-combo run envision "Build user preferences for notification channels"
```

**What you get:**
- Architecture diagram
- Implementation plan
- API contracts

**Related:** [Feature Development Workflow](docs/guides/workflows/feature-development-workflow.md)

</details>

<details>
<summary><strong>Security Audit</strong></summary>

**Quick scan:**
```bash
euxis pentester "Check for SQL injection in the search endpoint"
```

**Full audit:**
```bash
euxis-squad deploy quality "Complete security audit of the payment module"
```

**What you get:**
- Vulnerability report
- Risk assessment
- Remediation steps

**Related:** [Security Audit Workflow](docs/guides/workflows/security-audit-workflow.md)

</details>

<details>
<summary><strong>Prepare a Release</strong></summary>

**Validate everything:**
```bash
euxis-playbook run verify-everything "Prepare v2.0 release" --dry-run
```

**Execute release:**
```bash
euxis-playbook run zero-to-one "Launch v2.0"
```

**What you get:**
- Pre-release checklist
- Quality gates passed
- Release notes

**Related:** [Fleet Guide: Playbooks](docs/guides/fleet-guide.md#playbooks)

</details>

---

## How It Works

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  Your Task  │ ──▶ │ Orchestrator│ ──▶ │ Specialists │ ──▶ │   Output    │
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
                          │                   │                   │
                    Breaks down          Execute in         Quality gates
                     the work            parallel           validate results
```

### Provider Selection

Euxis routes each agent to the optimal AI provider:

| Agent Type | Provider | Reason |
|------------|----------|--------|
| Strategic (orchestrator, architect) | `claude` | Best reasoning |
| Research (researcher) | `gemini` | Large context (2M tokens) |
| Coding (debugger, tester) | `goose` | Tool use |
| Utility (butler, writer) | `ollama` | Fast, local |

Provider appears in output:
```
[euxis] Provider: claude
```

Override anytime:
```bash
euxis architect "Design the API" gemini
```

### Memory System

The Cortex is persistent, tri‑typed memory backed by a vector + graph hybrid.
It stores to disk at `~/.euxis/data/cortex/db` and scales with your history.
Use it to lock in wins, avoid repeat failures, and share team patterns.
Learn more in [Memory](docs/essentials/core-concepts/memory.md).

| Type | What It Stores | Example |
|------|----------------|---------|
| `episodic` | Events | "Fixed bug #42 with null check" |
| `semantic` | Facts | "Auth module uses JWT with RS256" |
| `procedural` | Workflows | "Deploy: test → build → push" |

**Store knowledge:**
```bash
euxis-cortex remember "API uses OAuth 2.0 with PKCE" "architect" --type semantic
```

**Recall knowledge:**
```bash
euxis-cortex recall "authentication patterns" --type procedural
```

### Quality Assurance

Every output passes three verification layers:

1. **Internal consistency** — Claims supported by evidence
2. **Cross-reference** — Validated against stored knowledge
3. **Reviewer checkpoint** — Multi-agent work validated before delivery

---

## Agent Fleet

42 specialists organized by authority.

### Core Agents (9)

Define direction. May block progress when necessary.

| Agent | Function |
|-------|----------|
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

Execute domain work. [See full list →](docs/guides/fleet-guide.md#default-21--auto-available-task-triggered)

`accountant` · `animator` · `automaton` · `debugger` · `designer` · `gatekeeper` · `inspector` · `interactor` · `investigator` · `maintainer` · `optimizer` · `pentester` · `polyglot` · `repairer` · `responder` · `sentinel` · `tactician` · `telemetrist` · `tester` · `watchdog` · `writer`

### On-Demand Agents (7)

Growth and communication. [See full list →](docs/guides/fleet-guide.md#on-demand-7--explicit-invocation-only)

`ambassador` · `butler` · `evangelist` · `localizer` · `marketer` · `researcher` · `strategist`

### Specialist Agents (4)

Deep domain expertise. [See full list →](docs/guides/fleet-guide.md#specialist-4--domain-specific-expertise)

`cryptographer` · `ledger` · `conduit` · `custodian`

---

## Terminal Interface (ETX)

Modern TUI built on Python Textual.

```bash
euxis-tui
```

**Preview (text capture):**
```
Euxis ETX • Fleet Dashboard
────────────────────────────────────────────
✓ orchestrator   ✓ architect    ✓ reviewer
✓ debugger       ✓ tester       ✓ pentester
✓ inspector      ✓ auditor      ✓ sentinel
────────────────────────────────────────────
Focus: quality • Active: 8 • Cortex: 214 memories
```

**Features:**
- Fleet dashboard with all 42 agents
- Command palette (`Ctrl+K`)
- Streaming agent execution
- Squad monitoring
- Playbook browser
- Performance sparklines
- Cortex memory browser
- Dark/light/contrast themes

**Keyboard shortcuts:**
| Key | Action |
|-----|--------|
| `Ctrl+K` | Command palette |
| `Ctrl+T` | Toggle theme |
| `/` | Search |
| `?` | Help |
| `Ctrl+Q` | Quit |

---

## Validation

Run the same benchmarks we use in CI.

```bash
euxis-bench
```

Benchmarks and baselines live in `docs/benchmarks/`.

## Contributing

Open a pull request, file an issue, or propose a new agent.  
Start with `CONTRIBUTING.md`. Security issues belong in `SECURITY.md`.  
Issues and feature requests live at [GitHub Issues](https://github.com/sebastienrousseau/euxis/issues).

## Changelog and Migration

Track changes in `CHANGELOG.md`.  
Upgrade guidance lives in `docs/guides/migration-guide.md`.

## Learn More

| Guide | What You'll Learn |
|-------|-------------------|
| [Quick Start](docs/essentials/quick-start.md) | Step-by-step first deployment |
| [Core Concepts](docs/essentials/) | When to use agents vs squads vs playbooks |
| [Workflows](docs/guides/workflows/) | Problem-to-solution tutorials |
| [Fleet Guide](docs/guides/fleet-guide.md) | All 42 agents in detail |
| [User Guide](docs/guides/user-guide.md) | Complete CLI reference |
| [UI Guide](docs/guides/ui-guide.md) | ETX terminal interface |
| [API Reference](docs/reference/api-reference.md) | Build custom integrations |
| [Changelog](CHANGELOG.md) | What changed in each release |
| [Migration Guide](docs/guides/migration-guide.md) | Upgrade steps across versions |

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

**Install the quickest option:**
```bash
# Local, free, no API key needed
curl -fsSL https://ollama.ai/install.sh | sh
ollama pull llama2
```

</details>

---

## Troubleshooting

<details>
<summary><strong>Command not found: euxis</strong></summary>

**Problem:** Shell can't find Euxis commands.

**Solution:**
1. Verify PATH: `echo $PATH | grep "$HOME/bin"`
2. Restart terminal
3. Re-run: `source ~/.zshrc` (or `~/.bashrc`)

</details>

<details>
<summary><strong>Health check failures</strong></summary>

**Problem:** `euxis-health` reports issues.

**Solution:**
```bash
euxis-certify
```

This identifies and resolves common configuration problems.

</details>

<details>
<summary><strong>No AI provider available</strong></summary>

**Problem:** Agents can't execute.

**Solution:**
```bash
# Install Ollama (free, local)
curl -fsSL https://ollama.ai/install.sh | sh
ollama pull llama2
```

</details>

---

## License

Apache-2.0 © 2026 Sebastien Rousseau.

---

Designed by Sebastien Rousseau — https://sebastienrousseau.com
Engineered with Euxis — Enterprise Unified Execution Intelligence System — https://euxis.co

*Euxis v0.0.7 · Build something that matters.*

<!-- Reference Links -->

[version-badge]: https://img.shields.io/badge/version-0.0.7-blue?style=for-the-badge
[version-url]: https://github.com/sebastienrousseau/euxis/releases

[license-badge]: https://img.shields.io/badge/license-Apache%202.0-green?style=for-the-badge
[license-url]: https://github.com/sebastienrousseau/euxis/blob/main/LICENSE

[platform-badge]: https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20WSL-lightgrey?style=for-the-badge
[platform-url]: https://github.com/sebastienrousseau/euxis

[agents-badge]: https://img.shields.io/badge/agents-42-blueviolet?style=for-the-badge
[agents-url]: https://github.com/sebastienrousseau/euxis/blob/main/docs/guides/fleet-guide.md

[claude-url]: https://docs.anthropic.com/en/docs/claude-cli
[gemini-url]: https://github.com/google-gemini/gemini-cli
[ollama-url]: https://ollama.com/
[codex-url]: https://github.com/openai/codex
[qwen-url]: https://github.com/QwenLM/qwen-code
[crush-url]: https://github.com/charmbracelet/crush
[kiro-cli-url]: https://kiro.dev
[goose-url]: https://github.com/block/goose
