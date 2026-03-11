# Euxis ETX: Desktop Orchestration Console

The `euxis-etx` module is the Qt6-powered desktop interface for the Euxis Agent OS. It provides a real-time, low-latency visual command center for monitoring fleet health, orchestrating agents, and auditing cryptographic provenance.

## Hardware-Accelerated UI

ETX leverages the native GPU via Qt6 RHI (Rendering Hardware Interface).

* **Precondition**: A compliant OpenGL, Vulkan, or Metal graphics driver is required.
* **Postcondition**: Renders a high-fidelity dashboard maintaining a consistent 60+ FPS under heavy telemetry load.

For high-throughput telemetry updates, the UI pipeline utilizes zero-copy structures to pipe data from the `euxis::network` backends into Qt models, bypassing expensive string formatting where possible.

## Omnigraph Integration

ETX visualizes the `TopologyGrid` and agent interaction patterns using the Omnigraph view.

* **ADL**: Argument-Dependent Lookup — Resolving function overloads dynamically based on argument types.

The graph dynamically resolves rendering strategies based on the active platform capabilities, delivering granular insight into the $O(1)$ SIMD-driven auction bids and task delegations occurring in the core mesh.

## Security Context

The GUI natively enforces the NHI (Non-Human Identity) IAM matrix.

* **UB**: Undefined Behavior — Avoid forcing state mutations outside of the established governance tiers.

All operational commands dispatched through ETX are funneled through the strict cryptographic gating mechanisms defined in the `euxis::security` module. The GUI will visibly block interactions that violate the `sentinel-identity` signing constraints.