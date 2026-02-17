"""Gateway adapter SDK (minimal)."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Callable, Dict, Protocol

MessageHandler = Callable[[str, str, Dict[str, Any]], None]


class Adapter(Protocol):
    def connect(self) -> None: ...
    def receive(self, message: Dict[str, Any]) -> None: ...
    def send(self, message: str, session_id: str) -> None: ...
    def ack(self, message_id: str) -> None: ...
    def disconnect(self) -> None: ...


@dataclass
class AdapterConfig:
    enabled: bool = False
    mode: str = ""


def require(config: Dict[str, Any], key: str) -> str:
    value = config.get(key, "")
    if not value:
        raise ValueError(f"Missing required config: {key}")
    return value
