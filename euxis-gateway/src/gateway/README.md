# Gateway

The Gateway is the WebSocket control plane for sessions, routing, and streaming events.

## Purpose
Gateway owns connection handling, session routing, approvals, and event streaming. It delegates execution to existing CLI tools and does not embed agent logic.

## Structure
- `server.py` — Gateway runtime (WS + HTTP)
- `utils.py` — Persistence helpers and audit logging
- `webchat/` — WebChat UI assets
- `demo.py` — Minimal demo client
- `smoke_test.py` — Health check tester

## Dependencies
- `core/` — shared utilities
- `config/` — gateway schemas and defaults
- `adapters/src/adapters/` — channel adapters
- `security/` — approval and allowlist policy defaults

## Usage
```bash
python3 api/src/gateway/server.py --bind 127.0.0.1 --port 18789
```

Default policy config:
`~/.euxis/security/gateway.json`

## Development
```bash
# Run smoke test
python3 api/src/gateway/smoke_test.py --url http://127.0.0.1:18789/health
```

## Testing
Gateway protocol tests live in `api/src/gateway/protocol_test.py` and integration tests in `tests/`.

## API / Exports
Gateway exposes the WebSocket protocol documented in `docs/reference/gateway.md`.
