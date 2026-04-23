# Gateway Adapters

Design notes for channel adapters that connect external messaging platforms to the Euxis Gateway.
This is an initial draft focused on Slack and Telegram.

## Goals

- Normalize inbound messages into Gateway sessions.
- Preserve deterministic reply-back to the originating channel.
- Keep adapters thin; all routing and policy decisions live in the Gateway.

## Common Adapter Contract

Each adapter exposes a minimal interface:

- `connect()`
- `receive(message)`
- `send(message, session_id)`
- `ack(message_id)`
- `disconnect()`

## Slack Adapter

### Auth

- Bot token via OAuth (recommended for workspace installs).
- Support Socket Mode for environments without inbound HTTP.

### Flow (Mermaid)

```text
sequenceDiagram
    participant Slack
    participant Adapter
    participant Gateway
    participant Agent

    Slack->>Adapter: event (message)
    Adapter->>Gateway: chat.send (session_id)
    Gateway->>Agent: dispatch
    Agent-->>Gateway: response stream
    Gateway-->>Adapter: agent event
    Adapter-->>Slack: postMessage (thread_ts)
```

### Inbound Mapping

- `event.user` + `event.channel` -> `session_id`
- `thread_ts` -> thread-scoped session
- `channel_type` -> `scope` (dm|group)

### Outbound Mapping

- Reply to `channel` and `thread_ts` when present.
- Maintain `message_id` mapping to Slack `ts` for updates.

### Limits

- Respect rate limits and retry with backoff.
- Chunk long responses into multiple messages.

## Telegram Adapter

### Auth

- Bot token via BotFather.
- Webhook mode preferred; long polling fallback.

### Flow (Mermaid)

```text
sequenceDiagram
    participant Telegram
    participant Adapter
    participant Gateway
    participant Agent

    Telegram->>Adapter: update (message)
    Adapter->>Gateway: chat.send (session_id)
    Gateway->>Agent: dispatch
    Agent-->>Gateway: response stream
    Gateway-->>Adapter: agent event
    Adapter-->>Telegram: sendMessage (message_thread_id)
```

### Inbound Mapping

- `chat.id` + `from.id` -> `session_id`
- `message_thread_id` -> topic-scoped session
- `chat.type` -> `scope` (private|group|supergroup)

### Outbound Mapping

- Reply via `sendMessage` with `chat_id` and optional `message_thread_id`.
- Track `message_id` for edits and deletes.

### Limits

- Honor Telegram rate limits and retry guidance.
- Split long responses and preserve ordering.

## Config Sketch

```json
{
  "gateway": {
    "channels": {
      "slack": {
        "enabled": false,
        "mode": "socket",
        "token": "xoxb-...",
        "app_token": "xapp-...",
        "signing_secret": "",
        "events_path": "/channels/slack/events"
      },
      "telegram": {
        "enabled": false,
        "mode": "webhook",
        "token": "123456:ABC...",
        "webhook_url": "https://example.com/telegram/webhook",
        "webhook_path": "/channels/telegram/webhook",
        "poll_timeout": 20,
        "poll_interval": 1.5
      }
    }
  }
}
```

## Next Steps

- Define session key format (`channel_id` + `chat_id` + `thread`).
- Add allowlist and admin controls per adapter.
- Specify media handling and file upload support.
