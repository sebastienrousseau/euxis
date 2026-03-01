"""ERC-8004 on-chain agent card generation."""

from __future__ import annotations

from dataclasses import dataclass, field

from euxis_identity.core.did import ServiceEndpoint
from euxis_identity.core.registry import AgentIdentity


@dataclass(frozen=True, slots=True)
class ERC8004AgentCard:
    """ERC-8004 compliant agent identity card."""

    agent_id: str
    agent_uri: str
    metadata: dict[str, object] = field(default_factory=dict)


def generate_agent_card(
    identity: AgentIdentity,
    services: tuple[ServiceEndpoint, ...] = (),
) -> ERC8004AgentCard:
    """Generate an ERC-8004 agent card from a registered identity."""
    metadata: dict[str, object] = {
        "name": identity.metadata.get("name", identity.did),
        "description": identity.metadata.get("description", ""),
        "protocols": ["did:euxis", "erc-8004"],
        "trust_config": {
            "attestation_count": len(identity.attestations),
            "credential_count": len(identity.credentials),
        },
    }

    if services:
        metadata["services"] = [
            {
                "id": s.id,
                "type": s.type,
                "endpoint": s.service_endpoint,
            }
            for s in services
        ]

    return ERC8004AgentCard(
        agent_id=identity.did,
        agent_uri=identity.did,
        metadata=metadata,
    )


def serialize_agent_card(card: ERC8004AgentCard) -> dict[str, object]:
    """Serialize an agent card to a JSON-compatible dict."""
    return {
        "agent_id": card.agent_id,
        "agent_uri": card.agent_uri,
        "metadata": card.metadata,
    }
