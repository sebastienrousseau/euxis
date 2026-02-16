# (c) 2026 Euxis Fleet. All rights reserved.
# Gateway CLI

Reference for the `euxis-gateway` command family.

## Synopsis

```bash
euxis-gateway <command> [options]
```

## Commands

### run

Start the Gateway control plane.

```bash
euxis-gateway run
```

Options:
- `--bind <addr>`: bind address (default: `127.0.0.1`)
- `--port <port>`: bind port (default: `18789`)
- `--config <path>`: config path (default: `~/.euxis/config/gateway.json`)
- `--auth-mode <token|password>`: override auth mode

### status

Show Gateway status.

```bash
euxis-gateway status
```

### config

Print the resolved Gateway configuration.

```bash
euxis-gateway config
```

### sessions

List active sessions.

```bash
euxis-gateway sessions
```

## Config Loading Order

1. `--config <path>` CLI override
2. `EUXIS_GATEWAY_CONFIG` environment override
3. Default: `~/.euxis/config/gateway.json`
4. Built-in defaults if file is missing

## Related

- Gateway Protocol: `docs/reference/gateway.md`
- Gateway Auth: `docs/reference/gateway-auth.md`
- Gateway Config: `docs/reference/gateway-config.md`
