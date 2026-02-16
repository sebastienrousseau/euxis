# (c) 2026 Euxis Fleet. All rights reserved.
# Gateway Auth

Guidance for authenticating against the Euxis Gateway in v0.1.

## Default Mode

The Gateway defaults to `token` authentication.

## Token Auth

Token auth expects a shared secret token provided by the Gateway configuration.

### Client Usage

Send the token as a WebSocket header:

```text
Authorization: Bearer <token>
```

HTTP endpoints (health, sessions, approvals, automation, canvas, voice) use the same Authorization header.

### WebChat Usage

If `allow_query_param` is enabled, you can pass the token via query param:

```
http://127.0.0.1:18789/webchat/?token=<token>
```

### Example (Python)

```python
import asyncio
import websockets

async def main():
    uri = "ws://127.0.0.1:18789"
    headers = {"Authorization": "Bearer YOUR_TOKEN"}
    async with websockets.connect(uri, extra_headers=headers) as ws:
        await ws.send("{\"type\":\"request\",\"id\":\"req_1\",\"method\":\"chat.history\",\"params\":{\"session_id\":\"sess_webchat_abc123\"}}")
        print(await ws.recv())

asyncio.run(main())
```

### Example (wscat)

```bash
wscat -H "Authorization: Bearer YOUR_TOKEN" -c ws://127.0.0.1:18789
```

## Password Auth

If `gateway.auth.mode` is set to `password`, clients should send a Basic auth header:

```text
Authorization: Basic <base64(username:password)>
```

## Failure Modes

- Missing token: return an auth error and close the connection.
- Invalid token: return an auth error and close the connection.

## Rotation Guidance

- Tokens should be rotated periodically.
- On rotation, clients must reconnect using the new token.

## Storage Guidance

- Store tokens in environment variables or a dedicated secrets manager.
- Avoid hardcoding tokens in source control or config files checked into git.
- Use per-environment tokens (dev, staging, prod) to limit blast radius.

## Config Example

```json
{
  "gateway": {
    "auth": {
      "mode": "token",
      "token": {
        "value": "replace-with-secure-random-value"
      }
    }
  }
}
```

## Token Generation

Generate a secure token locally:

```bash
openssl rand -hex 32
```

## Minimum Requirements

- Minimum token length: 32 bytes (64 hex characters).
- Rotate tokens at least quarterly or after any suspected exposure.
