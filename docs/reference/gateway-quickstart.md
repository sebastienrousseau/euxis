# Gateway Quickstart

A minimal end-to-end example for talking to the Euxis Gateway over WebSocket.

## 1) Connect

The Gateway listens on `127.0.0.1:18789` in v0.1.3.
Token auth is enabled by default, so clients should include an `Authorization` header.
See `docs/reference/gateway-auth.md` for details.

### Client Example (Python)

```python
import asyncio
import json
import websockets

async def main():
    uri = "ws://127.0.0.1:18789"
    headers = {"Authorization": "Bearer YOUR_TOKEN"}
    async with websockets.connect(uri, extra_headers=headers) as ws:
        await ws.send(json.dumps({
            "type": "request",
            "id": "req_connect_1",
            "method": "gateway.connect",
            "params": {
                "protocol": "v0.1.3",
                "client_id": "quickstart"
            }
        }))
        await ws.send(json.dumps({
            "type": "request",
            "id": "req_01HZX0M0T1Y5GZ9P5E9YB6KXG4",
            "method": "chat.send",
            "params": {
                "session_id": "sess_webchat_abc123",
                "channel_id": "webchat",
                "role": "user",
                "content": "Design a caching layer",
                "meta": {"agent": "architect", "priority": "P1"}
            }
        }))
        async for message in ws:
            print(message)

asyncio.run(main())
```

### Client Example (wscat)

```bash
# Install once (Node.js required)
npm install -g wscat

# Connect
wscat -H "Authorization: Bearer YOUR_TOKEN" -c ws://127.0.0.1:18789
```

Then paste a request frame:

```json
{
  "type": "request",
  "id": "req_connect_1",
  "method": "gateway.connect",
  "params": {
    "protocol": "v0.1.3",
    "client_id": "wscat"
  }
}
```

Then paste a chat request frame:

```json
{
  "type": "request",
  "id": "req_01HZX0M0T1Y5GZ9P5E9YB6KXG4",
  "method": "chat.send",
  "params": {
    "session_id": "sess_webchat_abc123",
    "channel_id": "webchat",
    "role": "user",
    "content": "Design a caching layer",
    "meta": {
      "agent": "architect",
      "priority": "P1"
    }
  }
}
```

### Client Example (websocat)

```bash
# Install (Rust + cargo required)
cargo install websocat

# Connect
websocat -H="Authorization: Bearer YOUR_TOKEN" ws://127.0.0.1:18789
```

Then paste a request frame (same as above).

## 2) Send a Message

Example request frame:

```json
{
  "type": "request",
  "id": "req_01HZX0M0T1Y5GZ9P5E9YB6KXG4",
  "method": "chat.send",
  "params": {
    "session_id": "sess_webchat_abc123",
    "channel_id": "webchat",
    "role": "user",
    "content": "Design a caching layer",
    "meta": {
      "agent": "architect",
      "priority": "P1"
    }
  }
}
```

Example response frame:

```json
{
  "type": "response",
  "id": "req_01HZX0M0T1Y5GZ9P5E9YB6KXG4",
  "ok": true,
  "result": {
    "message_id": "msg_01HZX0M6BQ9Z3P7J8R2H8J6KCN",
    "session_id": "sess_webchat_abc123",
    "run_id": "run_01HZX0NAN9ZK4D8FMQ8J8C1K0E"
  }
}
```

Example streaming event:

```json
{
  "type": "event",
  "event": "agent",
  "seq": 1,
  "ts": "current-02-15T19:45:12.118Z",
  "data": {
    "run_id": "run_01HZX0NAN9ZK4D8FMQ8J8C1K0E",
    "session_id": "sess_webchat_abc123",
    "agent_id": "architect",
    "status": "stream",
    "content": "Design outline: ..."
  }
}
```

## Health Check (Optional)

If the Gateway exposes a health endpoint:

```bash
curl http://127.0.0.1:18789/health
```

Expected shape (example):

```json
{
  "status": "ok",
  "sessions_active": 0,
  "uptime_ms": 120003
}
```

## 3) Fetch History

```json
{
  "type": "request",
  "id": "req_01HZX0M0T1Y5GZ9P5E9YB6KXG5",
  "method": "chat.history",
  "params": {
    "session_id": "sess_webchat_abc123",
    "limit": 50
  }
}
```

## 4) Abort a Run

```json
{
  "type": "request",
  "id": "req_01HZX0M0T1Y5GZ9P5E9YB6KXG6",
  "method": "chat.abort",
  "params": {
    "run_id": "run_01HZX0NAN9ZK4D8FMQ8J8C1K0E",
    "session_id": "sess_webchat_abc123"
  }
}
```

## 5) Inject a Note

```json
{
  "type": "request",
  "id": "req_01HZX0M0T1Y5GZ9P5E9YB6KXG7",
  "method": "chat.inject",
  "params": {
    "session_id": "sess_webchat_abc123",
    "role": "system",
    "content": "Note: session escalated to architect."
  }
}
```

## Related

- Gateway Protocol: `docs/reference/gateway.md`
- Schema files: `docs/reference/gateway/`
