<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">euxis::network</h1>

<p align="center">
  Network primitives for euxis: ACP control-plane state machine,
  MCP client, WebSocket helper, topology grid, and resilience patterns
  (CircuitBreaker, RetryPolicy). C++23, header-first.
</p>

<p align="center">
  <a href="https://github.com/sebastienrousseau/euxis/actions/workflows/cpp.yml"><img src="https://img.shields.io/github/actions/workflow/status/sebastienrousseau/euxis/cpp.yml?style=for-the-badge&logo=github" alt="Build" /></a>
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue?style=for-the-badge&logo=cplusplus" alt="C++23" />
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=for-the-badge" alt="License" /></a>
</p>

---

## Contents

- [Install](#install)
- [Quick start](#quick-start) — drive an ACP session through its lifecycle
- [Public surface](#public-surface)
- [Examples](#examples)
- [License](#license)

---

## Install

```cmake
add_subdirectory(libs/network)
target_link_libraries(my_app PRIVATE euxis-network-cpp)
```

## Quick start

```cpp
#include <iostream>

#include "euxis/network/acp.hpp"

int main() {
    using namespace euxis::network;

    AcpPolicy policy;
    policy.allow("scan-agent");

    AcpSessionManager mgr{policy};

    SessionSpec spec;
    spec.agent_id  = "scan-agent";
    spec.session_id = "s-001";

    const auto s = mgr.spawn(spec);                     // Pending
    mgr.set_state("s-001", SessionState::Running);
    mgr.set_state("s-001", SessionState::Completed);

    for (const auto& ev : mgr.ledger().events_for("s-001")) {
        std::cout << static_cast<int>(ev.kind) << '\n';
    }
}
```

## Public surface

| Header | What it owns |
|---|---|
| `acp.hpp` | `SessionState` enum (Pending → Running → Paused / Completed / Failed / Cancelled), `SessionSpec`, `AcpSession`, `AcpEvent`, `AcpEventLedger` (thread-safe append-only audit log), `AcpPolicy` (allowlist), `AcpSessionManager` |
| `mcp_client.hpp` | `McpClient` — Model Context Protocol client for invoking remote tools over JSON-RPC |
| `ws_client.hpp` | `WebSocketClient` — thin async WS wrapper over IXWebSocket |
| `topology_grid.hpp` | `TopologyGrid`, `NodeInfo` — flat-array grid of mesh nodes with O(1) lookup |
| `resilience.hpp` | `RetryPolicy`, `CircuitBreaker` — re-exported from `euxis::core::contracts` for convenience |

## Examples

### Bound a remote call with `CircuitBreaker`

```cpp
#include "euxis/network/resilience.hpp"

using namespace euxis::network;

CircuitBreaker cb{/*failure_threshold=*/5, /*reset_after=*/std::chrono::seconds{30}};

if (cb.should_attempt()) {
    auto rc = remote_call();
    if (rc) { cb.record_success(); }
    else    { cb.record_failure(); }
}
```

### Stream MCP tool invocations

```cpp
#include "euxis/network/mcp_client.hpp"

McpClient client{"https://mcp.example.com"};
const auto tools = client.list_tools();                 // returns the remote tool registry
const auto out   = client.call_tool("scan", {{"path", "src/"}});
```

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
