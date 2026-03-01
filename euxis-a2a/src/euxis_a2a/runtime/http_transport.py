"""HTTP-based A2A transport implementation."""

from __future__ import annotations

from typing import Any, AsyncIterator

from euxis_a2a.core.agent_card import AgentCard, validate_card


class HttpA2ATransport:
    """httpx-based A2A transport."""

    def __init__(self, timeout: float = 30.0) -> None:
        self._timeout = timeout
        self._client: Any = None

    def _ensure_client(self) -> Any:
        if self._client is not None:
            return self._client
        try:
            import httpx
        except ImportError as exc:
            raise RuntimeError("httpx is required for HTTP transport. Install with: pip install httpx") from exc
        self._client = httpx.Client(timeout=self._timeout)
        return self._client

    def send(self, endpoint: str, payload: dict[str, Any]) -> dict[str, Any]:
        client = self._ensure_client()
        resp = client.post(endpoint, json=payload)
        resp.raise_for_status()
        return resp.json()

    async def stream(self, endpoint: str, payload: dict[str, Any]) -> AsyncIterator[dict[str, Any]]:
        try:
            import httpx
        except ImportError as exc:
            raise RuntimeError("httpx required") from exc
        async with httpx.AsyncClient(timeout=self._timeout) as client:
            async with client.stream("POST", endpoint, json=payload) as resp:
                async for line in resp.aiter_lines():
                    if line.startswith("data: "):
                        import json
                        yield json.loads(line[6:])


class HttpA2ADiscovery:
    """Fetches agent cards from /.well-known/agent.json."""

    def __init__(self, timeout: float = 10.0) -> None:
        self._timeout = timeout

    def fetch_card(self, endpoint: str) -> AgentCard:
        try:
            import httpx
        except ImportError as exc:
            raise RuntimeError("httpx required") from exc
        url = endpoint.rstrip("/") + "/.well-known/agent.json"
        resp = httpx.get(url, timeout=self._timeout)
        resp.raise_for_status()
        return validate_card(resp.json())
