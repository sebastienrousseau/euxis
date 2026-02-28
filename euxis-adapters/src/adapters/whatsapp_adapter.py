# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""WhatsApp adapter for the Euxis Gateway using Meta Graph API."""

from __future__ import annotations

from dataclasses import dataclass
import json
import logging
import threading
from typing import Any, Dict, Optional
from urllib import error as urlerror
from urllib import request as urlrequest

from adapters.sdk import MessageHandler
from shared.gateway_utils import persist_message, timestamp

LOGGER = logging.getLogger("euxis.adapters.whatsapp")

@dataclass
class WhatsAppAdapterConfig:
    token: str = ""
    phone_number_id: str = ""
    verify_token: str = ""
    enabled: bool = False

class WhatsAppAdapter:
    def __init__(self, config: WhatsAppAdapterConfig, on_message: Optional[MessageHandler] = None) -> None:
        self.config = config
        self.on_message = on_message
        self.session_meta: Dict[str, Dict[str, Any]] = {}
        self._lock = threading.Lock()

    def connect(self) -> None:
        if not self.config.enabled or not self.config.token:
            LOGGER.info("WhatsApp adapter disabled or missing token")
            return
        LOGGER.info("WhatsApp adapter ready (webhook-based)")

    def receive(self, payload: Dict[str, Any]) -> None:
        """Handle incoming WhatsApp webhook payload."""
        # Meta sends a list of changes
        entries = payload.get("entry", [])
        for entry in entries:
            for change in entry.get("changes", []):
                value = change.get("value", {})
                messages = value.get("messages", [])
                for message in messages:
                    self._handle_single_message(message, value)

    def _handle_single_message(self, message: Dict[str, Any], value: Dict[str, Any]) -> None:
        from_id = message.get("from")
        message_id = message.get("id")
        text = message.get("text", {}).get("body", "")
        
        if not text or not from_id:
            return

        session_id = f"whatsapp_{from_id}"
        meta = {
            "channel_id": from_id,
            "sender": from_id,
            "scope": "dm",
            "approved": True
        }

        with self._lock:
            self.session_meta[session_id] = {"from_id": from_id}

        entry = {
            "message_id": message_id,
            "role": "user",
            "content": text,
            "timestamp": timestamp(),
            "meta": meta
        }
        persist_message(session_id, entry)

        if self.on_message:
            self.on_message(session_id, text, meta)

    def send(self, message: str, session_id: str) -> None:
        if not self.config.token or not self.config.phone_number_id:
            LOGGER.warning("WhatsApp adapter missing token or phone_number_id")
            return

        with self._lock:
            meta = self.session_meta.get(session_id, {})
        
        from_id = meta.get("from_id")
        if not from_id:
            if session_id.startswith("whatsapp_"):
                from_id = session_id[len("whatsapp_"):]
            else:
                LOGGER.warning(f"Could not resolve WhatsApp recipient for session {session_id}")
                return

        url = f"https://graph.facebook.com/v17.0/{self.config.phone_number_id}/messages"
        payload = {
            "messaging_product": "whatsapp",
            "recipient_type": "individual",
            "to": from_id,
            "type": "text",
            "text": {"body": message}
        }
        
        req = urlrequest.Request(
            url,
            data=json.dumps(payload).encode("utf-8"),
            headers={
                "Authorization": f"Bearer {self.config.token}",
                "Content-Type": "application/json"
            },
            method="POST"
        )

        try:
            with urlrequest.urlopen(req, timeout=10) as resp:
                data = json.loads(resp.read().decode("utf-8") or "{}")
                if "error" in data:
                    LOGGER.warning(f"WhatsApp send failed: {data['error']}")
        except urlerror.URLError as exc:
            LOGGER.warning(f"WhatsApp send error: {exc}")

    def ack(self, message_id: str) -> None:
        pass

    def disconnect(self) -> None:
        pass
