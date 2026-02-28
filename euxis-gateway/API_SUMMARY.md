# API Summary (api)

This summary is auto-generated from package layout and public __init__ modules.

## Packages
- `gateway`

## Notes
- This summary does not include dynamic exports or runtime plugin loading.
- Public APIs should be exposed via package `__init__.py` and documented here.

## 2026 Core Protocols

### Model Context Protocol (MCP) Host
- **Endpoint**: `ws://<bind>:<port>/mcp`
- **Description**: Exposes Euxis tools to standard MCP clients (like Wasm Agents, Claude Desktop, or external Swarms).
- **Tools**:
  - `get_metrics`: Returns uptime, active sessions, and mesh health.
  - `sign_payload`: Signs text using Euxis Cryptographic Provenance.

### Computer Using Agent (CUA) Device Nodes
- **Websocket Events**: `node.register`, `node.shell`, `node.browser.open`, `node.browser.screenshot`, `node.browser.stream_start`, `node.browser.stream_stop`.
- **Description**: Secure tunnel connecting sandboxed Euxis agents in the cloud directly to local hardware (via `euxis-node` daemon) to execute CLI commands and control headless Chrome with Playwright.

### Identity-First Governance & HITL
- **Approvals Endpoints**:
  - `GET /approvals`: List pending HITL agent actions.
  - `POST /approvals/{run_id}/approve`: Approve a restricted action.
  - `POST /approvals/{run_id}/reject`: Reject a restricted action.
- **Identity Manager**: Defines explicit scopes (`restricted`, `trusted`, `elevated`) managed internally via `identities.json`.