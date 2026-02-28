# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Discord adapter for the Euxis Gateway."""

from __future__ import annotations

import asyncio
from dataclasses import dataclass
import logging
import threading
from typing import Any, Dict, Optional

from adapters.sdk import MessageHandler
from shared.gateway_utils import persist_message, timestamp

LOGGER = logging.getLogger("euxis.adapters.discord")

@dataclass
class DiscordAdapterConfig:
    token: str = ""
    enabled: bool = False

class DiscordAdapter:
    def __init__(self, config: DiscordAdapterConfig, on_message: Optional[MessageHandler] = None) -> None:
        self.config = config
        self.on_message = on_message
        self.client: Optional[Any] = None
        self.loop: Optional[asyncio.AbstractEventLoop] = None
        self.session_meta: Dict[str, Dict[str, Any]] = {}
        self._lock = threading.Lock()

    def connect(self) -> None:
        if not self.config.enabled or not self.config.token:
            LOGGER.info("Discord adapter disabled or missing token")
            return

        try:
            import discord
        except ImportError:
            LOGGER.warning("discord.py not installed, Discord adapter unavailable")
            return

        intents = discord.Intents.default()
        intents.message_content = True
        self.client = discord.Client(intents=intents)

        @self.client.event
        async def on_ready():
            LOGGER.info(f"Discord adapter logged in as {self.client.user}")

        @self.client.event
        async def on_message(message):
            if message.author == self.client.user:
                return
            self.receive(message)

        def run_discord():
            self.loop = asyncio.new_event_loop()
            asyncio.set_event_loop(self.loop)
            self.loop.run_until_complete(self.client.start(self.config.token))

        thread = threading.Thread(target=run_discord, daemon=True)
        thread.start()

    def receive(self, message: Any) -> None:
        channel_id = str(message.channel.id)
        author_id = str(message.author.id)
        content = message.content
        
        # Determine session ID (discord_<channel_id>)
        session_id = f"discord_{channel_id}"
        
        # Meta for Euxis
        meta = {
            "channel_id": channel_id,
            "sender": author_id,
            "scope": "dm" if str(message.channel.type) == "private" else "group",
            "mentions_bot": self.client.user in message.mentions if self.client else False
        }

        with self._lock:
            self.session_meta[session_id] = {"channel_id": channel_id}

        entry = {
            "message_id": str(message.id),
            "role": "user",
            "content": content,
            "timestamp": timestamp(),
            "meta": meta
        }
        persist_message(session_id, entry)
        
        if self.on_message:
            self.on_message(session_id, content, meta)

    def send(self, message: str, session_id: str) -> None:
        if not self.client or not self.client.is_ready():
            LOGGER.warning("Discord client not ready")
            return

        with self._lock:
            meta = self.session_meta.get(session_id, {})
        
        channel_id = meta.get("channel_id")
        if not channel_id:
            # Try to parse from session_id
            if session_id.startswith("discord_"):
                channel_id = session_id[len("discord_"):]
            else:
                LOGGER.warning(f"Could not resolve Discord channel for session {session_id}")
                return

        async def _send():
            channel = self.client.get_channel(int(channel_id))
            if not channel:
                channel = await self.client.fetch_channel(int(channel_id))
            if channel:
                await channel.send(message)

        if self.loop:
            asyncio.run_coroutine_threadsafe(_send(), self.loop)

    def ack(self, message_id: str) -> None:
        pass

    def disconnect(self) -> None:
        if self.client and self.loop:
            asyncio.run_coroutine_threadsafe(self.client.close(), self.loop)
