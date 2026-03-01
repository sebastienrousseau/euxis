# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Core cryptographic operations - Pure functional implementation.

All functions in this module are pure functions:
- No filesystem access
- No global state modification
- Deterministic output for given inputs (except for IV generation)
- Return structured result objects
"""

import base64
import os
from dataclasses import dataclass

try:
    import crypto_lib_rs
    HAS_RUST_CORE = True
except ImportError:
    HAS_RUST_CORE = False
    from cryptography.hazmat.backends import default_backend
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

from .exceptions import CryptoError, DecryptionError, EncryptionError, InvalidKeyError

AES_256_KEY_SIZE = 32
GCM_AUTH_TAG_SIZE = 16
ENCRYPTED_PARTS = 4


@dataclass(frozen=True)
class EncryptionResult:
    """Immutable result object containing all encryption artifacts."""

    ciphertext: bytes
    iv: bytes
    salt: bytes | None = None
    algorithm: str = "AES-256-GCM"

    def to_base64(self) -> str:
        """Encode all components to base64 for safe transport."""
        data = {
            "ciphertext": base64.b64encode(self.ciphertext).decode("utf-8"),
            "iv": base64.b64encode(self.iv).decode("utf-8"),
            "algorithm": self.algorithm
        }
        if self.salt:
            data["salt"] = base64.b64encode(self.salt).decode("utf-8")

        # Simple concatenated format: algorithm:iv:salt:ciphertext (salt optional)
        if self.salt:
            return f"{self.algorithm}:{data['iv']}:{data['salt']}:{data['ciphertext']}"
        return f"{self.algorithm}:{data['iv']}::{data['ciphertext']}"


@dataclass(frozen=True)
class DecryptionResult:
    """Immutable result object containing decryption output."""

    plaintext: bytes
    algorithm: str
    iv: bytes
    salt: bytes | None = None

    def to_string(self, encoding: str = "utf-8") -> str:
        """Convert plaintext to string with specified encoding."""
        try:
            return self.plaintext.decode(encoding)
        except UnicodeDecodeError as exc:
            msg = f"Failed to decode plaintext with {encoding}: {exc}"
            raise CryptoError(msg) from exc


def encrypt(
    data: str | bytes,
    key: bytes,
    algorithm: str = "AES-256-GCM"
) -> EncryptionResult:
    """Pure function to encrypt data with given key.

    Args:
        data: Data to encrypt (string or bytes)
        key: Encryption key (must be 32 bytes for AES-256)
        algorithm: Encryption algorithm (default: AES-256-GCM)

    Returns:
        EncryptionResult object containing ciphertext, IV, and metadata

    Raises:
        CryptoError: On encryption failure
        InvalidKeyError: If key is invalid

    """
    if algorithm != "AES-256-GCM":
        msg = f"Unsupported algorithm: {algorithm}"
        raise EncryptionError(msg, algorithm=algorithm)

    if len(key) != AES_256_KEY_SIZE:
        msg = f"AES-256 requires 32-byte key, got {len(key)} bytes"
        raise InvalidKeyError(
            msg,
            key_type="AES-256",
            expected_size=32,
            actual_size=len(key),
        )

    # Convert string to bytes if needed
    plaintext = data.encode("utf-8") if isinstance(data, str) else data

    try:
        # Generate random IV (12 bytes for GCM)
        iv = os.urandom(12)

        if HAS_RUST_CORE:
            # 2026 Optimization: Native PyO3 Module
            final_ciphertext = crypto_lib_rs.aes_gcm_encrypt(plaintext, key, iv)
        else:
            # Fallback to pure python cryptography
            cipher = Cipher(algorithms.AES(key), modes.GCM(iv), backend=default_backend())
            encryptor = cipher.encryptor()
            ciphertext = encryptor.update(plaintext) + encryptor.finalize()
            final_ciphertext = ciphertext + encryptor.tag

        return EncryptionResult(
            ciphertext=final_ciphertext,
            iv=iv,
            algorithm=algorithm
        )

    except Exception as exc:
        msg = f"Encryption failed: {exc}"
        raise EncryptionError(msg, algorithm=algorithm) from exc


def decrypt(
    encrypted_data: EncryptionResult | str,
    key: bytes
) -> DecryptionResult:
    """Pure function to decrypt data with given key.

    Args:
        encrypted_data: EncryptionResult object or base64 encoded string
        key: Decryption key

    Returns:
        DecryptionResult object containing plaintext and metadata

    Raises:
        CryptoError: On decryption failure
        InvalidKeyError: If key is invalid
        DecryptionError: If decryption fails (wrong key, corrupted data, etc.)

    """
    # Parse encrypted data
    if isinstance(encrypted_data, str):
        result = _parse_encrypted_string(encrypted_data)
    elif isinstance(encrypted_data, EncryptionResult):
        result = encrypted_data
    else:
        msg = "encrypted_data must be EncryptionResult or base64 string"
        raise CryptoError(msg)

    if result.algorithm != "AES-256-GCM":
        msg = f"Unsupported algorithm: {result.algorithm}"
        raise CryptoError(msg)

    if len(key) != AES_256_KEY_SIZE:
        msg = f"AES-256 requires 32-byte key, got {len(key)} bytes"
        raise InvalidKeyError(msg)

    # Split ciphertext and auth tag (last 16 bytes)
    if len(result.ciphertext) < GCM_AUTH_TAG_SIZE:
        msg = "Ciphertext too short to contain auth tag"
        raise DecryptionError(msg)

    try:
        ciphertext = result.ciphertext[:-GCM_AUTH_TAG_SIZE]
        auth_tag = result.ciphertext[-GCM_AUTH_TAG_SIZE:]

        if HAS_RUST_CORE:
            # 2026 Optimization: Native PyO3 Module
            # rust module takes concatenated ciphertext + auth_tag
            plaintext = crypto_lib_rs.aes_gcm_decrypt(result.ciphertext, key, result.iv)
        else:
            # Fallback to pure python cryptography
            cipher = Cipher(algorithms.AES(key), modes.GCM(result.iv, auth_tag), backend=default_backend())
            decryptor = cipher.decryptor()
            plaintext = decryptor.update(ciphertext) + decryptor.finalize()

        return DecryptionResult(
            plaintext=plaintext,
            algorithm=result.algorithm,
            iv=result.iv,
            salt=result.salt
        )

    except Exception as exc:
        msg = f"Decryption failed: {exc}"
        raise DecryptionError(msg) from exc


def encrypt_aad(
    data: str | bytes,
    key: bytes,
    aad: bytes,
    algorithm: str = "AES-256-GCM",
) -> EncryptionResult:
    """Encrypt data with Additional Authenticated Data (AAD).

    AAD is authenticated but not encrypted, binding context like tier labels
    to the ciphertext so that decryption with a different AAD will fail.

    Args:
        data: Data to encrypt (string or bytes)
        key: Encryption key (must be 32 bytes for AES-256)
        aad: Additional authenticated data (e.g. tier name as bytes)
        algorithm: Encryption algorithm (default: AES-256-GCM)

    Returns:
        EncryptionResult object containing ciphertext, IV, and metadata

    """
    if algorithm != "AES-256-GCM":
        msg = f"Unsupported algorithm: {algorithm}"
        raise EncryptionError(msg, algorithm=algorithm)

    if len(key) != AES_256_KEY_SIZE:
        msg = f"AES-256 requires 32-byte key, got {len(key)} bytes"
        raise InvalidKeyError(
            msg, key_type="AES-256", expected_size=32, actual_size=len(key),
        )

    plaintext = data.encode("utf-8") if isinstance(data, str) else data

    try:
        iv = os.urandom(12)

        if HAS_RUST_CORE:
            final_ciphertext = crypto_lib_rs.aes_gcm_encrypt_aad(plaintext, key, iv, aad)
        else:
            cipher = Cipher(algorithms.AES(key), modes.GCM(iv), backend=default_backend())
            encryptor = cipher.encryptor()
            encryptor.authenticate_additional_data(aad)
            ciphertext = encryptor.update(plaintext) + encryptor.finalize()
            final_ciphertext = ciphertext + encryptor.tag

        return EncryptionResult(ciphertext=final_ciphertext, iv=iv, algorithm=algorithm)

    except Exception as exc:
        msg = f"Encryption failed: {exc}"
        raise EncryptionError(msg, algorithm=algorithm) from exc


def decrypt_aad(
    encrypted_data: EncryptionResult | str,
    key: bytes,
    aad: bytes,
) -> DecryptionResult:
    """Decrypt data with Additional Authenticated Data (AAD).

    The AAD must match the value used during encryption. If it doesn't,
    decryption will fail with a DecryptionError.

    Args:
        encrypted_data: EncryptionResult object or base64 encoded string
        key: Decryption key
        aad: Additional authenticated data (must match encryption AAD)

    Returns:
        DecryptionResult object containing plaintext and metadata

    """
    if isinstance(encrypted_data, str):
        result = _parse_encrypted_string(encrypted_data)
    elif isinstance(encrypted_data, EncryptionResult):
        result = encrypted_data
    else:
        msg = "encrypted_data must be EncryptionResult or base64 string"
        raise CryptoError(msg)

    if result.algorithm != "AES-256-GCM":
        msg = f"Unsupported algorithm: {result.algorithm}"
        raise CryptoError(msg)

    if len(key) != AES_256_KEY_SIZE:
        msg = f"AES-256 requires 32-byte key, got {len(key)} bytes"
        raise InvalidKeyError(msg)

    if len(result.ciphertext) < GCM_AUTH_TAG_SIZE:
        msg = "Ciphertext too short to contain auth tag"
        raise DecryptionError(msg)

    try:
        ciphertext = result.ciphertext[:-GCM_AUTH_TAG_SIZE]
        auth_tag = result.ciphertext[-GCM_AUTH_TAG_SIZE:]

        if HAS_RUST_CORE:
            plaintext = crypto_lib_rs.aes_gcm_decrypt_aad(result.ciphertext, key, result.iv, aad)
        else:
            cipher = Cipher(
                algorithms.AES(key), modes.GCM(result.iv, auth_tag), backend=default_backend(),
            )
            decryptor = cipher.decryptor()
            decryptor.authenticate_additional_data(aad)
            plaintext = decryptor.update(ciphertext) + decryptor.finalize()

        return DecryptionResult(
            plaintext=plaintext, algorithm=result.algorithm, iv=result.iv, salt=result.salt,
        )

    except Exception as exc:
        msg = f"Decryption failed: {exc}"
        raise DecryptionError(msg) from exc


def _parse_encrypted_string(encoded: str) -> EncryptionResult:
    """Parse base64 encoded encryption result string."""
    parts = encoded.split(":")
    if len(parts) != ENCRYPTED_PARTS:
        msg = "Invalid encrypted string format"
        raise CryptoError(msg)

    try:
        algorithm, iv_b64, salt_b64, ciphertext_b64 = parts

        iv = base64.b64decode(iv_b64)
        ciphertext = base64.b64decode(ciphertext_b64)

        salt = None
        if salt_b64:  # salt is optional (empty string if not present)
            salt = base64.b64decode(salt_b64)

        return EncryptionResult(
            ciphertext=ciphertext,
            iv=iv,
            salt=salt,
            algorithm=algorithm
        )

    except Exception as exc:
        msg = f"Failed to parse encrypted string: {exc}"
        raise CryptoError(msg) from exc
