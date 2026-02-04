# Provider Routing

**How Euxis matches agents to AI providers.**

Euxis automatically selects the best AI provider for each agent. You can override this anytime, but you rarely need to.

---

## Quick Reference

**Command syntax:**
```bash
euxis <agent> "<task>" [provider]
```

**Examples:**
```bash
euxis architect "Design the API"              # Uses default (claude)
euxis architect "Design the API" gemini       # Override to gemini
euxis debugger "Fix the bug" ollama           # Override to ollama
```

---

## The Simple Version

Different AI models are good at different things:

- **Claude** excels at reasoning and complex decisions
- **Gemini** handles massive context (2M+ tokens)
- **Goose** has native tool use for coding
- **Ollama** runs locally with zero latency

Euxis matches each agent to the provider that best fits its work.

---

## How Routing Works

When you run:

```bash
euxis architect "Design the API"
```

Euxis:
1. Looks up the agent (`architect`)
2. Checks its tier (Strategic → `claude`)
3. Displays the selection: `[euxis] Provider: claude`
4. Executes with that provider

You always see which provider is running.

---

## The Routing Table

### Strategic Tier → Claude

For complex reasoning and decision-making.

| Agent | Why Claude |
|-------|-----------|
| `orchestrator` | Coordinates complex multi-step work |
| `architect` | Makes design decisions |
| `planner` | Prioritizes and scopes |
| `reviewer` | Validates quality |
| `critic` | Challenges assumptions |
| `auditor` | Compliance reasoning |
| `arbiter` | Resolves conflicts |
| `historian` | Pattern recognition |

### Research Tier → Gemini

For massive context analysis.

| Agent | Why Gemini |
|-------|-----------|
| `researcher` | Analyzes large documents and codebases |

### Coding Tier → Goose

For agent-native tool use.

| Agent | Why Goose |
|-------|-----------|
| `debugger` | Reads and writes code |
| `tester` | Executes tests |
| `automaton` | Runs CI/CD commands |
| `maintainer` | Modifies codebases |

### Utility Tier → Ollama

For fast, local execution.

| Agent | Why Ollama |
|-------|-----------|
| `butler` | Quick summaries |
| `writer` | Document generation |
| `librarian` | Knowledge retrieval |

### Standard Tier → Claude (Default)

All other agents use Claude as a reliable default.

---

## Overriding the Provider

Add the provider as the third argument:

```bash
euxis architect "Design the API" gemini
```

Common override reasons:

| Situation | Override |
|-----------|----------|
| Need more context | Use `gemini` |
| Want local/offline | Use `ollama` |
| Testing different models | Use any provider |
| Cost optimization | Use `ollama` or `qwen` |

---

## Available Providers

| Provider | CLI | Best For |
|----------|-----|----------|
| `claude` | claude | Reasoning, strategy |
| `gemini` | gemini | Large context research |
| `ollama` | ollama | Local, zero cost |
| `goose` | goose | Tool use, coding |
| `qwen` | qwen | Math, logic |
| `crush` | crush | Multi-model TUI |
| `kiro-cli` | kiro-cli | AWS/enterprise |
| `codex` | codex | OpenAI models |

---

## P0 Override

For critical tasks marked as P0 (highest priority), Euxis always routes to `claude` regardless of the agent's default tier.

This ensures maximum reliability for critical work.

---

## Checking Provider Availability

```bash
euxis-health
```

The health check verifies which providers are installed and working.

---

## Environment Variables

Configure default models per provider:

```bash
export EUXIS_OLLAMA_MODEL="llama3.2"      # Default for ollama
export EUXIS_GOOSE_MODEL="claude-sonnet-4" # Default for goose
export EUXIS_QWEN_MODEL="qwen3-coder"      # Default for qwen
```

---

## When to Override

### Use Gemini When:
- Analyzing large codebases
- Processing long documents
- Need 2M+ token context

### Use Ollama When:
- Working offline
- Minimizing costs
- Simple tasks (summaries, formatting)
- Sensitive data (stays local)

### Use Claude When:
- Complex reasoning
- Important decisions
- Quality is critical

### Use Goose When:
- Heavy tool use
- File system operations
- Running commands

---

## Trust the Defaults

The routing system is tuned for optimal results. Override when you have a specific reason, but trust the defaults for general use.

```bash
# Usually best to let Euxis choose
euxis researcher "Compare testing frameworks"  # → gemini (large context)
euxis debugger "Fix the null pointer"          # → goose (tool use)
euxis reviewer "Review the PR"                 # → claude (reasoning)
```

---

*Euxis v0.0.7 · Build something that matters.*
