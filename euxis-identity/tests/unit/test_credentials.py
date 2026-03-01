"""Tests for verifiable credential issuance and verification."""

from dataclasses import replace
from unittest.mock import patch

import pytest

from euxis_identity.core.credentials import (
    Claim,
    Proof,
    VerifiableCredential,
    issue_credential,
    verify_credential,
)


ISSUER_DID = "did:euxis:issuer-1"
SUBJECT_DID = "did:euxis:subject-1"
ISSUER_KEY = b"super-secret-issuer-key"
CLAIMS = (
    Claim(type="role", value="admin", scope="platform"),
    Claim(type="tier", value="gold", scope="billing"),
)


def test_issue_and_verify_roundtrip() -> None:
    cred = issue_credential(ISSUER_DID, ISSUER_KEY, SUBJECT_DID, CLAIMS)

    assert isinstance(cred, VerifiableCredential)
    assert cred.issuer_did == ISSUER_DID
    assert cred.subject_did == SUBJECT_DID
    assert cred.claims == CLAIMS
    assert cred.issued_at < cred.expires_at
    assert cred.expires_at - cred.issued_at == 3600
    assert len(cred.id) == 64  # SHA-256 hex digest

    assert verify_credential(cred, ISSUER_KEY) is True


def test_custom_ttl() -> None:
    cred = issue_credential(
        ISSUER_DID, ISSUER_KEY, SUBJECT_DID, CLAIMS, ttl_seconds=60,
    )
    assert cred.expires_at - cred.issued_at == 60


def test_expired_credential_rejected() -> None:
    cred = issue_credential(ISSUER_DID, ISSUER_KEY, SUBJECT_DID, CLAIMS, ttl_seconds=100)

    # Simulate time advancing past expiry.
    future_time = cred.expires_at + 1
    with patch("euxis_identity.core.credentials.time") as mock_time:
        mock_time.time.return_value = future_time
        assert verify_credential(cred, ISSUER_KEY) is False


def test_tampered_claims_detected() -> None:
    cred = issue_credential(ISSUER_DID, ISSUER_KEY, SUBJECT_DID, CLAIMS)

    tampered_claims = (Claim(type="role", value="superadmin", scope="platform"),)
    tampered = replace(cred, claims=tampered_claims)

    assert verify_credential(tampered, ISSUER_KEY) is False


def test_wrong_key_rejected() -> None:
    cred = issue_credential(ISSUER_DID, ISSUER_KEY, SUBJECT_DID, CLAIMS)
    assert verify_credential(cred, b"wrong-key") is False


def test_proof_structure() -> None:
    cred = issue_credential(ISSUER_DID, ISSUER_KEY, SUBJECT_DID, CLAIMS)
    proof = cred.proof

    assert isinstance(proof, Proof)
    assert proof.type == "HmacSha256Signature2024"
    assert proof.verification_method == f"{ISSUER_DID}#key-1"
    assert len(proof.proof_value) > 0


def test_credential_is_frozen() -> None:
    cred = issue_credential(ISSUER_DID, ISSUER_KEY, SUBJECT_DID, CLAIMS)
    with pytest.raises(AttributeError):
        cred.id = "tampered"  # type: ignore[misc]
