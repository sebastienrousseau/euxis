# Bridge Daemon Runbook (macOS, Linux, WSL)

This runbook deploys `euxis-bridge-daemon` for OpenClaw-compatible remote channels.

## Prerequisites

- `EUXIS_HOME` set (default `~/.euxis`)
- Euxis gateway running on `ws://127.0.0.1:18789/ws`
- Bridge config at `~/.euxis/bridge_config.yaml` or repository root `bridge_config.yaml`

## Local run

```bash
python3 ~/.euxis/euxis-ops/bridge/daemon.py --config ~/.euxis/bridge_config.yaml
```

## macOS (launchd)

Create `~/Library/LaunchAgents/co.euxis.bridge.plist`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
  <dict>
    <key>Label</key><string>co.euxis.bridge</string>
    <key>ProgramArguments</key>
    <array>
      <string>/usr/bin/python3</string>
      <string>/Users/you/.euxis/euxis-ops/bridge/daemon.py</string>
      <string>--config</string>
      <string>/Users/you/.euxis/bridge_config.yaml</string>
    </array>
    <key>RunAtLoad</key><true/>
    <key>KeepAlive</key><true/>
    <key>EnvironmentVariables</key>
    <dict>
      <key>EUXIS_GATEWAY_TOKEN</key><string>replace-me</string>
      <key>TELEGRAM_BOT_TOKEN</key><string>replace-me</string>
    </dict>
  </dict>
</plist>
```

Load:

```bash
launchctl load ~/Library/LaunchAgents/co.euxis.bridge.plist
```

## Linux (systemd --user)

Create `~/.config/systemd/user/euxis-bridge.service`:

```ini
[Unit]
Description=Euxis Bridge Daemon
After=network-online.target

[Service]
Type=simple
ExecStart=/usr/bin/python3 %h/.euxis/euxis-ops/bridge/daemon.py --config %h/.euxis/bridge_config.yaml
Restart=always
RestartSec=2
Environment=EUXIS_GATEWAY_TOKEN=replace-me
Environment=TELEGRAM_BOT_TOKEN=replace-me

[Install]
WantedBy=default.target
```

Enable:

```bash
systemctl --user daemon-reload
systemctl --user enable --now euxis-bridge.service
```

## WSL

Use same `systemd --user` unit on WSL distributions with systemd enabled.
For non-systemd WSL, run daemon from shell profile or tmux session.

## Health checks

- `GET /health` on bridge bind/port
- audit log: `~/.euxis/euxis-data/security/bridge-audit.jsonl`
- outbox: `~/.euxis/euxis-data/bridge/outbox.jsonl`
- dead-letter queue: `~/.euxis/euxis-data/bridge/dead_letter.jsonl`
