# Euxis Quick Start

**Get your AI agent fleet running in 5 minutes.**

> Status note (February 18, 2026): for current test/coverage/performance
> verification metrics and known stabilization areas, see
> `reports/initial-user-expectations-2026-02-18.md`.

---

## What You'll Accomplish

By the end of this guide, you'll have:

- **53 AI agents** ready to handle engineering tasks
- **Your first completed task** with verified output
- **Working knowledge** of the three main usage patterns

---

## Before You Begin

### Prerequisites

| Requirement | Minimum Version | Check Command |
|-------------|-----------------|---------------|
| Bash | 4.0+ | `bash --version` |
| Python | 3.10+ | `python3 --version` |
| AI Provider CLI | Any one | See [Providers](#ai-providers) |

### AI Providers

Euxis works with 8 providers. You need at least one installed:

| Provider | Best For | Quick Install |
|:---------|:---------|:--------------|
| [Ollama](https://ollama.com/) | Local, free, no API key | `curl -fsSL https://ollama.ai/install.sh \| sh` |
| [Claude](https://docs.anthropic.com/en/docs/claude-cli) | Strategy, architecture | `brew install claude-cli` |
| [Gemini](https://github.com/google-gemini/gemini-cli) | Research, large context | Follow installation guide |

<details>
<summary><strong>All 8 Providers</strong></summary>

**Cloud Providers:**
- [Claude](https://docs.anthropic.com/en/docs/claude-cli) — Strategic reasoning
- [Gemini](https://github.com/google-gemini/gemini-cli) — Research with large context
- [Kiro CLI](https://kiro.dev) — AI coding assistant
- [Codex CLI](https://github.com/openai/codex) — OpenAI models

**Local Providers:**
- [Ollama](https://ollama.com/) — Local inference, zero cost
- [Qwen](https://github.com/QwenLM/qwen-code) — 256K context coding
- [Goose](https://github.com/block/goose) — MCP-native agent
- [Crush](https://github.com/charmbracelet/crush) — Multi-model TUI

</details>

---

## Step 1: Install Euxis

Clone the repository and run the installer:

```bash
git clone https://github.com/sebastienrousseau/euxis.git ~/.euxis
cd ~/.euxis
./install.sh
```

**Expected output (abridged):**
```
Euxis v0.0.3 Installation
Adding EUXIS_HOME and PATH to ~/.zshrc
Installation complete!
```

---

## Step 2: Add to Your PATH

Make Euxis commands available globally.

**For Zsh (macOS default):**
```bash
source ~/.zshrc
```

**For Bash:**
```bash
source ~/.bashrc
```

<details>
<summary><strong>Other Shells</strong></summary>

**Fish:**
```bash
source ~/.config/fish/config.fish
```

**Generic (add to your shell's config):**
```bash
export EUXIS_HOME="$HOME/.euxis"
export PATH="$EUXIS_HOME/euxis-bin:$PATH"
```

</details>

---

## Step 3: Verify Installation

Run the fleet health check:

```bash
euxis-health
```

**Expected output:**
```
✅ Naming Consistency
✅ Script Hardening
✅ Orphan Detection
✅ Header Schema Validation
✅ Documentation Drift
✅ Certification Readiness
✅ Provider Connectivity
✅ Codex Integrity
✅ Bus Pipe Activity
✅ Cortex Connectivity
```

**What each check validates:**

| Check | What It Verifies |
|-------|------------------|
| Naming Consistency | Agent filenames match their `agent_id` |
| Script Hardening | Shell scripts use strict error handling |
| Orphan Detection | No prompt files missing from registry |
| Header Schema Validation | Required headers present in scripts and prompts |
| Documentation Drift | Canonical docs have been updated recently |
| Certification Readiness | Certification prerequisites are satisfied |
| Provider Connectivity | At least one provider CLI is available |
| Codex Integrity | Prompt templates are valid |
| Bus Pipe Activity | Internal bus pipe is writable |
| Cortex Connectivity | Cortex backend is reachable (if enabled) |

✅ **Checkpoint:** All checks pass. Your fleet is ready.

---

## Step 4: Meet Your First Agent

The command syntax is:

```bash
euxis <agent> "<task>" [provider]
```

Try the **Butler** agent for a friendly introduction:

```bash
euxis butler "Introduce yourself briefly"
```

**Expected output:**
```
[euxis] Agent: butler
[euxis] Provider: ollama
[euxis] ⠋ butler (ollama)...

Hello! I'm your Euxis Butler, optimized for clear communication.
I translate complex system outputs into natural, spoken English.
The fleet has 53 agents ready to assist with engineering tasks.
```

**Understanding the output:**

| Line | Meaning |
|------|---------|
| `Agent: butler` | Which specialist is working |
| `Provider: ollama` | Which AI is running the agent |
| Spinner `⠋` | Live status while agent works |

<details>
<summary><strong>Understanding Provider Selection</strong></summary>

Euxis picks the optimal AI provider for each agent:

| Agent Type | Provider | Why |
|------------|----------|-----|
| Strategic (orchestrator, architect) | `claude` | Best reasoning |
| Research (researcher) | `gemini` | Massive context |
| Coding (debugger, tester) | `goose` | Tool use |
| Utility (butler, writer) | `ollama` | Fast, local |

**Override anytime:**
```bash
euxis butler "Introduce yourself" claude  # Force Claude
```

</details>

---

## Step 5: Run a Real Task

Now deploy the **Architect** agent for actual analysis:

```bash
euxis architect "Explain the key components of the Euxis system"
```

**Expected output:**
```
[euxis] Agent: architect
[euxis] Provider: claude
[euxis] ⠋ architect (claude)...

THOUGHT 1: I need to examine the Euxis system structure to understand its architecture.

ACTION 1: List the main directories to see the overall organization.

OBSERVATION 1: Found: euxis-bin/, config/, euxis-policy/, data/runtime/memory/, metrics/src/metrics/, data/runtime/, docs/, agents/, tests/, ui/src/tui/, api/src/gateway/, adapters/src/adapters/

THOUGHT 2: I can now describe each component's role in the system.

FINAL ANSWER:
# Euxis System Architecture

## Core Components

### 1. Agent Execution (cli/)
- 38 executable tools for agent deployment
- Shell libraries in core/lib/ for shared functionality
- Entry point: `euxis` command

### 2. Configuration (config/ + euxis-policy/)
- Gateway policy defaults: `euxis-policy/gateway.json`
- Quality patterns for code enforcement
- Playbook definitions for workflows
- Provider routing rules

### 3. Knowledge Storage (data/runtime/memory/ + metrics/src/metrics/ + data/runtime/)
- Metrics live in `metrics/src/metrics/` (runtime data in `~/.euxis/metrics/`)
- Cortex: Semantic memory with vector search
- Project-specific agent outputs
- Performance metrics (metrics/src/metrics/)

### 4. Agent Intelligence (agents/)
- 41 agent personality definitions
- 9 core agents + 32 specialists
- ReAct reasoning protocols

### 5. Terminal Interface (ui/src/tui/)
- Python Textual-based TUI
- 12 screens for fleet management
- Command palette and theming

[... continues with detailed breakdown ...]
```

**What you're seeing:**

1. **THOUGHT**: Agent's reasoning process
2. **ACTION**: Steps being taken
3. **OBSERVATION**: Results of each action
4. **FINAL ANSWER**: Synthesized deliverable

This is the **ReAct pattern** — agents reason through problems step by step, grounding every claim in observations.

✅ **Checkpoint:** You deployed an agent that reasoned through a task systematically.

---

## What's Next

You've completed the quick start. Here's where to go from here:

### Try These Tasks

```bash
# Debug a problem
euxis debugger "Why would a login function return null for valid credentials?"

# Research options
euxis researcher "Compare the top 3 Python web frameworks for REST APIs"

# Design something
euxis architect "Design a caching strategy for a high-traffic API"

# Write documentation
euxis writer "Create getting started docs for our CLI tool"
```

### Coordinate Multiple Agents

```bash
# Chain agents for refined output (planner → architect → evangelist → reviewer)
euxis-combo run envision "Design a user notification system"

# Deploy a full squad for comprehensive work
euxis-squad deploy quality "Audit the authentication module"
```

### Launch the Desktop Interface

```bash
euxis-etx
```

Features: Fleet dashboard, command palette (`Ctrl+K`), streaming execution, performance metrics, 17 screens, 3 themes.

### Learn the Concepts

| Guide | What You'll Learn |
|-------|-------------------|
| [Choosing Coordination](core-concepts/choosing-coordination.md) | When to use agents vs combos vs squads vs playbooks |
| [Agent Selection](core-concepts/agent-selection.md) | How to pick the right agent for your task |
| [Memory System](core-concepts/memory.md) | How agents learn and remember |
| [Workflows](../guides/workflows/) | Problem-to-solution tutorials |

### Reference Documentation

| Guide | What You'll Learn |
|-------|-------------------|
| [Fleet Guide](../guides/fleet-guide.md) | All 53 agents in detail |
| [User Guide](../guides/user-guide.md) | Complete CLI reference |
| [API Reference](../reference/api-reference.md) | Build custom integrations |

---

## Troubleshooting

<details>
<summary><strong>Command not found: euxis</strong></summary>

**Problem:** Shell can't find Euxis commands.

**Diagnosis:**
```bash
echo $PATH | grep "$HOME/.euxis/euxis-bin"
ls ~/.euxis/euxis-bin/euxis
```

**Solution:**
1. Verify PATH was updated correctly
2. Restart your terminal completely
3. Re-run: `source ~/.zshrc` (or `~/.bashrc`)

</details>

<details>
<summary><strong>Health check failures</strong></summary>

**Problem:** `euxis-health` reports failed checks.

**Solution:**
```bash
euxis-certify
```

This runs the full certification pipeline and fixes common issues automatically.

</details>

<details>
<summary><strong>No AI provider available</strong></summary>

**Problem:** Agents can't execute because no provider is configured.

**Solution:**
```bash
# Install Ollama (local, free)
curl -fsSL https://ollama.ai/install.sh | sh
ollama pull llama2

# Verify
ollama list
```

</details>

<details>
<summary><strong>Agent output is empty or truncated</strong></summary>

**Problem:** Agent returns incomplete response.

**Possible causes:**
- Provider rate limiting
- Network timeout
- Context too large

**Solutions:**
```bash
# Try a different provider
euxis architect "Your task" ollama

# Simplify the task
euxis architect "List the main components only"

# Check provider status
euxis-health
```

</details>

---

*Euxis v0.0.3 · Build something that matters.*
