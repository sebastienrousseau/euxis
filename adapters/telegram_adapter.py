"""Telegram adapter for the Euxis Gateway."""

from __future__ import annotations

from dataclasses import dataclass
import json
import logging
import threading
import time
from typing import Any, Dict, Optional
from urllib import error as urlerror
from urllib import parse as urlparse
from urllib import request as urlrequest

from adapters.sdk import MessageHandler
from gateway.utils import persist_message, timestamp

LOGGER = logging.getLogger(__name__)


@dataclass
class TelegramAdapterConfig:
    token: str = ""
    mode: str = "webhook"
    webhook_url: str = ""
    poll_timeout: int = 20
    poll_interval: float = 1.5


class TelegramAdapter:
    def __init__(self, config: TelegramAdapterConfig, on_message: Optional[MessageHandler] = None) -> None:
        self.config = config
        self._stop_event = threading.Event()
        self._poll_thread: Optional[threading.Thread] = None
        self._offset = 0
        self.session_meta: Dict[str, Dict[str, Any]] = {}
        self._on_message = on_message

    def connect(self) -> None:
        if not self.config.token:
            LOGGER.warning("Telegram adapter missing token")
            return
        if self.config.mode == "webhook":
            if not self.config.webhook_url:
                LOGGER.warning("Telegram webhook_url is empty")
                return
            self._api_call("setWebhook", {"url": self.config.webhook_url})
            return
        if self.config.mode in {"poll", "polling"}:
            if self._poll_thread and self._poll_thread.is_alive():
                return
            self._stop_event.clear()
            self._poll_thread = threading.Thread(target=self._poll_loop, daemon=True)
            self._poll_thread.start()

    def receive(self, message: Dict[str, Any]) -> None:
        payload = message.get("message") if isinstance(message, dict) else None
        if not payload and isinstance(message, dict):
            payload = message.get("edited_message") or message.get("channel_post") or message
        if not isinstance(payload, dict):
            return
        chat = payload.get("chat", {}) if isinstance(payload.get("chat"), dict) else {}
        chat_id = chat.get("id") or payload.get("chat_id")
        if chat_id is None:
            return
        text = payload.get("text") or payload.get("caption") or ""
        scope = "group" if chat.get("type") in {"group", "supergroup"} else "dm"
        session_id = f"telegram_{chat_id}"
        entry = {
            "message_id": payload.get("message_id", f"telegram_{timestamp()}"),
            "role": "user",
            "content": text,
            "timestamp": timestamp(),
            "meta": {
                "channel_id": "telegram",
                "chat_id": chat_id,
                "thread_id": payload.get("message_thread_id"),
                "sender": (payload.get("from") or {}).get("id"),
                "username": (payload.get("from") or {}).get("username"),
                "scope": scope,
            },
        }
        self.session_meta[session_id] = {"chat_id": chat_id}
        persist_message(session_id, entry)
        if self._on_message:
            self._on_message(session_id, text, entry.get("meta", {}))

    def send(self, message: str, session_id: str) -> None:
        if not self.config.token:
            LOGGER.warning("Telegram adapter missing token for send")
            return
        chat_id = self._resolve_chat_id(session_id)
        if chat_id is None:
            LOGGER.warning("Telegram adapter missing chat id for session %s", session_id)
            return
        self._api_call("sendMessage", {"chat_id": chat_id, "text": message})

    def ack(self, message_id: str) -> None:
        return

    def disconnect(self) -> None:
        self._stop_event.set()
        if self._poll_thread and self._poll_thread.is_alive():
            self._poll_thread.join(timeout=2)
        if self.config.mode == "webhook" and self.config.token:
            self._api_call("deleteWebhook", {})

    def _poll_loop(self) -> None:
        while not self._stop_event.is_set():
            payload = {
                "timeout": self.config.poll_timeout,
                "offset": self._offset,
            }
            data = self._api_call("getUpdates", payload) or {}
            updates = data.get("result", [])
            if isinstance(updates, list):
                for update in updates:
                    update_id = update.get("update_id")
                    if isinstance(update_id, int):
                        self._offset = update_id + 1
                    self.receive(update)
            time.sleep(self.config.poll_interval)

    def _resolve_chat_id(self, session_id: str) -> Optional[int]:
        meta = self.session_meta.get(session_id)
        if meta and "chat_id" in meta:
            return meta["chat_id"]
        raw = session_id
        if raw.startswith("telegram:"):
            parts = raw.split(":")
            if len(parts) >= 2:
                raw = parts[1]
        if raw.startswith("telegram_"):
            raw = raw[len("telegram_") :]
        try:
            return int(raw)
        except ValueError:
            return None

    def _api_call(self, method: str, data: Dict[str, Any]) -> Dict[str, Any]:
        url = f"https://api.telegram.org/bot{self.config.token}/{method}"
        payload = urlparse.urlencode(data).encode("utf-8")
        req = urlrequest.Request(url, data=payload, method="POST")
        try:
            with urlrequest.urlopen(req, timeout=15) as resp:
                raw = resp.read().decode("utf-8") or "{}"
                return json.loads(raw)
        except urlerror.URLError as exc:
            LOGGER.warning("Telegram API error: %s", exc)
        except json.JSONDecodeError as exc:
            LOGGER.warning("Telegram API decode error: %s", exc)
        return {}
