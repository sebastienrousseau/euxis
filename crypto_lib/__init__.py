"""Euxis Crypto Library - Pure Functional Encryption Operations.

This module provides pure functional cryptographic operations without filesystem dependencies.
All operations return structured EncryptionResult objects for safe handling.
"""

from .core import DecryptionResult, EncryptionResult, decrypt, encrypt
from .exceptions import CryptoError, DecryptionError, InvalidKeyError
from .key_management import derive_key, generate_key

__version__ = "0.0.7"
__all__ = [
    "CryptoError",
    "DecryptionError",
    "DecryptionResult",
    "EncryptionResult",
    "InvalidKeyError",
    "decrypt",
    "derive_key",
    "encrypt",
    "generate_key"
]
