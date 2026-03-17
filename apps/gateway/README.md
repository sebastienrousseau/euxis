# Euxis Gateway C++

The `euxis-gateway` is the high-throughput, asynchronous entry point for the Euxis Agent OS. It orchestrates traffic between external clients, internal agent swarms, and the MCP ecosystem.

## Asynchronous Communication

The gateway implements a persistent, duplex `WebSocketHub`.

* **Precondition**: Ensure the host network permits traffic on the configured port.
* **Postcondition**: Exposes a real-time, bidirectional A2A (Agent-to-Agent) message bus.

For optimal sub-millisecond dispatching, deploy the `WebSocketA2ATransport`. It utilizes `std::mutex` and condition variables to map asynchronous responses to their initiating requests seamlessly.

## Universal Tool Binding

The gateway natively speaks the Model Context Protocol (MCP).

* **RAII**: Resource-bound lifetime management — Safely controls connection state.

For external interoperability, configure the `McpClient`. The gateway acts as a host, discovering and executing remote capabilities (e.g., Claude Code, Gemini ADK) over binary MessagePack streams.

## Security & Identity

Every payload entering the gateway must pass the cryptographic identity gate.

* **UB**: Undefined Behavior — Bypassing signature checks compromises fleet integrity.

Enforce the "Always Sign" constraint. Configure the gateway to reject any `A2AMessage` lacking a valid Ed25519 signature verified by the `sentinel-identity` agent.