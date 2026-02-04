# Agents

## What They Are

Agents are specialist AI workers that execute specific domains with defined authority, scope, and escalation paths. Each agent has a unique role, optimal model tier routing, and clear boundaries about what they can and cannot do.

## When to Use

- **Single domain task**: Use a specific agent when you need expertise in their domain
- **Quality gate**: Use `reviewer` to validate outputs before shipping
- **Coordination**: Use `orchestrator` to break down complex goals into specialist tasks
- **Risk assessment**: Use `system-critic` to challenge assumptions and surface hidden risks

## Quick Example

```bash
# Single agent for bug fixing
euxis bug-fixer "Debug the auth timeout in login.py"

# Core agent with blocking authority
euxis reviewer "Review this PR for correctness and security"

# Agent with specific model tier
euxis perf-optimizer "Optimize database query performance" ollama
```

## Agent Tiers

### Core (7) — Authority-bearing, always present
Block progress to maintain quality. If one is missing, the system is incomplete.

| Agent | Authority Domain |
|-------|------------------|
| `orchestrator` | Coordination, synthesis, final assembly |
| `architect` | Structure, patterns, design decisions |
| `product-manager` | Intent, scope, prioritization |
| `reviewer` | Truth & quality gate |
| `librarian` | Memory, knowledge continuity |
| `compliance-officer` | Legal, privacy, regulatory |
| `system-critic` | Risk, pre-mortems, counter-bias |

### Default (20) — Auto-available, task-triggered
Execute and advise within scope. Do not define direction.

**Engineering**: `automation-engineer`, `bug-fixer`, `crash-investigator`, `legacy-maintainer`, `unit-tester`
**Quality**: `data-steward`, `edge-hunter`, `language-specialist`, `perf-optimizer`, `qa-coordinator`, `regression-analyst`
**Operations**: `incident-commander`, `release-manager`, `security-lead`
**Experience**: `tech-writer`, `ux-sentinel`, `cli-ui-artisan`, `web-ui-architect`, `theming-and-motion-engineer`, `interaction-and-input-specialist`

### On-Demand (7) — Explicit invocation only
Add leverage, not safety. Never block, never override core authority.

| Agent | Domain |
|-------|--------|
| `brand-evangelist` | Brand voice and storytelling |
| `butler` | Perfect voice summaries |
| `deep-researcher` | Find what others miss |
| `devrel-advocate` | Build community |
| `globalization-lead` | Global reach |
| `growth-marketer` | Scale your impact |
| `social-manager` | Amplify your voice |

### Specialist (4) — Domain-specific expertise
Activated for domain-scoped tasks only.

| Agent | Expertise |
|-------|-----------|
| `crypto-cryptography-auditor` | Cryptographic correctness, PQC, hashing |
| `payments-domain-steward` | ISO 20022, payments compliance |
| `realtime-audio-engineer` | Real-time audio, latency, DSP |
| `rust-crate-steward` | Rust crate publishing, MSRV |

## Model Tier Routing

Agents automatically route to optimal models based on task complexity:

- **S-Tier (Strategic)**: Complex reasoning, architecture → claude
- **A-Tier (Research)**: Deep analysis, massive context → gemini, amazon-q
- **B-Tier (Coding)**: Agentic tool use → goose, opencode, qwen
- **C-Tier (Utility)**: Summaries, formatting → ollama, crush, kilo

**Override**: Explicit provider argument always overrides tier routing.
**P0 Priority**: Always uses S-Tier regardless of default routing.

## Create Your Own

### Plugin Agent (5 minutes)

```bash
# 1. Create prompt file
cat > ~/my-specialist.txt <<'EOF'
---
agent_id: my-specialist
role: Custom domain specialist
version: "0.0.7"
tags: [custom, domain]
last_updated: "2026-02-02"
---

## Mandate
This agent specializes in custom domain tasks with specific expertise.

## Scope Boundaries
| In Scope | Out of Scope | Delegate To |
|----------|-------------|-------------|
| Custom domain analysis | General coding | bug-fixer |
| Domain validation | Architecture | architect |

## Primary Directives
1. Analyze domain-specific requirements
2. Validate domain constraints
3. Escalate architectural decisions

## Output Format
- **Analysis**: Domain-specific findings
- **Validation**: Pass/Fail with rationale
- **Recommendations**: Actionable next steps
EOF

# 2. Create manifest
cat > ~/my-specialist-manifest.json <<'EOF'
{
  "agent_id": "my-specialist",
  "role": "Custom domain specialist",
  "prompt_file": "/home/user/my-specialist.txt",
  "tier": "standard",
  "tags": ["custom", "domain"]
}
EOF

# 3. Register and test
source ~/.euxis/bin/lib/agents.sh
register_agent_plugin ~/my-specialist-manifest.json
euxis my-specialist "Test task"

# 4. Unregister if needed
unregister_agent_plugin "my-specialist"
```

### Fleet Agent

For permanent fleet addition, place prompt in `~/.euxis/prompts/fleet/<agent-id>.txt` and update the registry.

## Authority Model

- **Core agents** can block progress to maintain quality
- **Default agents** execute within scope, advise but don't override
- **On-demand agents** add leverage without safety authority
- **Specialist agents** provide deep domain expertise only

**Escalation**: Every agent must know when to delegate outside their scope and to whom.

## Available Options

```bash
# Single agent with provider override
euxis <agent> "<task>" [provider]

# Agent with orchestrator coordination
euxis orchestrator "Complex goal requiring multiple specialists"

# Agent status and capabilities
euxis-health  # Check all agent integrity
```

See [Fleet Guide](../fleet-guide.md) for complete agent reference and [User Guide](../user-guide.md) for all CLI commands.