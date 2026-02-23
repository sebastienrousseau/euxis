# The Inter-Agent Mesh

The Euxis autonomous mesh defines how isolated Wasm agents federate execution logic to accomplish monolithic tasks. Euxis does not operate synchronously or natively via standard HTTP polling between its agents.

## Zero-Copy Serialization

Traditional integration frameworks force JSON deserialization overhead on every inter-agent call. Euxis fundamentally bypasses this payload stringification utilizing the underlying Extism shared memory allocations.

1. Agent A completes processing.
2. The orchestrator maps Agent A's output byte buffer natively into Agent B's WebAssembly linear memory block.
3. Agent B begins execution with instant access to the binary structure.

## Agent Squads

A **Squad** represents a group of agents wired into an ephemeral topology graph.

When you dispatch a complex command ("Refactor this React application for strict TypeScript access"), your system instantiates the `Squad Orchestrator`. It spins up specialized Wasm modules running isolated tasks concurrently:

* **The Architect:** Drafts the component boundaries.
* **The Tester:** Executes parallel Vitest checks.
* **The Engineer:** Commits syntax changes.

Each agent within the squad communicates rapidly via memory manipulation handled entirely by the host process.

## Topology Constraints

The mesh is exclusively limited to a single host. If you invoke agents spanning disparate network environments (e.g., AWS Lambda and local machine nodes), Euxis utilizes the Gateway architecture to bridge the linear memory graphs over WebSocket telemetry streams. Explore the [Gateway Setup Manual](gateway.md) for remote architecture requirements.
