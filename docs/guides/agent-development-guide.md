# Agent Development Guide

## Overview

This guide covers creating, testing, and deploying new agents in the Euxis fleet. Whether adding a permanent fleet agent or a third-party plugin, follow these steps to ensure quality and compatibility.

## Quick Start: Plugin Agent (5 minutes)

The fastest way to add a new agent is via the plugin API:

```bash
# 1. Create the prompt file
cat > ~/my-agent.txt <<'EOF'
---
agent_id: my-agent
role: Custom domain specialist
version: "0.0.3"
tags: [custom, domain]
last_updated: "current-02-02"
---

## Mandate
<What this agent does — one paragraph>

## Scope Boundaries
| In Scope | Out of Scope | Delegate To |
|----------|-------------|-------------|
| <domain tasks> | <excluded tasks> | <agent-id> |

## Primary Directives
1. <First directive>
2. <Second directive>

## Output Format
<Describe the expected output structure>
EOF

# 2. Create the manifest
cat > ~/my-agent-manifest.json <<'EOF'
{
  "agent_id": "my-agent",
  "role": "Custom domain specialist",
  "prompt_file": "/home/user/my-agent.txt",
  "tier": "standard",
  "tags": ["custom", "domain"]
}
EOF

# 3. Register
source ~/.euxis/core/lib/agents.sh
register_agent_plugin ~/my-agent-manifest.json

# 4. Test
euxis my-agent "Hello, verify you can respond"

# 5. Unregister (if needed)
unregister_agent_plugin "my-agent"
```

## Permanent Fleet Agent

For agents that should be part of the permanent fleet:

### Step 1: Create Prompt File

Place the file in `~/.euxis/agents/prompts/fleet/<agent-id>.txt`.

**Required YAML frontmatter:**
```yaml
---
agent_id: <must match filename>
role: <one-line description>
version: "0.0.3"
tags: [<category tags>]
last_updated: "<YYYY-MM-DD>"
---
```

**Required sections:**
- `## Mandate` — What the agent does
- `## Scope Boundaries` — Table of in/out scope with delegation targets
- `## Output Format` — Expected response structure
- `## Primary Directives` (or integrated into Mandate)

### Step 2: Add to Registry

Add the agent entry to `~/.euxis/agents/registry.json`:

```json
{
  "id": "my-agent",
  "role": "Custom domain specialist",
  "tier": "core|default|on-demand",
  "tags": ["category"],
  "provider_tier": "standard"
}
```

### Step 3: Configure Intelligence Tiering

If the agent needs a specific provider, add a case to `resolve_tiered_provider()` in `core/lib/providers.sh`:

```bash
# In resolve_tiered_provider():
my-agent)
    echo "claude" ;;  # or gemini, goose, ollama, etc.
```

**Tier selection guide:**
| Task Profile | Tier | Provider |
|-------------|------|----------|
| Strategic reasoning, architecture, quality | S-Tier | claude |
| Deep research, massive context | A-Tier | gemini |
| Enterprise/AWS-native | A-Tier | kiro-cli |
| Agentic tool use, coding | B-Tier | goose |
| Math/logic/optimization | B-Tier | qwen |
| Summaries, formatting, utility | C-Tier | ollama |
| Default | Standard | claude |

### Step 4: Add to Lint Registry

Add the agent name to the `AGENTS` array in `euxis-bin/euxis-lint`:

```bash
AGENTS=("architect" ... "my-agent")
```

### Step 5: Validate

```bash
# Syntax check
bash -n ~/.euxis/agents/prompts/fleet/my-agent.txt

# Lint validation (checks frontmatter, sections, registry)
euxis-lint

# Health check
euxis health

# Gym evaluation
euxis-gym my-agent "standard-task"
```

### Step 6: Test

Create a golden test in `tests/golden/`:

```bash
# tests/golden/my-agent.task
echo "Analyze the authentication module for security issues"

# tests/golden/my-agent.expected
# List expected output patterns (grep-checked)
FINAL ANSWER
SECURITY
```

## Prompt Engineering Standards

### Do
- Use `MUST`, `MUST NOT`, `ALWAYS`, `NEVER` for requirements
- Include concrete examples (few-shot) in the prompt
- Define explicit output format with JSON schema where applicable
- Reference the mandatory protocol sections by number (e.g., "per Protocol Section 2.5")
- Include a Scope Boundaries table with delegation targets

### Don't
- Duplicate instructions already in `_protocol.txt` or `_common.txt` — these are automatically loaded
- Use weak language ("should", "try to", "please")
- Leave scope boundaries ambiguous — every edge case needs a delegation target
- Create overly long prompts — use the token budget system (`EUXIS_PROTOCOL_TOKEN_BUDGET`)

### Prompt Inheritance

Every agent prompt automatically receives (in order):
1. `_common.txt` — shared agent instructions
2. `_protocol.txt` — mandatory protocol (ReAct, audit, memory, handoff)
3. Conditional protocols — loaded based on task keywords (security, versioning, etc.)
4. Agent-specific prompt — your file
5. Memory context — tiered memory retrieval
6. Fleet roster — valid agent IDs for delegation
7. Current task — the user's request

**Do not repeat content from layers 1-3 in your agent prompt.** This is the most common source of prompt redundancy.

## Common Workflow Examples

### Example 1: Code Review Agent

```
euxis reviewer "Review PR #42 for security and correctness"
```

Flow: reviewer reads diff → applies SECURITY-001 and QUALITY-001 patterns → produces findings with severity → outputs EUXIS_HANDOFF JSON

### Example 2: Orchestrated Multi-Agent Task

```
euxis architect "Design a caching layer for the API"
euxis-dispatch plan.json  # architect's output manifest
```

Flow: architect produces dispatch manifest → euxis-dispatch routes to sub-agents → each sub-agent produces structured intermediate → orchestrator synthesizes

### Example 3: Research + Implementation Pipeline

```
euxis researcher "Survey rate limiting approaches for REST APIs"
euxis architect "Design rate limiter based on research findings"
euxis debugger "Implement the rate limiter per architect spec"
euxis tester "Write tests for the new rate limiter"
euxis reviewer "Final review of rate limiter implementation"
```

## Debugging

```bash
# Enable debug logging
EUXIS_DEBUG=1 euxis my-agent "test task"

# Check lifecycle state
cat ~/.euxis/data/runtime/lifecycle/my-agent.state

# View performance metrics
tail -5 ~/.euxis/metrics/events.jsonl | jq .

# View recent memory
tail -20 ~/.euxis/data/runtime/projects/*/my-agent/memory.md

# Run lint for specific issues
euxis-lint 2>&1 | grep my-agent
```

## Architecture Reference

See the ADR documents for rationale behind key design decisions:
- `docs/adr/001-intelligence-tiering.md` — Why agents map to specific providers
- `docs/adr/002-tiered-memory-architecture.md` — Why MemGPT-inspired memory
- `docs/adr/003-dispatch-modes.md` — Why hierarchical/mesh/federated execution
- `docs/adr/006-agent-fleet-size.md` — Why 53 agents and when to consolidate

---

*Euxis v0.1.2 · Build something that matters.*
