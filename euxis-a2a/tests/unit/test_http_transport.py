"""Tests for HTTP A2A transport implementation."""

from __future__ import annotations

from typing import Any
from unittest.mock import MagicMock, patch

import pytest

from euxis_a2a.runtime.http_transport import HttpA2ADiscovery, HttpA2ATransport


class TestHttpA2ATransport:
    def test_init_defaults(self) -> None:
        transport = HttpA2ATransport()
        assert transport._timeout == 30.0
        assert transport._client is None

    def test_init_custom_timeout(self) -> None:
        transport = HttpA2ATransport(timeout=60.0)
        assert transport._timeout == 60.0

    def test_ensure_client_creates_httpx_client(self) -> None:
        mock_client = MagicMock()
        mock_httpx = MagicMock()
        mock_httpx.Client.return_value = mock_client

        transport = HttpA2ATransport(timeout=15.0)
        with patch.dict("sys.modules", {"httpx": mock_httpx}):
            client = transport._ensure_client()

        assert client is mock_client
        mock_httpx.Client.assert_called_once_with(timeout=15.0)

    def test_ensure_client_reuses_existing(self) -> None:
        transport = HttpA2ATransport()
        mock_client = MagicMock()
        transport._client = mock_client
        assert transport._ensure_client() is mock_client

    def test_send_calls_post(self) -> None:
        mock_response = MagicMock()
        mock_response.json.return_value = {"result": "ok"}
        mock_client = MagicMock()
        mock_client.post.return_value = mock_response

        transport = HttpA2ATransport()
        transport._client = mock_client

        result = transport.send("https://example.com/a2a", {"method": "test"})

        mock_client.post.assert_called_once_with(
            "https://example.com/a2a", json={"method": "test"},
        )
        mock_response.raise_for_status.assert_called_once()
        assert result == {"result": "ok"}

    def test_ensure_client_httpx_not_installed(self) -> None:
        transport = HttpA2ATransport()
        with patch.dict("sys.modules", {"httpx": None}):
            with pytest.raises(RuntimeError, match="httpx is required"):
                transport._ensure_client()


class TestHttpA2ADiscovery:
    def test_init_defaults(self) -> None:
        discovery = HttpA2ADiscovery()
        assert discovery._timeout == 10.0

    def test_init_custom_timeout(self) -> None:
        discovery = HttpA2ADiscovery(timeout=5.0)
        assert discovery._timeout == 5.0

    def test_fetch_card_constructs_url(self) -> None:
        mock_response = MagicMock()
        mock_response.json.return_value = {
            "name": "remote",
            "description": "Remote agent",
            "url": "https://remote.example.com",
        }
        mock_httpx = MagicMock()
        mock_httpx.get.return_value = mock_response

        discovery = HttpA2ADiscovery(timeout=5.0)
        with patch.dict("sys.modules", {"httpx": mock_httpx}):
            card = discovery.fetch_card("https://remote.example.com")

        mock_httpx.get.assert_called_once_with(
            "https://remote.example.com/.well-known/agent.json", timeout=5.0,
        )
        assert card.name == "remote"
        assert card.url == "https://remote.example.com"

    def test_fetch_card_strips_trailing_slash(self) -> None:
        mock_response = MagicMock()
        mock_response.json.return_value = {
            "name": "agent", "url": "https://a.com",
        }
        mock_httpx = MagicMock()
        mock_httpx.get.return_value = mock_response

        discovery = HttpA2ADiscovery()
        with patch.dict("sys.modules", {"httpx": mock_httpx}):
            discovery.fetch_card("https://a.com/")

        mock_httpx.get.assert_called_once_with(
            "https://a.com/.well-known/agent.json", timeout=10.0,
        )

    def test_fetch_card_httpx_not_installed(self) -> None:
        discovery = HttpA2ADiscovery()
        with patch.dict("sys.modules", {"httpx": None}):
            with pytest.raises(RuntimeError, match="httpx required"):
                discovery.fetch_card("https://example.com")
