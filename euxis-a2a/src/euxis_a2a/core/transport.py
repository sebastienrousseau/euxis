"""A2A transport protocol abstractions."""

from __future__ import annotations

from typing import Any, AsyncIterator, Protocol

from euxis_a2a.core.agent_card import AgentCard


class A2ATransport(Protocol):
    def send(self, endpoint: str, payload: dict[str, Any]) -> dict[str, Any]: ...
    async def stream(self, endpoint: str, payload: dict[str, Any]) -> AsyncIterator[dict[str, Any]]: ...


class A2ADiscovery(Protocol):
    def fetch_card(self, endpoint: str) -> AgentCard: ...
