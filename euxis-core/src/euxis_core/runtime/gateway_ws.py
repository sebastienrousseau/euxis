# SPDX-License-Identifier: AGPL-3.0-or-later

"""Gateway WebSocket transport adapter.

Keeps networking concerns out of pure orchestration logic.
"""

from __future__ import annotations

from typing import Any

import websockets


class GatewayWebSocketClient:
    """Minimal async WebSocket client wrapper for gateway transport."""

    def __init__(self, url: str, token: str | None = None) -> None:
        self.url = url
        self.token = token
        self._ws = None

    @property
    def closed(self) -> bool:
        return self._ws is None or bool(self._ws.closed)

    async def connect(self) -> None:
        if self._ws is not None and not self._ws.closed:
            return
        headers = {}
        if self.token:
            headers["Authorization"] = f"Bearer {self.token}"
        self._ws = await websockets.connect(self.url, additional_headers=headers)

    async def send(self, message: str) -> None:
        if self._ws is None:
            raise RuntimeError("WebSocket is not connected")
        await self._ws.send(message)

    async def iter_messages(self):
        if self._ws is None:
            raise RuntimeError("WebSocket is not connected")
        async for message in self._ws:
            yield message

    async def close(self) -> None:
        if self._ws is not None:
            await self._ws.close()
            self._ws = None

    @staticmethod
    def invalid_status_code(exc: Exception) -> int | None:
        # Handle websocket library status exceptions across versions without
        # binding to deprecated exception classes.
        status = getattr(exc, "status_code", None)
        if status is None:
            response = getattr(exc, "response", None)
            status = getattr(response, "status_code", None)
        if status is not None:
            try:
                return int(status)
            except (TypeError, ValueError):
                return None
        return None
