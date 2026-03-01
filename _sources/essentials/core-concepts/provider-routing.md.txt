# Provider Routing

**How Euxis selects AI providers.**

Euxis uses a default provider plus a fallback chain. You can override per command or per session.

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

## Provider Selection

Default provider:
- `EUXIS_DEFAULT_PROVIDER` (default: `claude`)

Fallback chain:
- `EUXIS_FALLBACK_CHAIN` (default: `claude:gemini:openai:kiro-cli:qwen:crush:goose:ollama`)

Override the provider per command by passing it as the last argument. You can also override for a whole session using `EUXIS_SESSION_PROVIDER`.

---

## Available Providers

| Provider | CLI | Notes |
|----------|-----|-------|
| `claude` | claude | Reasoning, strategy |
| `gemini` | gemini | Large context research |
| `openai` | codex | OpenAI models via Codex CLI |
| `kiro-cli` | kiro-cli | Provider-specific coding workflows |
| `goose` | goose | Tool use, coding |
| `qwen` | qwen | Math, logic |
| `crush` | crush | Multi-model TUI |
| `ollama` | ollama | Local, zero cost |

---

## P0 Override

For critical tasks marked as P0, Euxis always routes to `claude` regardless of the agent's default tier.

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
export EUXIS_OLLAMA_MODEL="llama3.2"
export EUXIS_GOOSE_MODEL="claude-sonnet-4"
export EUXIS_QWEN_MODEL="qwen3-coder"
```
