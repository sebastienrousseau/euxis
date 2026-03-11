# euxis-a2a-cpp

C++23 A2A v0.2 protocol implementation — agent cards, task management, JSON-RPC server, HTTP transport.

## Overview

euxis-a2a-cpp implements the Google A2A v0.2 protocol for agent-to-agent communication. It provides AgentCard construction with camelCase JSON serialization, A2ATask management with state machine enforcement (submitted, working, input-required, completed, canceled, failed), strongly-typed A2AMessage and Artifact types, an A2AServerHandler for JSON-RPC method dispatch, and an HttpA2ATransport that discovers remote agents via /.well-known/agent.json.

## Dependencies

- euxis-identity-cpp
- nlohmann-json
- spdlog
- cpp-httplib

## Building

```bash
# From the euxis-cpp root
cmake -B build -S .
cmake --build build --target euxis-a2a-cpp
```

## Testing

```bash
ctest --test-dir build -R euxis-a2a-cpp_tests
```

## API

- **agent_card.hpp** -- AgentCard definition with camelCase JSON serialization and capability declaration.
- **task.hpp** -- A2ATask with state machine enforcement and history tracking.
- **message.hpp** -- A2AMessage and Artifact types for task communication.
- **transport.hpp** -- Transport interface for sending and receiving A2A messages.
- **server.hpp** -- A2AServerHandler: JSON-RPC dispatch for tasks/send, tasks/get, tasks/cancel.
- **http_transport.hpp** -- HttpA2ATransport: HTTP client with /.well-known/agent.json discovery.
