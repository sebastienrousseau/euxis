"""Key management functions - Pure functional implementation.

All key operations are pure functions that don't access the filesystem.
Keys are generated in memory and returned as bytes.
"""

import os

from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC

from .exceptions import CryptoError, InvalidKeyError

MIN_PBKDF2_ITERATIONS = 1000
MIN_SALT_BYTES = 8

def generate_key(key_size: int = 32) -> bytes:
    """Generate a cryptographically secure random key.

    Args:
        key_size: Size of key in bytes (default: 32 for AES-256)

    Returns:
        Random key bytes

    Raises:
        CryptoError: If key generation fails
        InvalidKeyError: If key_size is invalid

    """
    if key_size <= 0:
        msg = f"Key size must be positive, got {key_size}"
        raise InvalidKeyError(msg)

    if key_size not in [16, 24, 32]:  # AES-128, AES-192, AES-256
        msg = f"Unsupported key size: {key_size}. Use 16, 24, or 32 bytes"
        raise InvalidKeyError(msg)

    try:
        return os.urandom(key_size)
    except Exception as exc:
        msg = f"Key generation failed: {exc}"
        raise CryptoError(msg) from exc


def derive_key(
    password: str,
    salt: bytes | None = None,
    key_size: int = 32,
    iterations: int = 100000
) -> tuple[bytes, bytes]:
    """Derive encryption key from password using PBKDF2.

    Args:
        password: Password string
        salt: Salt bytes (generated if None)
        key_size: Desired key size in bytes
        iterations: PBKDF2 iteration count

    Returns:
        Tuple of (derived_key, salt)

    Raises:
        CryptoError: If key derivation fails
        InvalidKeyError: If parameters are invalid

    """
    if not password:
        msg = "Password cannot be empty"
        raise InvalidKeyError(msg)

    if key_size <= 0:
        msg = f"Key size must be positive, got {key_size}"
        raise InvalidKeyError(msg)

    if iterations < MIN_PBKDF2_ITERATIONS:
        msg = f"Iterations too low (minimum {MIN_PBKDF2_ITERATIONS}), got {iterations}"
        raise InvalidKeyError(msg)

    # Generate salt if not provided
    if salt is None:
        salt = os.urandom(16)
    elif len(salt) < MIN_SALT_BYTES:
        msg = f"Salt must be at least {MIN_SALT_BYTES} bytes"
        raise InvalidKeyError(msg)

    try:
        # Derive key using PBKDF2
        kdf = PBKDF2HMAC(
            algorithm=hashes.SHA256(),
            length=key_size,
            salt=salt,
            iterations=iterations,
            backend=default_backend()
        )

        key = kdf.derive(password.encode("utf-8"))

    except Exception as exc:
        msg = f"Key derivation failed: {exc}"
        raise CryptoError(msg) from exc
    else:
        return key, salt


def validate_key(key: bytes, expected_size: int = 32) -> bool:
    """Validate that a key meets requirements.

    Args:
        key: Key bytes to validate
        expected_size: Expected key size in bytes

    Returns:
        True if key is valid

    Raises:
        InvalidKeyError: If key is invalid

    """
    if not isinstance(key, bytes):
        msg = "Key must be bytes"
        raise InvalidKeyError(msg)

    if len(key) != expected_size:
        msg = f"Key must be {expected_size} bytes, got {len(key)}"
        raise InvalidKeyError(msg)

    # Check for obviously weak keys (all zeros, all ones, etc.)
    if key == b"\x00" * len(key):
        msg = "Key cannot be all zeros"
        raise InvalidKeyError(msg)

    if key == b"\xff" * len(key):
        msg = "Key cannot be all ones"
        raise InvalidKeyError(msg)

    return True
