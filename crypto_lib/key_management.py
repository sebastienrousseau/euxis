"""
Key management functions - Pure functional implementation

All key operations are pure functions that don't access the filesystem.
Keys are generated in memory and returned as bytes.
"""

import os
from typing import Optional
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
from cryptography.hazmat.backends import default_backend

from .exceptions import CryptoError, InvalidKeyError


def generate_key(key_size: int = 32) -> bytes:
    """
    Generate a cryptographically secure random key.

    Args:
        key_size: Size of key in bytes (default: 32 for AES-256)

    Returns:
        Random key bytes

    Raises:
        CryptoError: If key generation fails
        InvalidKeyError: If key_size is invalid
    """
    if key_size <= 0:
        raise InvalidKeyError(f"Key size must be positive, got {key_size}")

    if key_size not in [16, 24, 32]:  # AES-128, AES-192, AES-256
        raise InvalidKeyError(f"Unsupported key size: {key_size}. Use 16, 24, or 32 bytes")

    try:
        return os.urandom(key_size)
    except Exception as e:
        raise CryptoError(f"Key generation failed: {e}")


def derive_key(
    password: str,
    salt: Optional[bytes] = None,
    key_size: int = 32,
    iterations: int = 100000
) -> tuple[bytes, bytes]:
    """
    Derive encryption key from password using PBKDF2.

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
        raise InvalidKeyError("Password cannot be empty")

    if key_size <= 0:
        raise InvalidKeyError(f"Key size must be positive, got {key_size}")

    if iterations < 1000:
        raise InvalidKeyError(f"Iterations too low (minimum 1000), got {iterations}")

    try:
        # Generate salt if not provided
        if salt is None:
            salt = os.urandom(16)
        elif len(salt) < 8:
            raise InvalidKeyError("Salt must be at least 8 bytes")

        # Derive key using PBKDF2
        kdf = PBKDF2HMAC(
            algorithm=hashes.SHA256(),
            length=key_size,
            salt=salt,
            iterations=iterations,
            backend=default_backend()
        )

        key = kdf.derive(password.encode('utf-8'))
        return key, salt

    except Exception as e:
        raise CryptoError(f"Key derivation failed: {e}")


def validate_key(key: bytes, expected_size: int = 32) -> bool:
    """
    Validate that a key meets requirements.

    Args:
        key: Key bytes to validate
        expected_size: Expected key size in bytes

    Returns:
        True if key is valid

    Raises:
        InvalidKeyError: If key is invalid
    """
    if not isinstance(key, bytes):
        raise InvalidKeyError("Key must be bytes")

    if len(key) != expected_size:
        raise InvalidKeyError(f"Key must be {expected_size} bytes, got {len(key)}")

    # Check for obviously weak keys (all zeros, all ones, etc.)
    if key == b'\x00' * len(key):
        raise InvalidKeyError("Key cannot be all zeros")

    if key == b'\xff' * len(key):
        raise InvalidKeyError("Key cannot be all ones")

    return True