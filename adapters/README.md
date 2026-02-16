# Adapters

Channel adapters that connect external platforms to the Gateway.

## Purpose
Adapters translate platform-specific events into Gateway messages. Each adapter is independent and should not import from other adapters.

## Structure
- `registry.py` — Adapter registry and config wiring
- `sdk.py` — Shared adapter interface
- `slack_adapter.py` — Slack integration
- `telegram_adapter.py` — Telegram integration

## Dependencies
- `core/` — shared utilities
- `gateway/` — session and message routing APIs

## Usage
Adapters are loaded by the Gateway based on `config/gateway.json`.

## Development
```bash
# Run the gateway to exercise adapters
python3 gateway/server.py --bind 127.0.0.1 --port 18789
```

## Testing
Adapter behavior is covered by Gateway integration tests in `tests/`.

## API / Exports
Adapters export a `connect`, `receive`, `send`, and `disconnect` contract via `sdk.py`.
