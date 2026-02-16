# (c) 2026 Euxis Fleet. All rights reserved.
# ADR-006: Agent Fleet Size Justification

## Status
Accepted

## Context
Euxis operates a fleet of 50 specialized agents across four tiers (12 core, 24 default, 10 on-demand, 4 specialist). The fleet size has grown organically as new domain capabilities were added. Concern exists about coordination overhead and maintainability at this scale. The review-prompts framework demonstrates that focused single-purpose systems can achieve excellent domain-specific results with minimal infrastructure.

## Decision
Maintain the 50-agent architecture with the following constraints:

1. **Core agents (12) are fixed.** Orchestrator, architect, reviewer, critic, auditor, librarian, planner, arbiter, historian, route, pair, and guard form the governance layer. These are required for multi-agent coordination, routing, and safety gates.

2. **Fleet agents are demand-loaded.** Fleet agents are only instantiated when dispatched. An idle agent consumes zero resources (it's just a prompt file on disk). Coordination overhead only occurs during active multi-agent dispatch.

3. **Agent consolidation criteria.** An agent MUST be consolidated or removed if:
   - It has not been dispatched in 90 days (track via lifecycle transitions.jsonl)
   - Its scope overlaps >70% with another agent (measure via tag intersection in registry.json)
   - It cannot pass the gym evaluation with >80% quality score

4. **Maximum fleet size: 50 agents.** Beyond this, coordination complexity exceeds the benefit of specialization. New agents above this threshold must replace an existing agent.

5. **Plugin API for extensibility.** Third-party agents register via manifest files rather than modifying core fleet. Plugins are isolated and can be added/removed without affecting fleet integrity.

## Alternatives Considered

1. **Consolidate to 10-15 agents** — Would reduce maintenance but lose domain specialization. The pentester (security), custodian (Rust ecosystem), and ledger (PCI compliance) each carry specialized knowledge that a generic "security-agent" cannot replicate.

2. **Dynamic agent spawning** — Create agents on-the-fly based on task analysis. Rejected because prompt engineering quality degrades without curated, tested prompts. Euxis-synthesize provides this capability for ad-hoc needs while maintaining quality standards for the permanent fleet.

3. **Hierarchical sub-fleets** — Group agents into domain sub-fleets (security-fleet, dev-fleet, ops-fleet) with sub-orchestrators. Deferred as premature — the current flat structure with dispatch modes (hierarchical/mesh/federated) provides sufficient coordination patterns without adding another abstraction layer.

## Consequences
- **Maintenance cost:** Each agent prompt must be version-synced, validated by euxis-lint, and tested via euxis-gym. The lint check (CHECK 1) enforces registry completeness.
- **Coordination overhead:** Measured via perf/metrics.jsonl. Latency budgets enforce that prompt assembly stays <500ms and total overhead stays predictable regardless of fleet size.
- **Quality floor:** The gym evaluation and reviewer quality gate ensure no agent degrades the overall system quality.
- **Extensibility:** The plugin API allows third-party agents without modifying core fleet, reducing merge conflicts and maintenance burden.
