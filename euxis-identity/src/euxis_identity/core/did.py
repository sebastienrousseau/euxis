"""W3C DID-based agent identity primitives."""

from __future__ import annotations

import base64
import time
from dataclasses import dataclass, field


@dataclass(frozen=True, slots=True)
class VerificationMethod:
    """A verification method within a DID document."""

    id: str
    type: str
    controller: str
    public_key_multibase: str


@dataclass(frozen=True, slots=True)
class ServiceEndpoint:
    """A service endpoint exposed by a DID subject."""

    id: str
    type: str
    service_endpoint: str


@dataclass(frozen=True, slots=True)
class DIDDocument:
    """W3C DID Document representation for an Euxis agent."""

    id: str
    context: str
    verification_methods: tuple[VerificationMethod, ...]
    authentication: tuple[str, ...]
    services: tuple[ServiceEndpoint, ...]
    created: float
    updated: float


def _multibase_encode(public_key: bytes) -> str:
    """Encode a public key using multibase (base64url, 'z' prefix)."""
    encoded = base64.urlsafe_b64encode(public_key).decode("ascii").rstrip("=")
    return f"z{encoded}"


def create_did(agent_id: str, public_key: bytes) -> str:
    """Create a DID string for an agent.

    Format: ``did:euxis:{agent_id}``
    """
    return f"did:euxis:{agent_id}"


def create_did_document(
    agent_id: str,
    public_key: bytes,
    services: tuple[ServiceEndpoint, ...] = (),
) -> DIDDocument:
    """Build a full DID document for the given agent."""
    did = create_did(agent_id, public_key)
    multibase = _multibase_encode(public_key)
    now = time.time()

    vm = VerificationMethod(
        id=f"{did}#key-1",
        type="Ed25519VerificationKey2020",
        controller=did,
        public_key_multibase=multibase,
    )

    return DIDDocument(
        id=did,
        context="https://www.w3.org/ns/did/v1",
        verification_methods=(vm,),
        authentication=(vm.id,),
        services=services,
        created=now,
        updated=now,
    )


def resolve_did(did: str) -> tuple[str, str]:
    """Parse a ``did:euxis:{id}`` string into ``("euxis", id)``.

    Raises :class:`ValueError` when the DID is malformed or uses an
    unsupported method.
    """
    parts = did.split(":")
    if len(parts) != 3 or parts[0] != "did" or parts[1] != "euxis":
        raise ValueError(f"Invalid DID format: {did}")
    return (parts[1], parts[2])
