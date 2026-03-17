# Euxis Network C++

The `euxis::network` module provides the high-performance, asynchronous communication layer for the Euxis Agent OS. It implements zero-trust mesh topologies, MCP interoperability, and thread-safe resilience patterns.

## Universal Tool Interoperability

The `McpClient` bridges the Euxis fleet to external Model Context Protocol (MCP) servers.

* **Precondition**: Ensure a valid `A2AMessage` transport stream is established.
* **Postcondition**: Returns a monadic `std::expected` resolving the JSON-RPC execution.

For discovering and invoking remote capabilities, use the asynchronous WebSocket transport. The client enforces strict schema validation and timeout recovery.

## Hardware-Aware Topology

The `TopologyGrid` maps the capability distribution across the Agent OS mesh.

* **Precondition**: Grid coordinates must fall within the allocated boundary.
* **Postcondition**: Grants $O(1)$ access to topological costs.

For low-latency index lookups, the grid uses a flat `std::vector` to maximize cache locality. Access matrix elements using the C++23 multidimensional subscript operator:

```cpp
// Evaluate network cost in O(1)
double cost = grid[source_idx, target_idx];
```

## Concurrency & Resilience

The `CircuitBreaker` and `RetryPolicy` enforce strict execution bounds.

* **RAII**: Resource-bound lifetime management. 
* **UB**: Undefined Behavior — Avoid data races on shared state.

For concurrent state mutations, always use `std::lock_guard` with `std::mutex`. The network layer implements high-resolution `std::chrono::steady_clock` boundaries to prevent integer rounding failures during sub-second timeout evaluations.