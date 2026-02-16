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
        "value": "replace-with-secure-random-value",
        "allow_query_param": true
      }
    },
    "exec": {
      "policy": "allowlist",
      "ask": "on-miss",
      "ask_fallback": "deny",
      "elevated": "ask",
      "allowlist": [
        "architect",
        "debugger",
        "reviewer"
      ]
    },
    "channels": {
      "slack": {
        "enabled": false,
        "mode": "socket",
        "token": "",
        "app_token": "",
        "signing_secret": "",
        "events_path": "/channels/slack/events",
        "max_length": 3500
      },
      "telegram": {
        "enabled": false,
        "mode": "webhook",
        "token": "",
        "webhook_url": "",
        "webhook_path": "/channels/telegram/webhook",
        "poll_timeout": 20,
        "poll_interval": 1.5,
        "max_length": 4000
      }
    },
    "policy": {
      "mention_required": true,
      "allowlist": []
    },
    "session": {
      "dm_scope": "main",
      "account_id": "default"
    },
    "webhooks": [
      {
        "url": "https://example.com/hooks/gateway",
        "events": ["agent.final", "agent.error"]
      }
    ],
    "canvas": {
      "enabled": true
    },
    "voice": {
      "enabled": true,
      "stt": {
        "mode": "manual",
        "webhook_url": "",
        "command": "",
        "auto_inject": true
      },
      "tts": {
        "mode": "manual",
        "webhook_url": "",
        "command": ""
      }
    }
  }
}
```

## Fields

**File location (proposed):** `~/.euxis/config/gateway.json`

## Config Loading Order (v0.1)

1. `--config <path>` CLI override
2. `EUXIS_GATEWAY_CONFIG` environment override
3. Default: `~/.euxis/config/gateway.json`
4. Built-in defaults if file is missing

### `gateway`

- `bind` (string): interface to bind. Default: `127.0.0.1`
- `port` (int): port for WS + HTTP. Default: `18789`
- `health_enabled` (bool): expose health endpoint. Default: `true`
- `health_path` (string): path for health endpoint. Default: `/health`

### `gateway.auth`

- `mode` (string): `token` or `password`. Default: `token`

### `gateway.auth.token`

- `value` (string): shared token for Authorization header.
- `allow_query_param` (bool): allow `?token=` in WebSocket URL for webchat.

### `gateway.auth.password`

- `value` (string): password for basic auth (if enabled).

### `gateway.exec`

- `policy` (string): `deny`, `allowlist`, or `full`.
- `ask` (string): `off`, `on-miss`, `always`.
- `ask_fallback` (string): `deny`, `allowlist`, or `full`.
- `elevated` (string): `off`, `ask`, or `full`.
- `allowlist` (array): list of agent IDs allowed to execute.

### `gateway.channels`

- `slack.enabled` (bool): enable Slack adapter.
- `slack.mode` (string): `socket` or `events`.
- `slack.token` (string): bot token.
- `slack.app_token` (string): socket mode app token.
- `slack.signing_secret` (string): signing secret for events verification.
- `slack.events_path` (string): HTTP path for Slack Events API.
- `slack.max_length` (int): max characters per message chunk.
- `telegram.enabled` (bool): enable Telegram adapter.
- `telegram.mode` (string): `webhook` or `poll`.
- `telegram.token` (string): bot token.
- `telegram.webhook_url` (string): webhook endpoint.
- `telegram.webhook_path` (string): HTTP path for Telegram webhook.
- `telegram.poll_timeout` (int): long polling timeout (seconds).
- `telegram.poll_interval` (float): polling backoff (seconds).
- `telegram.max_length` (int): max characters per message chunk.

### `gateway.policy`

- `mention_required` (bool): require bot mention in group chats.
- `allowlist` (array): allowed sender IDs for group chats.

### `gateway.session`

- `dm_scope` (string): `main`, `per-peer`, `per-channel-peer`, or `per-account-channel-peer`.
- `account_id` (string): logical account identifier for multi-account gateways.

### `gateway.webhooks`

- List of webhook targets (objects with `url` and `events` array).
- Runtime updates are persisted to `~/.euxis/data/gateway/webhooks.json`.

### `gateway.canvas`

- `enabled` (bool): enable canvas endpoints and UI rendering.

### `gateway.voice`

- `enabled` (bool): enable voice endpoints.
- `stt.mode` (string): `manual`, `webhook`, or `command`.
- `stt.webhook_url` (string): POST audio metadata for transcription.
- `stt.command` (string): command to run for transcription (supports `{audio_path}`).
- `stt.auto_inject` (bool): inject transcript into agent pipeline.
- `tts.mode` (string): `manual`, `webhook`, or `command`.
- `tts.webhook_url` (string): POST text for synthesis.
- `tts.command` (string): command to run for synthesis (supports `{text}` and `{session_id}`).

## Notes

- If `mode = token`, clients should send `Authorization: Bearer <token>`.
- If `mode = password`, clients should send `Authorization: Basic <base64>`.
