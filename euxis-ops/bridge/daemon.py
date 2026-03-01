#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Simple bridge daemon for channel webhooks -> Euxis gateway outbox."""

from __future__ import annotations

import argparse
import json
import os
import signal
import sys
from hashlib import sha256
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from bridge_common import append_jsonl, expand_user_path, load_config, utc_ts


class BridgeRuntime:
    def __init__(self, config: dict[str, Any]) -> None:
        self.config = config
        self.euxis_data = expand_user_path(config.get("paths", {}).get("euxis_data", "~/.euxis/euxis-data"))
        self.audit_path = expand_user_path(
            config.get("security", {}).get("audit", {}).get("log_path", "~/.euxis/euxis-data/security/bridge-audit.jsonl")
        )
        self.outbox_path = self.euxis_data / "bridge" / "outbox.jsonl"

    def token_for(self, channel: str) -> str:
        env_key = (
            self.config.get("bridge_daemon", {})
            .get("inbound_channels", {})
            .get(channel, {})
            .get("token_env", "")
        )
        if env_key:
            return os.environ.get(env_key, "")
        return ""

    def normalize_event(self, channel: str, payload: dict[str, Any]) -> dict[str, Any]:
        if channel == "telegram":
            msg = payload.get("message", {}) if isinstance(payload.get("message"), dict) else {}
            sender = str(msg.get("from", {}).get("id", "unknown")) if isinstance(msg.get("from"), dict) else "unknown"
            text = str(msg.get("text", ""))
            chat_id = str(msg.get("chat", {}).get("id", "main")) if isinstance(msg.get("chat"), dict) else "main"
        elif channel == "signal":
            sender = str(payload.get("source", "unknown"))
            text = str(payload.get("message", ""))
            chat_id = str(payload.get("groupId", "main"))
        else:
            sender = "unknown"
            text = ""
            chat_id = "main"

        session_id = f"{channel}:{chat_id}:{sender}"
        return {
            "ts": utc_ts(),
            "event": "bridge.inbound",
            "channel": channel,
            "sender": sender,
            "session_id": session_id,
            "content": text,
            "payload": payload,
        }

    def process(self, channel: str, payload: dict[str, Any]) -> dict[str, Any]:
        frame = self.normalize_event(channel, payload)
        canonical = json.dumps(frame, separators=(",", ":"), sort_keys=True)
        frame_hash = sha256(canonical.encode("utf-8")).hexdigest()

        append_jsonl(self.outbox_path, {"hash": frame_hash, **frame})
        append_jsonl(
            self.audit_path,
            {
                "ts": utc_ts(),
                "event": "bridge.forward.queued",
                "channel": channel,
                "session_id": frame["session_id"],
                "hash": frame_hash,
            },
        )

        return {"status": "queued", "hash": frame_hash, "session_id": frame["session_id"]}


class BridgeHandler(BaseHTTPRequestHandler):
    runtime: BridgeRuntime

    def _send_json(self, status: int, payload: dict[str, Any]) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _read_json(self) -> dict[str, Any]:
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length) if length else b"{}"
        parsed = json.loads(raw.decode("utf-8") or "{}")
        if isinstance(parsed, dict):
            return parsed
        return {}

    def do_GET(self) -> None:
        if self.path == "/health":
            self._send_json(200, {"status": "ok", "service": "euxis-bridge-daemon"})
            return
        self._send_json(404, {"error": "not_found"})

    def _auth_ok(self, channel: str) -> bool:
        expected = self.runtime.token_for(channel)
        if not expected:
            return True
        actual = self.headers.get("X-Bridge-Token", "")
        return actual == expected

    def do_POST(self) -> None:
        channel = ""
        if self.path.endswith("/telegram"):
            channel = "telegram"
        elif self.path.endswith("/signal"):
            channel = "signal"

        if not channel:
            self._send_json(404, {"error": "unknown_endpoint"})
            return

        if not self._auth_ok(channel):
            self._send_json(401, {"error": "unauthorized"})
            return

        payload = self._read_json()
        result = self.runtime.process(channel, payload)
        self._send_json(200, result)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", default="bridge_config.yaml")
    parser.add_argument("--bind", default="")
    parser.add_argument("--port", type=int, default=0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    config = load_config(Path(args.config))

    daemon_cfg = config.get("bridge_daemon", {})
    bind = args.bind or daemon_cfg.get("bind", "127.0.0.1")
    port = args.port or int(daemon_cfg.get("port", 18889))

    runtime = BridgeRuntime(config)
    BridgeHandler.runtime = runtime
    server = ThreadingHTTPServer((bind, port), BridgeHandler)

    def _shutdown(_signum: int, _frame: Any) -> None:
        server.shutdown()

    signal.signal(signal.SIGINT, _shutdown)
    signal.signal(signal.SIGTERM, _shutdown)
    print(json.dumps({"status": "listening", "bind": bind, "port": port}))
    server.serve_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
