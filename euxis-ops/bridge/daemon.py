#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Bridge daemon for channel webhooks -> Euxis gateway forwarder."""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import signal
import sys
import time
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
        self.dead_letter_path = self.euxis_data / "bridge" / "dead_letter.jsonl"

        gateway_cfg = config.get("bridge_daemon", {}).get("gateway_forward", {})
        self.gateway_ws_url = str(gateway_cfg.get("ws_url", "ws://127.0.0.1:18789/ws"))
        self.gateway_token_env = str(gateway_cfg.get("auth_env", "EUXIS_GATEWAY_TOKEN"))
        self.live_forward = bool(gateway_cfg.get("live_forward", True))
        self.max_retries = int(gateway_cfg.get("max_retries", 3))
        self.backoff_ms = int(gateway_cfg.get("backoff_ms", 250))

        self.idempotency_cache_path = self.euxis_data / "bridge" / "idempotency_keys.jsonl"
        self.seen_keys: set[str] = set()
        self._load_seen_keys()

    def _load_seen_keys(self) -> None:
        if not self.idempotency_cache_path.exists():
            return
        lines = self.idempotency_cache_path.read_text(encoding="utf-8", errors="replace").splitlines()
        for line in lines[-10000:]:
            try:
                entry = json.loads(line)
            except Exception:
                continue
            key = entry.get("idempotency_key")
            if isinstance(key, str) and key:
                self.seen_keys.add(key)

    def _remember_key(self, key: str) -> None:
        self.seen_keys.add(key)
        append_jsonl(self.idempotency_cache_path, {"ts": utc_ts(), "idempotency_key": key})

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

    def channel_enabled(self, channel: str) -> bool:
        cfg = self.config.get("bridge_daemon", {}).get("inbound_channels", {}).get(channel, {})
        return bool(cfg.get("enabled", True))

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

    async def _send_gateway_frame(self, frame: dict[str, Any]) -> None:
        import websockets  # type: ignore

        token = os.environ.get(self.gateway_token_env, "")
        headers = {}
        if token:
            headers["Authorization"] = f"Bearer {token}"

        request = {
            "type": "request",
            "id": f"bridge_{int(time.time() * 1000)}",
            "method": "chat.send",
            "params": {
                "session_id": frame["session_id"],
                "role": "user",
                "content": frame["content"],
                "meta": {"agent": "bridge-daemon", "approved": True},
                "channel_id": frame["channel"],
                "scope": "dm",
            },
        }

        async with websockets.connect(self.gateway_ws_url, additional_headers=headers) as ws:
            await ws.send(json.dumps(request))

    def forward_to_gateway(self, frame: dict[str, Any]) -> tuple[bool, str]:
        if not self.live_forward:
            return True, "live_forward_disabled"

        for attempt in range(1, self.max_retries + 1):
            try:
                asyncio.run(self._send_gateway_frame(frame))
                return True, f"forwarded_attempt_{attempt}"
            except Exception as exc:
                if attempt >= self.max_retries:
                    return False, str(exc)
                time.sleep(self.backoff_ms / 1000.0)
        return False, "unreachable"

    def process(self, channel: str, payload: dict[str, Any], idempotency_key: str = "") -> dict[str, Any]:
        frame = self.normalize_event(channel, payload)
        canonical = json.dumps(frame, separators=(",", ":"), sort_keys=True)
        frame_hash = sha256(canonical.encode("utf-8")).hexdigest()

        idem_key = idempotency_key or frame_hash
        if idem_key in self.seen_keys:
            return {"status": "duplicate", "hash": frame_hash, "session_id": frame["session_id"]}
        self._remember_key(idem_key)

        append_jsonl(self.outbox_path, {"hash": frame_hash, "idempotency_key": idem_key, **frame})

        ok, detail = self.forward_to_gateway(frame)
        if ok:
            append_jsonl(
                self.audit_path,
                {
                    "ts": utc_ts(),
                    "event": "bridge.forward.ok",
                    "channel": channel,
                    "session_id": frame["session_id"],
                    "hash": frame_hash,
                    "detail": detail,
                },
            )
            return {"status": "forwarded", "hash": frame_hash, "session_id": frame["session_id"], "detail": detail}

        append_jsonl(
            self.dead_letter_path,
            {
                "ts": utc_ts(),
                "hash": frame_hash,
                "error": detail,
                "frame": frame,
            },
        )
        append_jsonl(
            self.audit_path,
            {
                "ts": utc_ts(),
                "event": "bridge.forward.dead_letter",
                "channel": channel,
                "session_id": frame["session_id"],
                "hash": frame_hash,
                "detail": detail,
            },
        )
        return {"status": "dead_letter", "hash": frame_hash, "session_id": frame["session_id"], "detail": detail}


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

        if not self.runtime.channel_enabled(channel):
            self._send_json(403, {"error": "channel_disabled"})
            return

        if not self._auth_ok(channel):
            self._send_json(401, {"error": "unauthorized"})
            return

        payload = self._read_json()
        idem_key = self.headers.get("X-Idempotency-Key", "")
        result = self.runtime.process(channel, payload, idem_key)
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
