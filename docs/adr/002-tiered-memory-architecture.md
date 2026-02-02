# ADR-002: Tiered Memory Architecture

## Status
Accepted

## Context
Agents need persistent memory across sessions to avoid repeating work and to build on prior discoveries. A flat memory file (append-only markdown) doesn't scale — as the file grows, including all of it in the prompt wastes tokens on irrelevant history. Agents also need access to sibling agents' knowledge without tight coupling.

Inspired by MemGPT's hierarchical memory model (core/archival/recall), we needed a retrieval strategy that balances recency, relevance, and cross-agent awareness.

## Decision
Implement a three-tier memory retrieval system:

- **Tier 1: Hot Memory** — The 20 most recent entries from the agent's memory file. Always included. Provides continuity with the immediate prior session.
- **Tier 2: Relevant Memory** — Keyword-matched entries from the full memory file. Extracts keywords (5+ chars) from the current task, builds a grep alternation pattern, and returns the top 10 unique matches. Surfaces older but task-relevant memories.
- **Tier 3: Cross-Agent Memory** — Keyword-matched entries from sibling agents' memory files within the same project. Read-only. Enables an architect to see what the bug-fixer discovered, or a reviewer to access the unit-tester's findings, without explicit delegation.

All three tiers are assembled by `build_tiered_memory()` and injected into the prompt under clearly labeled sections so the agent can distinguish recency from relevance from cross-agent context.

Additionally, the Cortex subsystem (`euxis-cortex`) provides typed vector memory (episodic, semantic, procedural) with embedding-based recall for deeper retrieval beyond keyword matching.

## Alternatives Considered
1. **Full memory inclusion** — Include the entire memory file in every prompt. Rejected because memory files grow unbounded and would exceed context limits within a few sessions.
2. **Embedding-only retrieval** — Use vector similarity exclusively (like a RAG system). Rejected as the sole mechanism because it requires an embedding model at boot time, adding latency. Keyword matching is instant and sufficient for most cases. The Cortex provides embedding-based recall as an opt-in deeper layer.
3. **No cross-agent memory** — Each agent only sees its own history. Rejected because multi-agent workflows (dispatch, squads) produce knowledge that's valuable across agents. The architect's structural analysis helps the reviewer; the bug-fixer's findings help the unit-tester.

## Consequences
- **Token efficiency:** Only relevant memories are included, keeping prompt size manageable regardless of memory file size.
- **Graceful degradation:** If memory files are empty or keywords don't match, each tier returns empty markers (`<empty>`, `<no matches>`) without breaking the prompt.
- **Cross-agent leakage risk:** Tier 3 exposes sibling memory, which could include sensitive findings. Mitigated by project-level isolation (agents only see siblings within the same project directory).
- **Keyword matching limitations:** Simple grep-based matching misses semantic similarity (e.g., "authentication" won't match "login"). The Cortex vector memory addresses this for users who opt in.
