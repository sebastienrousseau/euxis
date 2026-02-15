# (c) 2026 Euxis Fleet. All rights reserved.
# Gateway Config

Reference for the Gateway configuration file.

## Example

```json
{
  "gateway": {
    "bind": "127.0.0.1",
    "port": 18789,
    "health_enabled": true,
    "health_path": "/health",
    "auth": {
      "mode": "token",
      "token": {
        "value": "replace-with-secure-random-value"
      }
    }
  }
}
```

## Fields

**File location (proposed):** `~/.euxis/config/gateway.json`

### `gateway`

- `bind` (string): interface to bind. Default: `127.0.0.1`
- `port` (int): port for WS + HTTP. Default: `18789`
- `health_enabled` (bool): expose health endpoint. Default: `true`
- `health_path` (string): path for health endpoint. Default: `/health`

### `gateway.auth`

- `mode` (string): `token` or `password`. Default: `token`

### `gateway.auth.token`

- `value` (string): shared token for Authorization header.

### `gateway.auth.password`

- `value` (string): password for basic auth (if enabled).

## Notes

- If `mode = token`, clients should send `Authorization: Bearer <token>`.
- If `mode = password`, clients should send `Authorization: Basic <base64>`.
