# Euxis Quick Start

**Get your AI agent fleet running in 5 minutes.**

## What You'll Build

By the end of this guide, you'll have:
- **41 AI agents** ready to run engineering tasks
- **Your first successful task** completed with verification
- **Working knowledge** of how to deploy agents for common workflows

Expected time: **5 minutes**

---

## Before You Begin

### Prerequisites

- **Bash 4.0+** and **Python 3.8+**
- **At least one AI provider** from our [supported list](#ai-providers)

<details>
<summary><strong>Verify Prerequisites</strong></summary>

Check your environment:

```bash
bash --version    # Should show 4.0 or higher
python3 --version # Should show 3.8 or higher
```

</details>

### AI Providers

Euxis works with **8 providers** right away. You need at least one:

| Provider | Best For | Install |
|:---------|:---------|:--------|
| [Claude](https://docs.anthropic.com/en/docs/claude-cli) | Strategy, architecture | `brew install claude-cli` |
| [Ollama](https://ollama.com/) | Local inference, zero cost | `curl -fsSL https://ollama.ai/install.sh \| sh` |
| [Gemini](https://github.com/google-gemini/gemini-cli) | Research, large context | Follow installation guide |

<details>
<summary><strong>See All 8 Providers</strong></summary>

**Cloud Providers:**
- [Claude](https://docs.anthropic.com/en/docs/claude-cli): Strategic reasoning
- [Gemini](https://github.com/google-gemini/gemini-cli): Research with massive context
- [Kiro CLI](https://kiro.dev): AI coding assistant
- [Codex CLI](https://github.com/openai/codex): OpenAI models

**Local Providers:**
- [Ollama](https://ollama.com/): Local inference
- [Qwen](https://github.com/QwenLM/qwen-code): 256K context coding
- [Goose](https://github.com/block/goose): MCP-native agent
- [Crush](https://github.com/charmbracelet/crush): Multi-model TUI

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
  - euxis
  - euxis-health
  - euxis-certify
  [... 30+ tools linked ...]
Euxis is ready.
```

---

## Step 2: Add to Your PATH

Make Euxis commands available globally:

```bash
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.profile && source ~/.profile
```

<details>
<summary><strong>Alternative Shell Configurations</strong></summary>

**For Zsh (macOS default):**
```bash
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.zshrc && source ~/.zshrc
```

**For Fish:**
```bash
echo 'set -gx PATH $HOME/bin $PATH' >> ~/.config/fish/config.fish
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

✅ **Success checkpoint:** All 8 checks pass. Your fleet is ready.

---

## Step 4: Meet Your First Agent

Start with the **Butler**: your friendly introduction agent.

**Command syntax:**
```bash
euxis <agent> "<task>" [provider]
```

**Try it:**
```bash
euxis butler "Introduce yourself briefly."
```

**What you'll see:**
```
[euxis] Agent: butler
[euxis] Provider: ollama
[euxis] ⠋ butler (ollama)...

Hello! I'm your Euxis Butler, optimized for voice and clear communication.
I help translate complex system outputs into natural spoken English.
The fleet has 41 agents ready to assist with your engineering tasks.
```

Notice how Euxis shows you what's happening:
- **Agent:** Which specialist is working
- **Provider:** Which AI is running (butler uses `ollama` for fast local execution)
- **Spinner:** Live status while the agent works

<details>
<summary><strong>Understanding Provider Selection</strong></summary>

Euxis picks the best AI provider for each agent:

| Agent Type | Provider | Why |
|------------|----------|-----|
| Strategic (orchestrator, architect) | `claude` | Best reasoning |
| Research (researcher) | `gemini` | Massive context |
| Coding (debugger, tester) | `goose` | Tool use |
| Utility (butler, writer) | `ollama` | Fast, local |

**Override anytime** by adding the provider as a third argument:
```bash
euxis butler "Introduce yourself" claude  # Force Claude instead
```

</details>

---

## Step 5: Run Your First Real Task

Now let's have an agent analyze something useful. Use the **Architect** to review your system:

```bash
euxis architect "Review the Euxis directory structure and summarize what each folder contains."
```

**Expected output:**
```
THOUGHT 1: I need to examine the Euxis directory structure to understand its organization and purpose.

ACTION 1: List the main directories in ~/.euxis to see the overall structure.

OBSERVATION 1: Found directories: bin/, config/, data/, docs/, prompts/, tests/, deploy/, and key files like registry.json

FINAL ANSWER:
# Euxis Directory Analysis

## Core Structure
- **bin/**: 30+ executable tools, symlinked to ~/bin/ for global access
- **config/**: Operational configuration (codex, patterns, playbooks, templates, branding)
- **prompts/**: Agent intelligence (9 core + 32 fleet agents)
- **registry.json**: Master agent registry with capabilities and routing

[... detailed breakdown continues ...]
```

✅ **Success checkpoint:** You deployed an agent that reasoned through a task using the ReAct pattern.

---

## What's Next

You've completed the quick start. Here's where to go from here:

### Try These Common Tasks

```bash
# Debug a problem
euxis debugger "Why would a login function return null for valid credentials?"

# Research options
euxis researcher "Compare the top 3 Python web frameworks for a REST API"

# Design something
euxis architect "Design a caching strategy for a high-traffic API"
```

### Level Up with Coordination

```bash
# Chain agents for refined output
euxis-combo run steve-jobs "Design a user notification system"

# Deploy a full squad for comprehensive work
euxis-squad deploy quality "Audit the authentication module"
```

### Learn the Concepts

| Guide | What You'll Learn |
|-------|-------------------|
| [Choosing Coordination](concepts/choosing-coordination.md) | When to use agents, combos, squads, or playbooks |
| [Agent Selection](concepts/agent-selection.md) | How to pick the right agent |
| [Workflows](workflows/) | Problem-to-solution tutorials |

### Complete Reference

| Guide | What You'll Learn |
|-------|-------------------|
| [Fleet Guide](fleet-guide.md) | All 41 agents in detail |
| [User Guide](user-guide.md) | Complete CLI reference |
| [API Reference](api-reference.md) | Build custom integrations |

---

## Troubleshooting

<details>
<summary><strong>Command not found: euxis</strong></summary>

**Problem:** Your shell can't find the Euxis commands.

**Solution:**
1. Verify PATH was updated: `echo $PATH | grep "bin"`
2. Restart your terminal completely
3. Re-run: `source ~/.profile` (or `~/.zshrc` for Zsh)

</details>

<details>
<summary><strong>Health check failures</strong></summary>

**Problem:** `euxis-health` reports failed checks.

**Solution:** Run the full certification to fix issues:
```bash
euxis-certify
```

This will identify and resolve common configuration problems.

</details>

<details>
<summary><strong>No AI provider available</strong></summary>

**Problem:** Agents can't execute because no provider is configured.

**Solution:** Install at least one provider from the [supported list](#ai-providers):
```bash
# Quickest option (local, free)
curl -fsSL https://ollama.ai/install.sh | sh
ollama pull llama2
```

</details>

---

*Euxis v0.0.7 · Build something that matters.*