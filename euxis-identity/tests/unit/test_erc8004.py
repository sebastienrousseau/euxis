"""Tests for ERC-8004 agent card generation."""

import time

from euxis_identity.core.did import ServiceEndpoint
from euxis_identity.core.erc8004 import (
    ERC8004AgentCard,
    generate_agent_card,
    serialize_agent_card,
)
from euxis_identity.core.registry import AgentIdentity


def _make_identity(
    did: str = "did:euxis:agent-1",
    name: str = "Agent One",
    description: str = "Test agent",
) -> AgentIdentity:
    return AgentIdentity(
        did=did,
        public_key=b"test-key",
        credentials=("cred-1",),
        attestations=("att-1", "att-2"),
        created_at=time.time(),
        metadata={"name": name, "description": description},
    )


def test_generate_agent_card() -> None:
    identity = _make_identity()
    card = generate_agent_card(identity)

    assert isinstance(card, ERC8004AgentCard)
    assert card.agent_id == "did:euxis:agent-1"
    assert card.agent_uri == "did:euxis:agent-1"
    assert card.metadata["name"] == "Agent One"
    assert card.metadata["description"] == "Test agent"
    assert "did:euxis" in card.metadata["protocols"]
    assert "erc-8004" in card.metadata["protocols"]
    assert card.metadata["trust_config"]["attestation_count"] == 2
    assert card.metadata["trust_config"]["credential_count"] == 1


def test_generate_agent_card_with_services() -> None:
    identity = _make_identity()
    svc = ServiceEndpoint(
        id="did:euxis:agent-1#api",
        type="APIEndpoint",
        service_endpoint="https://api.euxis.dev/v1",
    )
    card = generate_agent_card(identity, services=(svc,))

    assert "services" in card.metadata
    services = card.metadata["services"]
    assert len(services) == 1
    assert services[0]["type"] == "APIEndpoint"
    assert services[0]["endpoint"] == "https://api.euxis.dev/v1"


def test_generate_agent_card_default_metadata() -> None:
    identity = AgentIdentity(did="did:euxis:bare", public_key=b"k")
    card = generate_agent_card(identity)

    assert card.metadata["name"] == "did:euxis:bare"
    assert card.metadata["description"] == ""


def test_serialize_agent_card() -> None:
    identity = _make_identity()
    card = generate_agent_card(identity)
    serialized = serialize_agent_card(card)

    assert isinstance(serialized, dict)
    assert serialized["agent_id"] == "did:euxis:agent-1"
    assert serialized["agent_uri"] == "did:euxis:agent-1"
    assert isinstance(serialized["metadata"], dict)
    assert serialized["metadata"]["name"] == "Agent One"


def test_serialize_roundtrip_keys() -> None:
    identity = _make_identity()
    card = generate_agent_card(identity)
    serialized = serialize_agent_card(card)

    expected_keys = {"agent_id", "agent_uri", "metadata"}
    assert set(serialized.keys()) == expected_keys


def test_agent_card_no_services_key_without_services() -> None:
    identity = _make_identity()
    card = generate_agent_card(identity)
    assert "services" not in card.metadata
