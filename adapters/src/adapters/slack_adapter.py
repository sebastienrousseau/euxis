"""Slack adapter for the Euxis Gateway."""

from __future__ import annotations

from dataclasses import dataclass
import json
import logging
import threading
from typing import Any, Dict, Optional
from urllib import error as urlerror
from urllib import request as urlrequest

from adapters.sdk import MessageHandler
from gateway.utils import persist_message, timestamp

LOGGER = logging.getLogger(__name__)


@dataclass
class SlackAdapterConfig:
    token: str = ""
    app_token: str = ""
    mode: str = "socket"


class SlackAdapter:
    def __init__(self, config: SlackAdapterConfig, on_message: Optional[MessageHandler] = None) -> None:
        self.config = config
        self.session_meta: Dict[str, Dict[str, Any]] = {}
        self._socket_client: Optional[Any] = None
        self._lock = threading.Lock()
        self._on_message = on_message

    def connect(self) -> None:
        if self.config.mode != "socket":
            return
        if not self.config.token or not self.config.app_token:
            LOGGER.warning("Slack adapter missing token/app_token for socket mode")
            return
        try:
            from slack_sdk.socket_mode import SocketModeClient  # type: ignore
            from slack_sdk.socket_mode.response import SocketModeResponse  # type: ignore
            from slack_sdk.web import WebClient  # type: ignore
        except Exception as exc:  # pragma: no cover - optional dependency
            LOGGER.warning("Slack socket mode requires slack_sdk: %s", exc)
            return

        client = SocketModeClient(app_token=self.config.app_token, web_client=WebClient(token=self.config.token))

        def _listener(req: Any) -> None:
            if req.type != "events_api":
                return
            payload = req.payload or {}
            self.receive(payload)
            try:
                client.send_socket_mode_response(SocketModeResponse(envelope_id=req.envelope_id))
            except Exception as exc:  # pragma: no cover - best effort ack
                LOGGER.warning("Slack socket ack failed: %s", exc)

        client.socket_mode_request_listeners.append(_listener)
        client.connect()
        self._socket_client = client

    def receive(self, message: Dict[str, Any]) -> None:
        event = message.get("event") if isinstance(message, dict) else None
        payload = event or message
        if not isinstance(payload, dict):
            return
        if payload.get("subtype") == "bot_message":
            return

        channel = payload.get("channel") or payload.get("channel_id") or "unknown"
        text = payload.get("text", "") or payload.get("message", "")
        thread_ts = payload.get("thread_ts")
        user = payload.get("user") or payload.get("user_id")
        channel_type = payload.get("channel_type")
        scope = "group" if channel_type in {"channel", "group", "mpim"} else "dm"
        session_id = f"slack_{channel}"
        if thread_ts:
            session_id = f"{session_id}:{thread_ts}"
        entry = {
            "message_id": payload.get("ts", message.get("message_id", f"slack_{timestamp()}")),
            "role": "user",
            "content": text,
            "timestamp": timestamp(),
            "meta": {
                "channel_id": channel,
                "thread_ts": thread_ts,
                "sender": user,
                "scope": scope,
            },
        }
        with self._lock:
            self.session_meta[session_id] = {"channel": channel, "thread_ts": thread_ts, "user": user}
        persist_message(session_id, entry)
        if self._on_message:
            self._on_message(session_id, text, entry.get("meta", {}))

    def send(self, message: str, session_id: str) -> None:
        if not self.config.token:
            LOGGER.warning("Slack adapter missing token for send")
            return
        channel, thread_ts = self._resolve_session(session_id)
        if not channel:
            LOGGER.warning("Slack adapter missing channel for session %s", session_id)
            return
        payload = {"channel": channel, "text": message}
        if thread_ts:
            payload["thread_ts"] = thread_ts
        req = urlrequest.Request(
            "https://slack.com/api/chat.postMessage",
            data=json.dumps(payload).encode("utf-8"),
            headers={
                "Authorization": f"Bearer {self.config.token}",
                "Content-Type": "application/json; charset=utf-8",
            },
            method="POST",
        )
        try:
            with urlrequest.urlopen(req, timeout=10) as resp:
                data = json.loads(resp.read().decode("utf-8") or "{}")
                if not data.get("ok", False):
                    LOGGER.warning("Slack send failed: %s", data)
        except urlerror.URLError as exc:
            LOGGER.warning("Slack send error: %s", exc)

    def ack(self, message_id: str) -> None:
        return

    def disconnect(self) -> None:
        if self._socket_client:
            try:
                self._socket_client.disconnect()
            except Exception as exc:  # pragma: no cover - best effort
                LOGGER.warning("Slack socket disconnect failed: %s", exc)

    def _resolve_session(self, session_id: str) -> tuple[str, Optional[str]]:
        with self._lock:
            meta = self.session_meta.get(session_id, {})
        if meta:
            return meta.get("channel", ""), meta.get("thread_ts")
        raw = session_id
        if raw.startswith("slack:"):
            parts = raw.split(":")
            if len(parts) >= 3:
                return parts[1], parts[2]
            if len(parts) == 2:
                return parts[1], None
        if raw.startswith("slack_"):
            raw = raw[len("slack_") :]
        if ":" in raw:
            channel, thread_ts = raw.split(":", 1)
            return channel, thread_ts
        return raw, None
