# ADR-001: Intelligence Tiering

## Status
Accepted

## Context
Euxis operates a fleet of 30+ specialized agents, each with distinct cognitive profiles (strategic planning, deep research, coding, utility tasks). Running all agents on a single provider wastes cost on simple tasks and under-serves complex ones. Different AI providers excel at different task types — Claude at reasoning, Gemini at large-context research, local models at zero-latency utility work.

## Decision
Implement a tiered intelligence routing system that maps each agent to its optimal provider based on task complexity profile:

- **S-Tier (Strategic):** orchestrator, architect, product-manager, reviewer → Claude (best reasoning and tool use)
- **A-Tier (Research):** deep-researcher, compliance-officer → Gemini (massive context window)
- **A-Tier (Enterprise):** incident-commander → Amazon Q (AWS-native security context)
- **B-Tier (Coding):** bug-fixer, unit-tester, automation-engineer → Goose (agent-native tool use)
- **B-Tier (Local Code):** legacy-maintainer → OpenCode (specialized local models)
- **B-Tier (Math/Logic):** perf-optimizer, data-steward → Qwen (algorithmic optimization)
- **C-Tier (Utility):** butler, librarian, tech-writer → Ollama (zero latency, no cost)

P0-priority tasks always override to Claude (Strategic tier) regardless of agent assignment. Explicit provider arguments always override tiering.

## Alternatives Considered
1. **Single provider for all agents** — Simpler but wasteful. Utility tasks don't need frontier models, and research tasks benefit from Gemini's 1M+ token context.
2. **Per-task dynamic routing** — Route based on task content rather than agent identity. Rejected because agent identity already encodes task complexity (an architect task is inherently strategic), and content-based routing adds latency and complexity.
3. **User-configured mapping** — Let users define their own tier map. Rejected as default because it shifts cognitive load to the user; the system should work well out of the box. Explicit provider override serves power users.

## Consequences
- **Cost reduction:** C-Tier agents (butler, librarian, tech-writer) run on free local models, eliminating API costs for routine tasks.
- **Quality improvement:** Strategic agents get the best available reasoning model; research agents get the largest context window.
- **Vendor lock-in risk:** The tiering assumes specific provider strengths that may shift over time. Mitigated by the explicit override mechanism and centralized mapping in `resolve_tiered_provider()`.
- **Maintenance overhead:** New providers or model upgrades require updating the tier map. Centralized in a single function to minimize churn.
