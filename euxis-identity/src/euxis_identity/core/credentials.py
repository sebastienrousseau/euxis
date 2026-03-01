"""Verifiable credential issuance and verification."""

from __future__ import annotations

import base64
import hashlib
import hmac
import json
import time
from dataclasses import dataclass, field


@dataclass(frozen=True, slots=True)
class Claim:
    """A single claim within a verifiable credential."""

    type: str
    value: str
    scope: str


@dataclass(frozen=True, slots=True)
class Proof:
    """Cryptographic proof attached to a credential."""

    type: str
    created: float
    verification_method: str
    proof_value: str


@dataclass(frozen=True, slots=True)
class VerifiableCredential:
    """A verifiable credential binding claims to a subject."""

    id: str
    issuer_did: str
    subject_did: str
    claims: tuple[Claim, ...]
    proof: Proof
    issued_at: float
    expires_at: float


def _claims_payload(
    issuer_did: str,
    subject_did: str,
    claims: tuple[Claim, ...],
) -> bytes:
    """Deterministic JSON payload used for signing."""
    obj = {
        "issuer": issuer_did,
        "subject": subject_did,
        "claims": [(c.type, c.value, c.scope) for c in claims],
    }
    return json.dumps(obj, sort_keys=True).encode("utf-8")


def _sign(payload: bytes, key: bytes) -> str:
    """HMAC-SHA256 sign and base64-encode."""
    sig = hmac.new(key, payload, hashlib.sha256).digest()
    return base64.urlsafe_b64encode(sig).decode("ascii")


def issue_credential(
    issuer_did: str,
    issuer_key: bytes,
    subject_did: str,
    claims: tuple[Claim, ...],
    ttl_seconds: int = 3600,
) -> VerifiableCredential:
    """Issue a verifiable credential signed with HMAC-SHA256."""
    now = time.time()

    # Deterministic credential id from issuer + subject + timestamp.
    id_input = f"{issuer_did}:{subject_did}:{now}".encode("utf-8")
    cred_id = hashlib.sha256(id_input).hexdigest()

    payload = _claims_payload(issuer_did, subject_did, claims)
    signature = _sign(payload, issuer_key)

    proof = Proof(
        type="HmacSha256Signature2024",
        created=now,
        verification_method=f"{issuer_did}#key-1",
        proof_value=signature,
    )

    return VerifiableCredential(
        id=cred_id,
        issuer_did=issuer_did,
        subject_did=subject_did,
        claims=claims,
        proof=proof,
        issued_at=now,
        expires_at=now + ttl_seconds,
    )


def verify_credential(credential: VerifiableCredential, issuer_key: bytes) -> bool:
    """Verify a credential's proof and expiry.

    Returns ``False`` when the credential has expired or the signature
    does not match.
    """
    if time.time() > credential.expires_at:
        return False

    payload = _claims_payload(
        credential.issuer_did,
        credential.subject_did,
        credential.claims,
    )
    expected = _sign(payload, issuer_key)
    return hmac.compare_digest(credential.proof.proof_value, expected)
