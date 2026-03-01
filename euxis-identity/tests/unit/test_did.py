"""Tests for W3C DID primitives."""

import base64

import pytest

from euxis_identity.core.did import (
    DIDDocument,
    ServiceEndpoint,
    VerificationMethod,
    create_did,
    create_did_document,
    resolve_did,
)


def test_create_did_format() -> None:
    did = create_did("agent-42", b"fake-key")
    assert did == "did:euxis:agent-42"


def test_create_did_document_structure() -> None:
    key = b"test-public-key-bytes"
    doc = create_did_document("agent-1", key)

    assert isinstance(doc, DIDDocument)
    assert doc.id == "did:euxis:agent-1"
    assert doc.context == "https://www.w3.org/ns/did/v1"
    assert len(doc.verification_methods) == 1
    assert len(doc.authentication) == 1
    assert doc.services == ()
    assert doc.created > 0
    assert doc.updated == doc.created


def test_did_document_verification_method() -> None:
    key = b"test-public-key-bytes"
    doc = create_did_document("agent-1", key)
    vm = doc.verification_methods[0]

    assert isinstance(vm, VerificationMethod)
    assert vm.id == "did:euxis:agent-1#key-1"
    assert vm.type == "Ed25519VerificationKey2020"
    assert vm.controller == "did:euxis:agent-1"


def test_multibase_encoding() -> None:
    key = b"hello-world"
    doc = create_did_document("agent-mb", key)
    vm = doc.verification_methods[0]

    expected = "z" + base64.urlsafe_b64encode(key).decode("ascii").rstrip("=")
    assert vm.public_key_multibase == expected


def test_did_document_with_services() -> None:
    svc = ServiceEndpoint(
        id="did:euxis:agent-1#svc-1",
        type="MessagingService",
        service_endpoint="https://example.com/msg",
    )
    doc = create_did_document("agent-1", b"key", services=(svc,))
    assert len(doc.services) == 1
    assert doc.services[0].type == "MessagingService"


def test_resolve_did_valid() -> None:
    method, agent_id = resolve_did("did:euxis:agent-42")
    assert method == "euxis"
    assert agent_id == "agent-42"


def test_resolve_did_invalid_prefix() -> None:
    with pytest.raises(ValueError, match="Invalid DID format"):
        resolve_did("urn:euxis:agent-1")


def test_resolve_did_wrong_method() -> None:
    with pytest.raises(ValueError, match="Invalid DID format"):
        resolve_did("did:web:example.com")


def test_resolve_did_too_few_parts() -> None:
    with pytest.raises(ValueError, match="Invalid DID format"):
        resolve_did("did:euxis")


def test_resolve_did_too_many_parts() -> None:
    with pytest.raises(ValueError, match="Invalid DID format"):
        resolve_did("did:euxis:agent:extra")


def test_did_document_is_frozen() -> None:
    doc = create_did_document("agent-frozen", b"key")
    with pytest.raises(AttributeError):
        doc.id = "tampered"  # type: ignore[misc]
