# (c) 2026 Euxis Fleet. All rights reserved.
# Gateway Protocol

Reference for the Euxis Gateway WebSocket protocol. This document defines the frame envelope, supported methods, event types, and validation rules used by the control plane.

## Schemas

Schemas live in `docs/reference/gateway/`:
- `docs/reference/gateway/gateway-request.schema.json`
- `docs/reference/gateway/gateway-response.schema.json`
- `docs/reference/gateway/gateway-event.schema.json`
- `docs/reference/gateway/gateway-frame.schema.json`

## Frame Envelopes

All frames are JSON objects with a top-level `type` field.

### Request

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

### Response

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

### Error Response

```json
{
  "type": "response",
  "id": "req_01HZX0M0T1Y5GZ9P5E9YB6KXG4",
  "ok": false,
  "error": {
    "code": "INVALID_REQUEST",
    "message": "Missing session_id",
    "retryable": false
  }
}
```

### Event

```json
{
  "type": "event",
  "event": "agent",
  "seq": 81,
  "ts": "2026-02-15T19:45:12.118Z",
  "data": {
    "run_id": "run_01HZX0NAN9ZK4D8FMQ8J8C1K0E",
    "session_id": "sess_webchat_abc123",
    "agent_id": "architect",
    "status": "stream",
    "content": "Design outline: ..."
  }
}
```

## Methods

Supported `method` values for request frames:
- `gateway.connect`
- `chat.history`
- `chat.send`
- `chat.abort`
- `chat.inject`

## Auth

Gateway connections require an `Authorization` header when auth is enabled.
See `docs/reference/gateway-auth.md` for token and password examples.

Example header:

```text
Authorization: Bearer <token>
```

### gateway.connect

Params:
- `protocol` (required, string)
- `client_id` (optional)
- `resume_seq` (optional, integer)

Result:
- `protocol` (string)
- `server_time` (ISO timestamp)
- `stateVersion` (integer)
- `sessions_active` (integer)

### chat.history

Params:
- `session_id` (required)
- `limit` (optional, 1..1000)

### chat.send

Params:
- `session_id` (required)
- `channel_id` (required)
- `role` (required, `user|assistant|system`)
- `content` (required)
- `meta` (optional)

### chat.abort

Params:
- `run_id` (required)
- `session_id` (required)

### chat.inject

Params:
- `session_id` (required)
- `role` (required, `assistant|system`)
- `content` (required)

## Events

Supported `event` values:
- `agent`
- `presence`
- `tick`
- `shutdown`

### agent

Data fields:
- `run_id`
- `session_id`
- `agent_id`
- `status` (`stream|final|error|aborted`)
- `content`

### presence

Data fields:
- `stateVersion`
- `sessions_active`

### tick

Data fields:
- `uptime_ms`
- `stateVersion`

### shutdown

Data fields:
- `reason`
- `message`

## Validation Rules (v0.1)

- Reject unknown `type` values.
- Reject unknown `method` values.
- Require `protocol` for `gateway.connect`.
- Require `session_id` for all chat methods.
- Require `run_id` for `chat.abort`.
- Require `role` for `chat.send` and `chat.inject`.

## Test Harness

Run the protocol validator:

```bash
python3 scripts/gateway_protocol_test.py
```

Optional validator deps:

```bash
pip install jsonschema referencing
```

## Smoke Test

Run a health check against a running gateway:

```bash
python3 scripts/gateway_smoke_test.py --url http://127.0.0.1:18789/health
```

## Demo Client

Run a minimal WebSocket client:

```bash
python3 scripts/gateway_demo.py --url ws://127.0.0.1:18789 --session sess_demo
```

## WebChat UI

Start the gateway and open:

```
http://127.0.0.1:18789/webchat/
```

## Session APIs

List sessions:

```
http://127.0.0.1:18789/sessions
```

## Automation APIs

Create a cron job:

```
POST /automation/cron
{
  "job_id": "cron_morning",
  "every_seconds": 3600,
  "session_id": "sess_ops",
  "content": "Run hourly status check",
  "meta": { "agent": "responder", "approved": true }
}
```

List cron jobs:

```
GET /automation/cron
```

Delete a cron job:

```
DELETE /automation/cron/{job_id}
```

## Canvas APIs

Canvas endpoints are available when `gateway.canvas.enabled=true`.

Store canvas state:

```
POST /canvas/{session_id}
{
  "state": { "view": "dashboard", "widgets": [] }
}
```

Fetch canvas state:

```
GET /canvas/{session_id}
```

## Voice APIs

Voice endpoints are available when `gateway.voice.enabled=true`.

Wake the voice pipeline:

```
POST /voice/wake
{
  "session_id": "sess_voice",
  "content": "Start daily standup",
  "meta": { "agent": "orchestrator" }
}
```

Upload voice audio:

```
POST /voice/upload?session_id=sess_voice
Content-Type: multipart/form-data
file=@audio.wav
```

## Webhook APIs

Webhook updates are persisted to `~/.euxis/data/gateway/webhooks.json`.

List webhooks:

```
GET /webhooks
```

Update webhooks:

```
POST /webhooks
{
  "webhooks": [
    { "url": "https://example.com/hooks/gateway", "events": ["agent.final"] }
  ]
}
```

Fetch a session:

```
http://127.0.0.1:18789/sessions/<session_id>
```

Export sessions:

```
http://127.0.0.1:18789/sessions/export
```

Import sessions (POST JSON body):

```
http://127.0.0.1:18789/sessions/import
```

## Run APIs

List runs:

```
http://127.0.0.1:18789/runs
```

Fetch a run:

```
http://127.0.0.1:18789/runs/<run_id>
```

## Approvals APIs

List approvals:

```
http://127.0.0.1:18789/approvals
```

Approve:

```
http://127.0.0.1:18789/approvals/<run_id>/approve
```

Reject:

```
http://127.0.0.1:18789/approvals/<run_id>/reject
```

## Session Broadcast

Post a message to all active connections in a session:

```
http://127.0.0.1:18789/sessions/<session_id>/broadcast
```

## Elevated Mode

Set `meta.elevated` to `full` in `chat.send` to request elevated execution. This is governed by `gateway.exec.elevated`.

Validate custom frames (JSON array or JSONL):

```bash
python3 scripts/gateway_protocol_test.py --frames /path/to/frames.jsonl
```
