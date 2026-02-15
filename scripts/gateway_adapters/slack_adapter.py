"""Slack adapter stub for the Euxis Gateway."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict


@dataclass
class SlackAdapterConfig:
    token: str = ""
    app_token: str = ""
    mode: str = "socket"


class SlackAdapter:
    def __init__(self, config: SlackAdapterConfig) -> None:
        self.config = config

    def connect(self) -> None:
        raise NotImplementedError("Slack adapter connect is not implemented")

    def receive(self, message: Dict[str, Any]) -> None:
        raise NotImplementedError("Slack adapter receive is not implemented")

    def send(self, message: str, session_id: str) -> None:
        raise NotImplementedError("Slack adapter send is not implemented")

    def ack(self, message_id: str) -> None:
        raise NotImplementedError("Slack adapter ack is not implemented")

    def disconnect(self) -> None:
        raise NotImplementedError("Slack adapter disconnect is not implemented")
