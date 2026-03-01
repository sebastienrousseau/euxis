"""Google A2A protocol agent card types and utilities."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any


@dataclass(frozen=True, slots=True)
class Capability:
    name: str
    description: str
    input_schema: dict[str, Any] = field(default_factory=dict)
    output_schema: dict[str, Any] = field(default_factory=dict)


@dataclass(frozen=True, slots=True)
class AuthSchema:
    type: str  # "bearer", "api_key", "none"
    config: dict[str, Any] = field(default_factory=dict)


@dataclass(frozen=True, slots=True)
class AgentCard:
    name: str
    description: str
    url: str
    capabilities: tuple[Capability, ...] = ()
    protocols: tuple[str, ...] = ("a2a/1.0",)
    auth: AuthSchema = field(default_factory=lambda: AuthSchema(type="none"))
    identity_did: str | None = None


def generate_agent_card(
    name: str,
    description: str,
    endpoint: str,
    capabilities: tuple[Capability, ...] = (),
    identity_did: str | None = None,
) -> AgentCard:
    return AgentCard(
        name=name, description=description, url=endpoint,
        capabilities=capabilities, identity_did=identity_did,
    )


def serialize_card(card: AgentCard) -> dict[str, Any]:
    result: dict[str, Any] = {
        "name": card.name,
        "description": card.description,
        "url": card.url,
        "capabilities": [
            {"name": c.name, "description": c.description,
             "inputSchema": c.input_schema, "outputSchema": c.output_schema}
            for c in card.capabilities
        ],
        "protocols": list(card.protocols),
        "auth": {"type": card.auth.type, "config": card.auth.config},
    }
    if card.identity_did:
        result["identityDid"] = card.identity_did
    return result


def validate_card(data: dict[str, Any]) -> AgentCard:
    if "name" not in data or "url" not in data:
        raise ValueError("agent card requires 'name' and 'url' fields")
    caps = tuple(
        Capability(
            name=c["name"], description=c.get("description", ""),
            input_schema=c.get("inputSchema", {}), output_schema=c.get("outputSchema", {}),
        )
        for c in data.get("capabilities", [])
    )
    auth_data = data.get("auth", {"type": "none"})
    auth = AuthSchema(type=auth_data.get("type", "none"), config=auth_data.get("config", {}))
    return AgentCard(
        name=data["name"], description=data.get("description", ""),
        url=data["url"], capabilities=caps,
        protocols=tuple(data.get("protocols", ["a2a/1.0"])),
        auth=auth, identity_did=data.get("identityDid"),
    )
