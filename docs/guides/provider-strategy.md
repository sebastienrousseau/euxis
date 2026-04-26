# Provider Strategy Guide

Euxis uses a semantic task classification system to route agent workloads to
the optimal LLM provider. This replaces simple tier-based routing with
workload-aware provider selection.

## Task Classes

11 task classes map to provider specializations:

| Task Class | Primary Provider | Use Case |
|-----------|-----------------|----------|
| `research` | OpenAI | General research, summarization |
| `multilingual_analysis` | OpenAI | Multi-language analysis |
| `policy_synthesis` | OpenAI | Policy document generation |
| `coding` | Claude | Code generation, debugging |
| `architecture` | Claude | System design, architecture review |
| `audit` | Claude | Code audit, compliance review |
| `deep_research` | Gemini | Long-context research |
| `security` | Gemini | Security analysis, vuln scanning |
| `private_coding` | Ollama | Air-gapped / private code |
| `surgical_edit` | Aider | Targeted file edits |
| `terminal_automation` | Kiro/ShellGPT | Shell command generation |

## Configuration

Strategy routing is configured in `data/config/provider_strategy.json`:

```json
{
  "defaults": {
    "coding": {
      "primary": "claude",
      "fallback": ["gemini", "openai", "ollama"]
    },
    "security": {
      "primary": "gemini",
      "fallback": ["claude", "openai"]
    }
  },
  "agent_class_hints": {
    "librarian": "research",
    "sentinel": "security",
    "architect": "architecture"
  },
  "classification_keywords": {
    "security": ["vulnerability", "cve", "exploit", "threat"]
  }
}
```

## Classification Priority

Task class is determined in this order:

1. **Environment override** (`EUXIS_DEFAULT_*_PROVIDER`)
2. **Agent class hints** (per-agent mapping in config)
3. **Pillar classification** (security pillar → security class)
4. **Keyword matching** (prompt content analysis)
5. **Tier fallback** (Reason→architecture, Code→coding, etc.)

## Environment Overrides

Override provider selection per task class:

```bash
# Force all research tasks to Gemini
export EUXIS_DEFAULT_RESEARCH_PROVIDER=gemini

# Force all coding tasks to Ollama (local)
export EUXIS_DEFAULT_CODING_PROVIDER=ollama

# Force all security tasks to Claude
export EUXIS_DEFAULT_SECURITY_PROVIDER=claude
```

## Route Resolution

The full resolution chain for a given agent:

1. Explicit `--provider` flag (highest priority)
2. `EUXIS_LOCAL_ONLY=1` forces everything to Ollama
3. Environment variable for this task class
4. Strategy config: try primary provider, then fallbacks
5. Tier-based fallback (original routing)

At each step, the system checks provider availability (API keys for
cloud providers, PATH detection for tools like aider/kiro).

## Fallback Chains

When the primary provider is unavailable, fallback chains activate:

```
Claude unavailable → OpenAI → Gemini → Ollama
OpenAI unavailable → Claude → Gemini → Ollama
Gemini unavailable → Claude → OpenAI → Ollama
```

Custom fallback chains can be configured in `router.json`:

```json
{
  "model_fallback": {
    "claude-opus-4-6": [
      {"provider": "gemini", "model": "gemini-2.5-pro"},
      {"provider": "openai", "model": "gpt-5.4"}
    ]
  }
}
```

## Tool Detection

For CLI tool providers (aider, kiro, sgpt), availability is checked
via PATH lookup. The result is cached for the session lifetime and
invalidated on config hot-reload.

## Hot Reload

Both `router.json` and `provider_strategy.json` support hot reload.
When file timestamps change, the router automatically re-reads both
configs without requiring a restart. This enables:

- Changing provider routing mid-session
- A/B testing different routing strategies
- Emergency provider failover

## Observability

Route decisions are logged with `route_reason` and `task_class` fields
in the verdict artifact, enabling post-hoc analysis of routing behavior.

```bash
# See routing decisions for a run
euxis check --provider-verbose .
```
