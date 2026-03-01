"""Signature verification for bridged skill entrypoints."""

from __future__ import annotations

import base64
import binascii
from pathlib import Path


class SkillVerificationError(RuntimeError):
    """Raised when signature verification fails or is misconfigured."""


def _load_crypto() -> tuple[object | None, object | None]:
    try:
        import crypto_lib_rs  # type: ignore[import-untyped]

        return crypto_lib_rs, None
    except ModuleNotFoundError:
        try:
            from cryptography.hazmat.primitives import serialization

            return None, serialization
        except ModuleNotFoundError:
            return None, None


def _decode_signature(raw: bytes, sig_encoding: str) -> bytes:
    if sig_encoding == "raw":
        return raw
    if sig_encoding == "base64":
        return base64.b64decode(raw)
    if sig_encoding == "hex":
        return binascii.unhexlify(raw.strip())
    raise SkillVerificationError(f"unsupported signature encoding: {sig_encoding}")


def verify_skill_signature(
    entrypoint: Path,
    public_key_path: str,
    sig_encoding: str = "base64",
) -> bool:
    """Verify detached Ed25519 signature for a skill entrypoint.

    Expects a `.sig` file adjacent to the entrypoint.
    """
    sig_path = entrypoint.with_suffix(entrypoint.suffix + ".sig")
    pub_path = Path(public_key_path)

    if not sig_path.exists():
        return False
    if not pub_path.exists():
        return False

    message = entrypoint.read_bytes()
    signature = _decode_signature(sig_path.read_bytes(), sig_encoding)

    crypto_lib_rs, serialization = _load_crypto()

    if crypto_lib_rs is not None:
        public_key = base64.b64decode(pub_path.read_bytes())
        return bool(crypto_lib_rs.ed25519_verify(public_key, message, signature))

    if serialization is not None:
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey

        pub = serialization.load_pem_public_key(pub_path.read_bytes())
        if not isinstance(pub, Ed25519PublicKey):
            return False
        try:
            pub.verify(signature, message)
            return True
        except Exception:
            return False

    raise SkillVerificationError(
        "no crypto backend available: install crypto_lib_rs or cryptography"
    )


def require_verified(
    entrypoint: Path,
    public_key_path: str,
    sig_encoding: str = "base64",
) -> None:
    """Verify signature or raise SkillVerificationError."""
    if not verify_skill_signature(entrypoint, public_key_path, sig_encoding):
        raise SkillVerificationError(
            f"signature verification failed for {entrypoint}"
        )
