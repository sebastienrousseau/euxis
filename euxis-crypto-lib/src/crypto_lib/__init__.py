# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Euxis Crypto Library - Pure Functional Encryption Operations.

This module provides pure functional cryptographic operations without filesystem dependencies.
All operations return structured EncryptionResult objects for safe handling.

Performance features:
- Async operations with thread pool for high TPS (>=1000)
- Cipher context caching for reduced initialization overhead
- Batch encryption/decryption for bulk operations
- Target: response time <=10ms
"""

from .core import DecryptionResult, EncryptionResult, decrypt, encrypt
from .exceptions import CryptoError, DecryptionError, InvalidKeyError
from .key_management import derive_key, generate_key

# Lazy load async module to avoid import overhead for sync-only usage
_async_module = None

def _get_async():
    global _async_module
    if _async_module is None:
        from . import async_core as _async_module
    return _async_module

# Async function proxies
def async_encrypt(*args, **kwargs):
    """Async encryption - see async_core.async_encrypt for details."""
    return _get_async().async_encrypt(*args, **kwargs)

def async_decrypt(*args, **kwargs):
    """Async decryption - see async_core.async_decrypt for details."""
    return _get_async().async_decrypt(*args, **kwargs)

def async_encrypt_batch(*args, **kwargs):
    """Batch async encryption - see async_core.async_encrypt_batch for details."""
    return _get_async().async_encrypt_batch(*args, **kwargs)

def async_decrypt_batch(*args, **kwargs):
    """Batch async decryption - see async_core.async_decrypt_batch for details."""
    return _get_async().async_decrypt_batch(*args, **kwargs)

__version__ = "0.1.0"
__all__ = [
    # Sync API
    "CryptoError",
    "DecryptionError",
    "DecryptionResult",
    "EncryptionResult",
    "InvalidKeyError",
    "decrypt",
    "derive_key",
    "encrypt",
    "generate_key",
    # Async API (high-performance)
    "async_encrypt",
    "async_decrypt",
    "async_encrypt_batch",
    "async_decrypt_batch",
]
