<!-- SPDX-License-Identifier: AGPL-3.0-only -->

<p align="center">
  <img src="https://cloudcdn.pro/euxis/v1/logos/euxis.svg" alt="Euxis logo" width="128" />
</p>

<h1 align="center">apps/gateway — <code>euxis-gateway</code></h1>

<p align="center">
  HTTP + WebSocket gateway for euxis. Hosts the A2A v0.2 surface,
  exposes the ACP control plane, bridges MCP, and routes webhooks
  into the agent fleet.
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
- [Route map](#route-map)
- [Authentication](#authentication)
- [License](#license)

---

## Install

```bash
cmake -B build/cmake-build
cmake --build build/cmake-build --target euxis-gateway
./build/cmake-build/apps/gateway/euxis-gateway --config gateway.yaml
```

## Quick start

```bash
# Health probe
curl -s http://localhost:8088/health

# List sessions (requires a valid Ed25519 signature on the Authorization header)
curl -s http://localhost:8088/v1/sessions

# Open an ACP session over WebSocket
websocat ws://localhost:8088/v1/ws
```

## Route map

| Source | What it serves |
|---|---|
| `routes_health.cpp` | `GET /health`, `GET /ready`, `GET /metrics` |
| `routes_admin.cpp`  | Admin plane — policy + fleet config inspection |
| `routes_mcp.cpp`    | MCP host endpoints — tool discovery + invocation |
| `routes_sessions.cpp` | A2A session lifecycle (`tasks.create`, `tasks.get`, `tasks.cancel`) |
| `routes_webhooks.cpp` | Inbound webhooks from `libs/adapters` channel integrations |
| `websocket.cpp`     | Duplex JSON-RPC bus for ACP control-plane events |
| `mcp_stdio.cpp`     | Stdio MCP transport (used when launched as a child of an IDE) |
| `mcp_fleet_tools.cpp` | Exposes the agent fleet as MCP tools to external orchestrators |

The route handlers consume `libs/a2a`, `libs/network` (`AcpSessionManager`, `McpClient`), and `libs/core` (`Router`, `DelegationCoordinator`) directly. The duplex bus is one `WebSocketTransport` from `libs/a2a` plus the ACP ledger from `libs/network`.

## Authentication

`auth.cpp` enforces Ed25519 signature checks (via `libs/crypto`) on every mutating request. The signing key is bound to a `did:euxis:*` identity in `libs/identity`; the gateway rejects requests whose key isn't registered in the local `IdentityRegistry`. Read endpoints (`GET /health`, `GET /ready`) are unauthenticated by design so probes work without credentials.

## License

AGPL-3.0-only. See [`LICENSE`](../../LICENSE).
