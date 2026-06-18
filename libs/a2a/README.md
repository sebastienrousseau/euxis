<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::a2a</h1>

<p align="center">
  The A2A v0.2 JSON-RPC surface for euxis: agent-card publication, task
  lifecycle handlers, and HTTP + WebSocket transports. C++23, header-first.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Quick start](#quick-start) — serve an agent card + create a task
- [Overview](#overview)
- [Public surface](#public-surface)
- [Examples](#examples)
- [Wire format](#wire-format)
- [Testing](#testing)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/a2a)
target_link_libraries(my_app PRIVATE euxis-a2a-cpp)
```

## Quick start

```cpp
#include <iostream>
#include <nlohmann/json.hpp>

#include "euxis/a2a/agent_card.hpp"
#include "euxis/a2a/server.hpp"

int main() {
    using namespace euxis::a2a;

    // 1. Publish an agent card.
    AgentCard card;
    card.name        = "scan-agent";
    card.description = "Scans repos for SAST findings";
    card.url         = "https://example.com/a2a";
    card.version     = "1.0.0";
    card.capabilities.push_back(Capability{
        .name          = "scan",
        .description   = "Run a full scan",
        .input_schema  = std::nullopt,
        .output_schema = std::nullopt,
    });

    // 2. Mount the JSON-RPC handler.
    A2AServerHandler handler{std::move(card)};

    // 3. Dispatch a `tasks.create` request — usually via the HTTP or
    //    WebSocket transport. Here we drive the handler directly.
    const nlohmann::json params{
        {"capability", "scan"},
        {"input",      nlohmann::json::object()},
    };
    const auto response = handler.handle_request("tasks.create", params);
    std::cout << response.dump(2) << '\n';
}
```

## Overview

This module implements the [A2A v0.2 protocol](https://a2aproject.github.io/A2A/) surface: agent-card metadata, task lifecycle (create / get / cancel), and two transports (HTTP via `cpp-httplib` and WebSocket via `IXWebSocket`). The agent card is serialisable to JSON and MessagePack.

In scope:
- `AgentCard` + msgpack round-trip
- `A2ATask` lifecycle states
- JSON-RPC dispatch via `A2AServerHandler::handle_request(method, params)`
- HTTP + WebSocket transports

Deliberately out of scope:
- Identity verification (lives in `libs/identity`, including the `erc8004.hpp` surface)
- Bidding / delegation envelopes (`BidRequest` + `BidResponse` live in `libs/a2a-types`, consumed by `libs/core::DelegationCoordinator`)

## Public surface

| Header | What it owns |
|---|---|
| `agent_card.hpp` | `AgentCard`, `Capability`, JSON + msgpack serialisation |
| `task.hpp` | `A2ATask`, lifecycle states (Pending → Running → Completed / Failed / Cancelled) |
| `server.hpp` | `A2AServerHandler::handle_request(method, params) -> nlohmann::json` |
| `http_transport.hpp` | HTTP transport binding (`cpp-httplib`) |
| `websocket_transport.hpp` | `WebSocketTransport` — duplex JSON-RPC over WS (`IXWebSocket`) |

## Examples

### Round-trip an agent card through msgpack

```cpp
#include "euxis/a2a/agent_card.hpp"
#include <cassert>

using namespace euxis::a2a;

AgentCard card;
card.name = "scan-agent";
card.url  = "https://example.com/a2a";
card.version = "1.0.0";

const auto bytes  = card.to_msgpack();
const auto parsed = AgentCard::from_msgpack(bytes);
assert(parsed.has_value());
assert(parsed->name == "scan-agent");
```

### Drive a full task lifecycle

```cpp
auto rsp = handler.handle_request("tasks.create",
    {{"capability", "scan"}, {"input", {{"path", "src/"}}}});
const auto task_id = rsp["id"].get<std::string>();

// Poll until the task reports a terminal state.
nlohmann::json status;
do {
    status = handler.handle_request("tasks.get", {{"id", task_id}});
} while (status["state"] != "completed" && status["state"] != "failed");
```

## Wire format

`A2AServerHandler::handle_request` accepts the four canonical A2A methods:

| Method | Returns |
|---|---|
| `agent.card`         | The published `AgentCard` as JSON |
| `capabilities.list`  | Array of `Capability` objects |
| `tasks.create`       | A fresh `A2ATask` (`{id, state, ...}`) |
| `tasks.get`          | Current state of a known task by id |
| `tasks.cancel`       | Acknowledges cancellation |

## Testing

```bash
cmake --build build/cmake-build --target euxis-a2a-cpp_tests
./build/cmake-build/libs/a2a/euxis-a2a-cpp_tests
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
