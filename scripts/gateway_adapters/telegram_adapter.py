"""Telegram adapter stub for the Euxis Gateway."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict


@dataclass
class TelegramAdapterConfig:
    token: str = ""
    mode: str = "webhook"
    webhook_url: str = ""


class TelegramAdapter:
    def __init__(self, config: TelegramAdapterConfig) -> None:
        self.config = config

    def connect(self) -> None:
        raise NotImplementedError("Telegram adapter connect is not implemented")

    def receive(self, message: Dict[str, Any]) -> None:
        raise NotImplementedError("Telegram adapter receive is not implemented")

    def send(self, message: str, session_id: str) -> None:
        raise NotImplementedError("Telegram adapter send is not implemented")

    def ack(self, message_id: str) -> None:
        raise NotImplementedError("Telegram adapter ack is not implemented")

    def disconnect(self) -> None:
        raise NotImplementedError("Telegram adapter disconnect is not implemented")
