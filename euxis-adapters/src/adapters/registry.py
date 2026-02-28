# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Adapter registry for gateway channel configuration."""

from __future__ import annotations

from typing import Any, Dict, Optional

from adapters.slack_adapter import SlackAdapter, SlackAdapterConfig
from adapters.telegram_adapter import TelegramAdapter, TelegramAdapterConfig
from adapters.discord_adapter import DiscordAdapter, DiscordAdapterConfig
from adapters.whatsapp_adapter import WhatsAppAdapter, WhatsAppAdapterConfig
from adapters.sdk import MessageHandler

def build_adapters(config: Dict[str, Any], on_message: Optional[MessageHandler] = None) -> Dict[str, Any]:
    channels = config.get("gateway", {}).get("channels", {})
    adapters: Dict[str, Any] = {}

    slack_cfg = channels.get("slack", {})
    if slack_cfg.get("enabled"):
        adapters["slack"] = SlackAdapter(
            SlackAdapterConfig(
                token=slack_cfg.get("token", ""),
                app_token=slack_cfg.get("app_token", ""),
                mode=slack_cfg.get("mode", "socket"),
            ),
            on_message=on_message,
        )

    telegram_cfg = channels.get("telegram", {})
    if telegram_cfg.get("enabled"):
        adapters["telegram"] = TelegramAdapter(
            TelegramAdapterConfig(
                token=telegram_cfg.get("token", ""),
                mode=telegram_cfg.get("mode", "webhook"),
                webhook_url=telegram_cfg.get("webhook_url", ""),
                poll_timeout=telegram_cfg.get("poll_timeout", 20),
                poll_interval=telegram_cfg.get("poll_interval", 1.5),
            ),
            on_message=on_message,
        )

    discord_cfg = channels.get("discord", {})
    if discord_cfg.get("enabled"):
        adapters["discord"] = DiscordAdapter(
            DiscordAdapterConfig(
                token=discord_cfg.get("token", ""),
                enabled=True,
            ),
            on_message=on_message,
        )

    whatsapp_cfg = channels.get("whatsapp", {})
    if whatsapp_cfg.get("enabled"):
        adapters["whatsapp"] = WhatsAppAdapter(
            WhatsAppAdapterConfig(
                token=whatsapp_cfg.get("token", ""),
                phone_number_id=whatsapp_cfg.get("phone_number_id", ""),
                verify_token=whatsapp_cfg.get("verify_token", ""),
                enabled=True,
            ),
            on_message=on_message,
        )

    return adapters
