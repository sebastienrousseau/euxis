"""Tests for identity registry."""

import time

from euxis_identity.core.registry import AgentIdentity, InMemoryIdentityRegistry


def _make_identity(did: str, key: bytes = b"key") -> AgentIdentity:
    return AgentIdentity(
        did=did,
        public_key=key,
        credentials=(),
        attestations=(),
        created_at=time.time(),
        metadata={"name": did},
    )


def test_register_and_resolve() -> None:
    reg = InMemoryIdentityRegistry()
    identity = _make_identity("did:euxis:agent-1")
    reg.register(identity)

    resolved = reg.resolve("did:euxis:agent-1")
    assert resolved is not None
    assert resolved.did == "did:euxis:agent-1"


def test_resolve_unknown_returns_none() -> None:
    reg = InMemoryIdentityRegistry()
    assert reg.resolve("did:euxis:unknown") is None


def test_list_agents() -> None:
    reg = InMemoryIdentityRegistry()
    id1 = _make_identity("did:euxis:a1")
    id2 = _make_identity("did:euxis:a2")
    reg.register(id1)
    reg.register(id2)

    agents = reg.list_agents()
    assert len(agents) == 2
    dids = {a.did for a in agents}
    assert dids == {"did:euxis:a1", "did:euxis:a2"}


def test_list_agents_empty() -> None:
    reg = InMemoryIdentityRegistry()
    assert reg.list_agents() == []


def test_revoke_existing() -> None:
    reg = InMemoryIdentityRegistry()
    identity = _make_identity("did:euxis:agent-1")
    reg.register(identity)

    assert reg.revoke("did:euxis:agent-1") is True
    assert reg.resolve("did:euxis:agent-1") is None


def test_revoke_nonexistent() -> None:
    reg = InMemoryIdentityRegistry()
    assert reg.revoke("did:euxis:ghost") is False


def test_register_overwrite() -> None:
    reg = InMemoryIdentityRegistry()
    id1 = _make_identity("did:euxis:agent-1", key=b"old-key")
    id2 = _make_identity("did:euxis:agent-1", key=b"new-key")
    reg.register(id1)
    reg.register(id2)

    resolved = reg.resolve("did:euxis:agent-1")
    assert resolved is not None
    assert resolved.public_key == b"new-key"


def test_identity_default_fields() -> None:
    identity = AgentIdentity(did="did:euxis:bare", public_key=b"k")
    assert identity.credentials == ()
    assert identity.attestations == ()
    assert identity.created_at == 0.0
    assert identity.metadata == {}
