# (c) 2026 Euxis Fleet. All rights reserved.
# ADR-004: AI Agent Architecture Patterns

## Status
Accepted

## Context
Research into state-of-the-art AI agent frameworks reveals several architectural patterns that inform Euxis's design. This ADR documents key findings from analyzing review-prompts, AutoGPT, CrewAI, LangGraph, Claude Code, and academic literature on multi-agent systems.

## Decision
Adopt a hybrid architecture combining the best patterns from each approach:

### Pattern 1: Conditional Context Loading (from review-prompts)
Load domain-specific knowledge only when task analysis indicates relevance. This reduces token usage by 60-70% compared to loading all protocols unconditionally. Implemented via `resolve_protocols()` with keyword matching and token budget enforcement.

### Pattern 2: Intelligence Tiering (original to Euxis)
Route agents to optimal providers based on cognitive complexity profiles. Strategic agents use frontier models; utility agents use local models. This reduces cost without sacrificing quality where it matters.

### Pattern 3: MemGPT-Inspired Tiered Memory
Three-tier retrieval (hot/relevant/cross-agent) balances recency, relevance, and inter-agent knowledge sharing. This addresses the fundamental limitation of flat append-only memory files.

### Pattern 4: ReAct Reasoning Loops (from Yao et al. 2023)
Mandatory Thought → Action → Observation cycles force agents to ground claims in verifiable evidence. Combined with evidence-based verification requirements, this dramatically reduces hallucination.

### Pattern 5: Structured Intermediates for Pipelines
Typed JSON artifacts (`json-intermediate` blocks) enable machine-parseable handoffs between agents. This replaces brittle markdown parsing in multi-agent workflows.

### Pattern 6: Slash Commands for Fast-Path Execution (from review-prompts)
Lightweight command shortcuts that bypass full agent bootstrap for common workflows. Reduces latency from 2-4s to <500ms for simple operations.

## Research Summary: Key AI Agent Frameworks

### AutoGPT / BabyAGI
- **Strength:** Autonomous goal decomposition and task planning
- **Weakness:** Unbounded execution, difficulty knowing when to stop
- **Euxis adoption:** Bounded execution via `--max-turns` and phased dispatch

### CrewAI
- **Strength:** Role-based agent specialization with clear delegation
- **Weakness:** Python-only, heavy runtime dependencies
- **Euxis adoption:** Agent role specialization via prompt-defined mandates; bash keeps runtime minimal

### LangGraph
- **Strength:** Stateful agent workflows as directed graphs
- **Weakness:** Complex API, steep learning curve
- **Euxis adoption:** Hierarchical/mesh/federated dispatch modes provide graph-like coordination without the complexity

### Claude Code (Anthropic)
- **Strength:** Tool use with permission controls, agentic coding
- **Weakness:** Single-provider, no multi-agent orchestration
- **Euxis adoption:** Uses Claude Code as the S-Tier provider; adds multi-agent coordination on top

### MemGPT (Packer et al. 2023)
- **Strength:** Hierarchical memory management for LLM agents
- **Weakness:** Requires embedding model infrastructure
- **Euxis adoption:** Keyword-based tiered retrieval as lightweight alternative; Cortex provides embedding-based recall as opt-in layer

## Alternatives Considered
1. **Python-based framework** (like CrewAI) — Rejected: adds runtime dependency, slower startup, less portable than bash
2. **Single-agent with tools** (like Claude Code alone) — Rejected: insufficient for complex multi-step workflows requiring diverse expertise
3. **Full graph-based orchestration** (like LangGraph) — Deferred: current dispatch modes cover practical cases; can add DAG execution later

## Consequences
- **Positive:** Euxis combines proven patterns from multiple frameworks while staying lightweight (bash, no runtime deps)
- **Negative:** Bash imposes limitations (no native async, limited data structures) that may require workarounds for complex coordination
- **Trade-off:** Simplicity and portability over feature completeness — Euxis will never match Python frameworks for flexibility, but will always be faster to bootstrap and easier to deploy
