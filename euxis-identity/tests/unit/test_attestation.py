"""Tests for attestation creation and verification."""

from dataclasses import replace

from euxis_identity.core.attestation import (
    Attestation,
    create_attestation,
    verify_attestation,
)


ATTESTER_DID = "did:euxis:attester-1"
SUBJECT_DID = "did:euxis:subject-1"
ATTESTER_KEY = b"attester-secret-key"


def test_create_and_verify_roundtrip() -> None:
    att = create_attestation(
        ATTESTER_DID, ATTESTER_KEY, SUBJECT_DID, "capability", "sandbox-exec",
    )

    assert isinstance(att, Attestation)
    assert att.attester_did == ATTESTER_DID
    assert att.subject_did == SUBJECT_DID
    assert att.claim_type == "capability"
    assert att.value == "sandbox-exec"
    assert att.timestamp > 0
    assert len(att.id) > 0

    assert verify_attestation(att, ATTESTER_KEY) is True


def test_wrong_key_rejected() -> None:
    att = create_attestation(
        ATTESTER_DID, ATTESTER_KEY, SUBJECT_DID, "capability", "sandbox-exec",
    )
    assert verify_attestation(att, b"wrong-key") is False


def test_tampered_value_detected() -> None:
    att = create_attestation(
        ATTESTER_DID, ATTESTER_KEY, SUBJECT_DID, "capability", "sandbox-exec",
    )
    tampered = replace(att, value="root-access")
    assert verify_attestation(tampered, ATTESTER_KEY) is False


def test_tampered_claim_type_detected() -> None:
    att = create_attestation(
        ATTESTER_DID, ATTESTER_KEY, SUBJECT_DID, "capability", "sandbox-exec",
    )
    tampered = replace(att, claim_type="admin-override")
    assert verify_attestation(tampered, ATTESTER_KEY) is False


def test_tampered_subject_detected() -> None:
    att = create_attestation(
        ATTESTER_DID, ATTESTER_KEY, SUBJECT_DID, "capability", "sandbox-exec",
    )
    tampered = replace(att, subject_did="did:euxis:evil-agent")
    assert verify_attestation(tampered, ATTESTER_KEY) is False


def test_attestation_is_frozen() -> None:
    att = create_attestation(
        ATTESTER_DID, ATTESTER_KEY, SUBJECT_DID, "capability", "sandbox-exec",
    )
    try:
        att.value = "tampered"  # type: ignore[misc]
        assert False, "Should have raised AttributeError"
    except AttributeError:
        pass
