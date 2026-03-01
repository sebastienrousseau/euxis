"""Identity registry abstractions and in-memory implementation."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Protocol


@dataclass(frozen=True, slots=True)
class AgentIdentity:
    """Immutable identity record for a registered agent."""

    did: str
    public_key: bytes
    credentials: tuple[object, ...] = ()
    attestations: tuple[object, ...] = ()
    created_at: float = 0.0
    metadata: dict[str, str] = field(default_factory=dict)


class IdentityRegistry(Protocol):
    """Protocol for identity storage backends."""

    def register(self, identity: AgentIdentity) -> None: ...
    def resolve(self, did: str) -> AgentIdentity | None: ...
    def list_agents(self) -> list[AgentIdentity]: ...
    def revoke(self, did: str) -> bool: ...


class InMemoryIdentityRegistry:
    """Dict-backed implementation of :class:`IdentityRegistry`."""

    def __init__(self) -> None:
        self._store: dict[str, AgentIdentity] = {}

    def register(self, identity: AgentIdentity) -> None:
        """Register an agent identity."""
        self._store[identity.did] = identity

    def resolve(self, did: str) -> AgentIdentity | None:
        """Resolve a DID to its identity, or ``None`` if not found."""
        return self._store.get(did)

    def list_agents(self) -> list[AgentIdentity]:
        """Return all registered identities."""
        return list(self._store.values())

    def revoke(self, did: str) -> bool:
        """Revoke (remove) an identity. Returns ``True`` if it existed."""
        if did in self._store:
            del self._store[did]
            return True
        return False
