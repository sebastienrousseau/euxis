# (c) 2026 Euxis Fleet. All rights reserved.
# ADR-003: Dispatch Execution Modes

## Status
Accepted

## Context
The orchestrator produces mission manifests that deploy multiple agents to accomplish complex tasks. Not all multi-agent tasks have the same coordination requirements. A code review needs sequential handoffs (architect → reviewer → bug-fixer), while a security audit benefits from parallel independent analysis that gets merged. Some tasks need peer-to-peer agent communication without a central coordinator.

A single dispatch strategy would either over-serialize parallel-safe work or under-coordinate dependent work.

## Decision
Implement three dispatch execution modes, selectable per manifest:

- **Hierarchical Mode** (default) — The orchestrator dispatches agents in phased waves. Each phase completes before the next begins. Agents report back to the orchestrator, which synthesizes results and plans the next phase. Best for tasks with strict dependencies (build → test → deploy).

- **Mesh Mode** — All dispatched agents run concurrently and can read each other's outputs via shared `review-context/` directories. No central coordinator during execution. A synthesis agent runs after all agents complete to merge findings. Best for parallel independent analysis (security audit, multi-reviewer code review).

- **Federated Mode** — Agents operate autonomously with a shared message bus (`euxis-bus`). Agents publish findings to topics and subscribe to topics they depend on. No phased gating. Best for long-running autonomous workflows where agents may produce results at different times.

The dispatch mode is declared in the mission manifest's `mode` field. The `euxis-dispatch` script reads this field and selects the execution strategy. Structured JSON intermediates (agent output blocks with typed findings and artifacts) enable machine-parseable handoffs between agents in all modes.

## Alternatives Considered
1. **Hierarchical only** — Simpler, single execution model. Rejected because it serializes inherently parallel work, wasting time on tasks like multi-agent code review where reviewers don't depend on each other.
2. **Fully autonomous agents** — No coordination at all; agents self-organize. Rejected because it provides no guarantees about task completion, conflict resolution, or output synthesis. Useful as a research direction but not reliable for production workflows.
3. **DAG-based execution** — Define agent dependencies as a directed acyclic graph and execute with maximum parallelism. Considered but deferred — it's a refinement of hierarchical mode that adds complexity. The three-mode system covers the practical cases; DAG execution can be added later as an optimization of hierarchical mode.

## Consequences
- **Flexibility:** Users can match the dispatch strategy to the task's coordination requirements, avoiding both under- and over-serialization.
- **Complexity:** Three execution paths means three code paths to maintain in `euxis-dispatch`. Mitigated by shared infrastructure (structured intermediates, output capture, provider routing).
- **Mode selection burden:** Users must understand which mode fits their task. Mitigated by making hierarchical the default (works for all cases, just not optimally for parallel work) and documenting clear selection criteria.
- **Structured intermediates dependency:** Mesh and federated modes depend on agents producing parseable JSON intermediates. If an agent produces only markdown, downstream synthesis degrades to text-based merging.
