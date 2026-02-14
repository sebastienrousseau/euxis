"""
Euxis Crypto Library - Pure Functional Encryption Operations

This module provides pure functional cryptographic operations without filesystem dependencies.
All operations return structured EncryptionResult objects for safe handling.
"""

from .core import encrypt, decrypt, EncryptionResult, DecryptionResult
from .key_management import generate_key, derive_key
from .exceptions import CryptoError, InvalidKeyError, DecryptionError

__version__ = "0.0.7"
__all__ = [
    "encrypt",
    "decrypt",
    "EncryptionResult",
    "DecryptionResult",
    "generate_key",
    "derive_key",
    "CryptoError",
    "InvalidKeyError",
    "DecryptionError"
]