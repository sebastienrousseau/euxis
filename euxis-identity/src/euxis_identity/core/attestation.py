"""Attestation creation and verification."""

from __future__ import annotations

import base64
import hashlib
import hmac
import json
import time
import uuid
from dataclasses import dataclass, field


@dataclass(frozen=True, slots=True)
class Attestation:
    """An attestation made by one agent about another."""

    id: str
    attester_did: str
    subject_did: str
    claim_type: str
    value: str
    signature: str
    timestamp: float


def _attestation_payload(
    attester_did: str,
    subject_did: str,
    claim_type: str,
    value: str,
) -> bytes:
    """Deterministic JSON payload used for signing."""
    obj = {
        "attester": attester_did,
        "subject": subject_did,
        "claim": claim_type,
        "value": value,
    }
    return json.dumps(obj, sort_keys=True).encode("utf-8")


def _sign(payload: bytes, key: bytes) -> str:
    """HMAC-SHA256 sign and base64-encode."""
    sig = hmac.new(key, payload, hashlib.sha256).digest()
    return base64.urlsafe_b64encode(sig).decode("ascii")


def create_attestation(
    attester_did: str,
    attester_key: bytes,
    subject_did: str,
    claim_type: str,
    value: str,
) -> Attestation:
    """Create a signed attestation."""
    payload = _attestation_payload(attester_did, subject_did, claim_type, value)
    signature = _sign(payload, attester_key)

    return Attestation(
        id=str(uuid.uuid4()),
        attester_did=attester_did,
        subject_did=subject_did,
        claim_type=claim_type,
        value=value,
        signature=signature,
        timestamp=time.time(),
    )


def verify_attestation(attestation: Attestation, attester_key: bytes) -> bool:
    """Verify an attestation's HMAC signature."""
    payload = _attestation_payload(
        attestation.attester_did,
        attestation.subject_did,
        attestation.claim_type,
        attestation.value,
    )
    expected = _sign(payload, attester_key)
    return hmac.compare_digest(attestation.signature, expected)
