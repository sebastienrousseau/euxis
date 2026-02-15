# Euxis Quick Start

**Get your AI agent fleet running in 5 minutes.**

---

## What You'll Accomplish

By the end of this guide, you'll have:

- **42 AI agents** ready to handle engineering tasks
- **Your first completed task** with verified output
- **Working knowledge** of the three main usage patterns

---

## Before You Begin

### Prerequisites

| Requirement | Minimum Version | Check Command |
|-------------|-----------------|---------------|
| Bash | 4.0+ | `bash --version` |
| Python | 3.8+ | `python3 --version` |
| AI Provider | Any one | See [Providers](#ai-providers) |

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
- [Gemini](https://github.com/google-gemini/gemini-cli) — Research with 2M token context
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

Clone the repository and run setup:

```bash
git clone https://github.com/sebastienrousseau/euxis.git ~/.euxis
~/.euxis/setup.sh
```

**Expected output:**
```
Installing Euxis Fleet...
Linking tools to ~/bin...
  ✓ euxis
  ✓ euxis-health
  ✓ euxis-certify
  [... 35+ tools linked ...]
Euxis is ready.
```

---

## Step 2: Add to Your PATH

Make Euxis commands available globally.

**For Zsh (macOS default):**
```bash
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.zshrc && source ~/.zshrc
```

**For Bash:**
```bash
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc && source ~/.bashrc
```

<details>
<summary><strong>Other Shells</strong></summary>

**Fish:**
```bash
echo 'set -gx PATH $HOME/bin $PATH' >> ~/.config/fish/config.fish
```

**Generic (add to your shell's config):**
```bash
export PATH="$HOME/bin:$PATH"
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

**What each check validates:**

| Check | What It Verifies |
|-------|------------------|
| Agent Naming | All 42 agents follow naming conventions |
| Script Hardening | Shell scripts have proper error handling |
| Orphaned Executables | No broken symlinks in ~/bin |
| Protocol Headers | Agent prompts include required headers |
| Documentation Drift | Docs match actual capabilities |
| Certification Status | System passed quality gates |
| Provider Dependencies | At least one AI provider available |
| Codex Integration | Prompt templates are valid |

✅ **Checkpoint:** All 8 checks pass. Your fleet is ready.

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
The fleet has 42 agents ready to assist with engineering tasks.
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

OBSERVATION 1: Found: bin/, config/, data/, docs/, prompts/, tests/, tui/

THOUGHT 2: I can now describe each component's role in the system.

FINAL ANSWER:
# Euxis System Architecture

## Core Components

### 1. Agent Execution (bin/)
- 38 executable tools for agent deployment
- Shell libraries in bin/lib/ for shared functionality
- Entry point: `euxis` command

### 2. Configuration (config/)
- Quality patterns for code enforcement
- Playbook definitions for workflows
- Provider routing rules
- Gateway settings (optional): `~/.euxis/config/gateway.json`

Minimal gateway config example:

```json
{
  "gateway": {
    "bind": "127.0.0.1",
    "port": 18789,
    "health_enabled": true,
    "health_path": "/health",
    "auth": {
      "mode": "token",
      "token": {
        "value": "replace-with-secure-random-value"
      }
    }
  }
}
```

Token guidance:

```bash
openssl rand -hex 32
```

Store tokens in environment variables or a secrets manager. Avoid committing them to git.

See also: `docs/reference/gateway-auth.md` and `docs/reference/gateway-config.md`.

### 3. Knowledge Storage (data/)
- Cortex: Semantic memory with vector search
- Project-specific agent outputs
- Performance metrics

### 4. Agent Intelligence (prompts/)
- 41 agent personality definitions
- 9 core agents + 32 specialists
- ReAct reasoning protocols

### 5. Terminal Interface (tui/)
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

### Launch the Terminal Interface

```bash
euxis-tui
```

Features: Fleet dashboard, command palette (`Ctrl+K`), streaming execution, performance metrics.

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
| [Fleet Guide](../guides/fleet-guide.md) | All 42 agents in detail |
| [User Guide](../guides/user-guide.md) | Complete CLI reference |
| [API Reference](../reference/api-reference.md) | Build custom integrations |

---

## Troubleshooting

<details>
<summary><strong>Command not found: euxis</strong></summary>

**Problem:** Shell can't find Euxis commands.

**Diagnosis:**
```bash
echo $PATH | grep "$HOME/bin"
ls ~/bin/euxis
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

*Euxis v0.0.7 · Build something that matters.*
