"""
Core cryptographic operations - Pure functional implementation

All functions in this module are pure functions:
- No filesystem access
- No global state modification
- Deterministic output for given inputs (except for IV generation)
- Return structured result objects
"""

import os
import base64
from typing import Union, Optional
from dataclasses import dataclass
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
from cryptography.hazmat.backends import default_backend

from .exceptions import CryptoError, InvalidKeyError, DecryptionError, EncryptionError, ParsingError


@dataclass(frozen=True)
class EncryptionResult:
    """Immutable result object containing all encryption artifacts"""
    ciphertext: bytes
    iv: bytes
    salt: Optional[bytes] = None
    algorithm: str = "AES-256-GCM"

    def to_base64(self) -> str:
        """Encode all components to base64 for safe transport"""
        data = {
            'ciphertext': base64.b64encode(self.ciphertext).decode('utf-8'),
            'iv': base64.b64encode(self.iv).decode('utf-8'),
            'algorithm': self.algorithm
        }
        if self.salt:
            data['salt'] = base64.b64encode(self.salt).decode('utf-8')

        # Simple concatenated format: algorithm:iv:salt:ciphertext (salt optional)
        if self.salt:
            return f"{self.algorithm}:{data['iv']}:{data['salt']}:{data['ciphertext']}"
        else:
            return f"{self.algorithm}:{data['iv']}::{data['ciphertext']}"


@dataclass(frozen=True)
class DecryptionResult:
    """Immutable result object containing decryption output"""
    plaintext: bytes
    algorithm: str
    iv: bytes
    salt: Optional[bytes] = None

    def to_string(self, encoding: str = 'utf-8') -> str:
        """Convert plaintext to string with specified encoding"""
        try:
            return self.plaintext.decode(encoding)
        except UnicodeDecodeError as e:
            raise CryptoError(f"Failed to decode plaintext with {encoding}: {e}")


def encrypt(
    data: Union[str, bytes],
    key: bytes,
    algorithm: str = "AES-256-GCM"
) -> EncryptionResult:
    """
    Pure function to encrypt data with given key.

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
        raise EncryptionError(f"Unsupported algorithm: {algorithm}", algorithm=algorithm)

    if len(key) != 32:
        raise InvalidKeyError(
            f"AES-256 requires 32-byte key, got {len(key)} bytes",
            key_type="AES-256",
            expected_size=32,
            actual_size=len(key)
        )

    # Convert string to bytes if needed
    if isinstance(data, str):
        plaintext = data.encode('utf-8')
    else:
        plaintext = data

    try:
        # Generate random IV (12 bytes for GCM)
        iv = os.urandom(12)

        # Create cipher
        cipher = Cipher(
            algorithms.AES(key),
            modes.GCM(iv),
            backend=default_backend()
        )

        # Encrypt
        encryptor = cipher.encryptor()
        ciphertext = encryptor.update(plaintext) + encryptor.finalize()

        # Get authentication tag
        auth_tag = encryptor.tag

        # Combine ciphertext and auth tag
        final_ciphertext = ciphertext + auth_tag

        return EncryptionResult(
            ciphertext=final_ciphertext,
            iv=iv,
            algorithm=algorithm
        )

    except Exception as e:
        raise EncryptionError(f"Encryption failed: {e}", algorithm=algorithm)


def decrypt(
    encrypted_data: Union[EncryptionResult, str],
    key: bytes
) -> DecryptionResult:
    """
    Pure function to decrypt data with given key.

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
        raise CryptoError("encrypted_data must be EncryptionResult or base64 string")

    if result.algorithm != "AES-256-GCM":
        raise CryptoError(f"Unsupported algorithm: {result.algorithm}")

    if len(key) != 32:
        raise InvalidKeyError(f"AES-256 requires 32-byte key, got {len(key)} bytes")

    try:
        # Split ciphertext and auth tag (last 16 bytes)
        if len(result.ciphertext) < 16:
            raise DecryptionError("Ciphertext too short to contain auth tag")

        ciphertext = result.ciphertext[:-16]
        auth_tag = result.ciphertext[-16:]

        # Create cipher
        cipher = Cipher(
            algorithms.AES(key),
            modes.GCM(result.iv, auth_tag),
            backend=default_backend()
        )

        # Decrypt
        decryptor = cipher.decryptor()
        plaintext = decryptor.update(ciphertext) + decryptor.finalize()

        return DecryptionResult(
            plaintext=plaintext,
            algorithm=result.algorithm,
            iv=result.iv,
            salt=result.salt
        )

    except Exception as e:
        raise DecryptionError(f"Decryption failed: {e}")


def _parse_encrypted_string(encoded: str) -> EncryptionResult:
    """Parse base64 encoded encryption result string"""
    try:
        parts = encoded.split(':')
        if len(parts) != 4:
            raise CryptoError("Invalid encrypted string format")

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

    except Exception as e:
        raise CryptoError(f"Failed to parse encrypted string: {e}")