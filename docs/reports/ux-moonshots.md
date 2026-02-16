# Euxis UX Moonshots

## Purpose
Document two long-horizon UX initiatives that would redefine Euxis interaction, aligned to Ink & Switch local-first and malleable software principles.

---

## Moonshot 1: Local-first Session Studio

**Vision**: A hybrid TUI/Web/Native environment where sessions, workflows, and traces are first-class local files. Users can fork, diff, merge, and sync interactions without server dependency.

### What it unlocks
- Branchable sessions and workflows (Patchwork-inspired)
- Offline-first operation with delayed sync (Local-first)
- User-owned artifacts with explicit export/import

### Core capabilities
- Session bundle format (`.euxis-session`) with:
  - `metadata.json` (session meta, channel, route, owner)
  - `trace.jsonl` (event stream)
  - `messages.jsonl` (chat history)
  - `workflows.json` (agent chains, overrides)
- Visual diff and merge of session branches
- Conflict resolution UI for concurrent edits
- Local search across all sessions and traces

### Minimum technical requirements
- Structured trace emission from Gateway
- Deterministic export/import endpoints
- Local storage indexer
- Conflict-safe merge strategy (CRDT or patch-based)

### Risks
- Versioning complexity of trace schema
- Merge UX for non-technical users

---

## Moonshot 2: Spatial Orchestration Board

**Vision**: A physical/virtual workspace where agents, sessions, and tools are tangible objects. Users can move, connect, and rewire them directly, making orchestration spatial and intuitive.

### What it unlocks
- Visual reasoning about multi-agent flows
- Direct manipulation of routing and approvals
- Collaborative “room-scale” orchestration

### Core capabilities
- Spatial canvas for sessions, agents, and tools
- Live topology: edges show routing, approvals, and tool calls
- Gesture-driven reconfiguration of flows
- Persistent spatial layouts per user/team

### Minimum technical requirements
- Real-time topology API for sessions and agents
- Event-driven UI updates with latency bounds
- Layout persistence and portability

### Risks
- Higher interaction complexity without strong onboarding
- Need for consistent spatial metaphors across surfaces
