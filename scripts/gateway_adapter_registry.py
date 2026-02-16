"""Adapter registry for gateway channel configuration."""

from __future__ import annotations

from typing import Any, Dict

from gateway_adapters.slack_adapter import SlackAdapter, SlackAdapterConfig
from gateway_adapters.telegram_adapter import TelegramAdapter, TelegramAdapterConfig


def build_adapters(config: Dict[str, Any]) -> Dict[str, Any]:
    channels = config.get("gateway", {}).get("channels", {})
    adapters: Dict[str, Any] = {}

    slack_cfg = channels.get("slack", {})
    if slack_cfg.get("enabled"):
        adapters["slack"] = SlackAdapter(
            SlackAdapterConfig(
                token=slack_cfg.get("token", ""),
                app_token=slack_cfg.get("app_token", ""),
                mode=slack_cfg.get("mode", "socket"),
            )
        )

    telegram_cfg = channels.get("telegram", {})
    if telegram_cfg.get("enabled"):
        adapters["telegram"] = TelegramAdapter(
            TelegramAdapterConfig(
                token=telegram_cfg.get("token", ""),
                mode=telegram_cfg.get("mode", "webhook"),
                webhook_url=telegram_cfg.get("webhook_url", ""),
            )
        )

    return adapters
