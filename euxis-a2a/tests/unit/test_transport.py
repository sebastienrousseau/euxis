"""Tests for A2A transport protocol abstractions."""

from __future__ import annotations

import inspect

from euxis_a2a.core.transport import A2ADiscovery, A2ATransport


def test_a2a_transport_protocol_exists() -> None:
    assert inspect.isclass(A2ATransport)


def test_a2a_transport_has_send_method() -> None:
    assert hasattr(A2ATransport, "send")


def test_a2a_transport_has_stream_method() -> None:
    assert hasattr(A2ATransport, "stream")


def test_a2a_discovery_protocol_exists() -> None:
    assert inspect.isclass(A2ADiscovery)


def test_a2a_discovery_has_fetch_card_method() -> None:
    assert hasattr(A2ADiscovery, "fetch_card")


def test_transport_send_signature() -> None:
    sig = inspect.signature(A2ATransport.send)
    params = list(sig.parameters.keys())
    assert "endpoint" in params
    assert "payload" in params


def test_discovery_fetch_card_signature() -> None:
    sig = inspect.signature(A2ADiscovery.fetch_card)
    params = list(sig.parameters.keys())
    assert "endpoint" in params
