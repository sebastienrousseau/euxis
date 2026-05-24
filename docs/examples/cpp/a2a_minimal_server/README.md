# Minimal A2A Server Example

This example shows the smallest end-to-end use of the Euxis A2A v0.2 SDK:
construct an agent card, validate it, then drive the server handler through
a full task lifecycle.

## What it demonstrates

| Step | API surface |
|------|-------------|
| Build an agent card | `euxis::a2a::AgentCard`, `Capability` |
| Validate the card | `validate_card()` returning `std::expected<void, std::string>` |
| Discovery | `A2AServerHandler::handle_request("agent/card", ...)` |
| List capabilities | `handle_request("capabilities/list", ...)` |
| Create a task | `handle_request("task/create", {{"message", "..."}})` |
| Poll task state | `handle_request("task/get", {{"id", ...}})` |
| Cancel a task | `handle_request("task/cancel", {{"id", ...}})` |

The example dispatches against `A2AServerHandler` directly so it stays
transport-agnostic. For HTTP/WebSocket serving see
`libs/a2a/include/euxis/a2a/http_transport.hpp` and
`libs/a2a/include/euxis/a2a/websocket_transport.hpp`.

## Build

```bash
cmake -B cmake-build -DEUXIS_BUILD_EXAMPLES=ON
cmake --build cmake-build --target euxis_example_a2a_minimal_server
```

## Run

```bash
./cmake-build/docs/examples/cpp/a2a_minimal_server/euxis_example_a2a_minimal_server
```

## Expected output (excerpt)

```text
[agent/card]
{
  "result": {
    "name": "minimal-example-agent",
    "url": "http://localhost:9090",
    "version": "0.2.0",
    ...
  }
}

[task/create]
{
  "result": {
    "id": "...",
    "status": "pending",
    ...
  }
}

[task/cancel]
{
  "result": {
    "id": "...",
    "status": "cancelled",
    "history": ["pending"],
    ...
  }
}

Example completed successfully.
```

## Where to go from here

- `libs/a2a/tests/test_server.cpp` — full JSON-RPC error-path coverage
- `libs/a2a/tests/test_http_transport.cpp` — HTTP serving wired to a handler
- `libs/runtime/tests/test_agent_session.cpp` — tool registration + dispatch
