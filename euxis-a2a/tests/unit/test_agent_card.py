"""Tests for A2A agent card types and utilities."""

from __future__ import annotations

import pytest

from euxis_a2a.core.agent_card import (
    AgentCard,
    AuthSchema,
    Capability,
    generate_agent_card,
    serialize_card,
    validate_card,
)


def test_generate_agent_card_basic() -> None:
    card = generate_agent_card(
        name="test-agent",
        description="A test agent",
        endpoint="https://example.com/a2a",
    )
    assert card.name == "test-agent"
    assert card.description == "A test agent"
    assert card.url == "https://example.com/a2a"
    assert card.capabilities == ()
    assert card.protocols == ("a2a/1.0",)
    assert card.auth.type == "none"
    assert card.identity_did is None


def test_generate_agent_card_with_capabilities() -> None:
    cap = Capability(
        name="summarize",
        description="Summarize text",
        input_schema={"type": "string"},
        output_schema={"type": "string"},
    )
    card = generate_agent_card(
        name="summarizer",
        description="Summarization agent",
        endpoint="https://example.com/a2a",
        capabilities=(cap,),
        identity_did="did:euxis:abc123",
    )
    assert len(card.capabilities) == 1
    assert card.capabilities[0].name == "summarize"
    assert card.identity_did == "did:euxis:abc123"


def test_serialize_card_basic() -> None:
    card = generate_agent_card(
        name="test", description="desc", endpoint="https://example.com",
    )
    data = serialize_card(card)
    assert data["name"] == "test"
    assert data["description"] == "desc"
    assert data["url"] == "https://example.com"
    assert data["protocols"] == ["a2a/1.0"]
    assert data["auth"]["type"] == "none"
    assert "identityDid" not in data


def test_serialize_card_with_identity_did() -> None:
    card = generate_agent_card(
        name="test", description="desc", endpoint="https://example.com",
        identity_did="did:euxis:xyz",
    )
    data = serialize_card(card)
    assert data["identityDid"] == "did:euxis:xyz"


def test_serialize_card_with_capabilities() -> None:
    cap = Capability(name="cap1", description="desc1", input_schema={"a": 1}, output_schema={"b": 2})
    card = generate_agent_card(
        name="test", description="desc", endpoint="https://example.com",
        capabilities=(cap,),
    )
    data = serialize_card(card)
    assert len(data["capabilities"]) == 1
    assert data["capabilities"][0]["name"] == "cap1"
    assert data["capabilities"][0]["inputSchema"] == {"a": 1}
    assert data["capabilities"][0]["outputSchema"] == {"b": 2}


def test_validate_card_full() -> None:
    data = {
        "name": "remote-agent",
        "description": "A remote agent",
        "url": "https://remote.example.com",
        "capabilities": [
            {"name": "translate", "description": "Translate text",
             "inputSchema": {"lang": "string"}, "outputSchema": {"text": "string"}},
        ],
        "protocols": ["a2a/1.0", "a2a/2.0"],
        "auth": {"type": "bearer", "config": {"header": "Authorization"}},
        "identityDid": "did:euxis:remote1",
    }
    card = validate_card(data)
    assert card.name == "remote-agent"
    assert card.url == "https://remote.example.com"
    assert len(card.capabilities) == 1
    assert card.capabilities[0].name == "translate"
    assert card.protocols == ("a2a/1.0", "a2a/2.0")
    assert card.auth.type == "bearer"
    assert card.identity_did == "did:euxis:remote1"


def test_validate_card_minimal() -> None:
    data = {"name": "minimal", "url": "https://min.example.com"}
    card = validate_card(data)
    assert card.name == "minimal"
    assert card.description == ""
    assert card.capabilities == ()
    assert card.auth.type == "none"


def test_validate_card_missing_name() -> None:
    with pytest.raises(ValueError, match="requires 'name' and 'url' fields"):
        validate_card({"url": "https://example.com"})


def test_validate_card_missing_url() -> None:
    with pytest.raises(ValueError, match="requires 'name' and 'url' fields"):
        validate_card({"name": "no-url"})


def test_agent_card_is_frozen() -> None:
    card = generate_agent_card(name="x", description="y", endpoint="https://z.com")
    with pytest.raises(AttributeError):
        card.name = "changed"  # type: ignore[misc]


def test_capability_defaults() -> None:
    cap = Capability(name="c", description="d")
    assert cap.input_schema == {}
    assert cap.output_schema == {}


def test_auth_schema_defaults() -> None:
    auth = AuthSchema(type="api_key")
    assert auth.config == {}
