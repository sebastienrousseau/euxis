<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::a2a (types)</h1>

<p align="center">
  Shared wire types for the A2A v0.2 protocol surface. Lives in its own
  module so consumers can depend on the data shapes without pulling in
  the HTTP / WebSocket transports from <code>libs/a2a</code>.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Quick start](#quick-start)
- [Public surface](#public-surface)
- [Examples](#examples)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/a2a-types)
target_link_libraries(my_app PRIVATE euxis-a2a-types-cpp)
```

## Quick start

```cpp
#include <iostream>

#include "euxis/a2a/message.hpp"

int main() {
    using namespace euxis::a2a;

    A2AMessage msg;
    msg.message_id = "m-001";
    msg.task_id    = "t-001";
    msg.role       = "user";
    msg.parts.push_back(ContentPart{
        .type = "text",
        .text = "Please scan the auth module.",
    });
    std::cout << "message ready: " << msg.parts.size() << " parts\n";
}
```

## Overview

This module contains *only* the value types and the abstract transport interface for the A2A protocol. There are no I/O paths here — the HTTP and WebSocket bindings live in `libs/a2a`. Splitting the types out keeps test fixtures, mocks, and downstream embedders from dragging `cpp-httplib` and `IXWebSocket` into their dependency graph.

## Public surface

| Header | What it owns |
|---|---|
| `message.hpp` | `A2AMessage`, `ContentPart`, `Artifact`, `BidRequest`, `BidResponse` |
| `transport.hpp` | `ITransport` (abstract), `TransportError` enum |

## Examples

### Implement a mock `ITransport` for unit tests

```cpp
#include "euxis/a2a/transport.hpp"

class EchoTransport : public euxis::a2a::ITransport {
public:
    auto send(const euxis::a2a::A2AMessage& msg)
        -> std::expected<euxis::a2a::A2AMessage, euxis::a2a::TransportError> override {
        return msg;                                     // echo back
    }
};
```

### Build a delegation bid envelope

```cpp
euxis::a2a::BidRequest req;
req.task_id     = "scan-002";
req.required_capability = "scan";
req.instruction = "Scan src/ for taint flows reaching network sinks.";
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
