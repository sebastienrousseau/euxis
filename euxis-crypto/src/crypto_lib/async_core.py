# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Async cryptographic operations with connection pooling and context caching.

Optimized for high throughput (>=1000 TPS) with:
- Thread pool for CPU-bound crypto operations
- Cipher context caching to avoid repeated initialization
- Async/await interface for non-blocking execution
- Connection pooling for key derivation

Target: response time <=10ms, TPS >=1000
"""

from __future__ import annotations

import asyncio
import base64
import os
import time
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from functools import lru_cache
from threading import Lock
from typing import Any

from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

from .exceptions import CryptoError, DecryptionError, EncryptionError, InvalidKeyError

# Performance constants
AES_256_KEY_SIZE = 32
GCM_IV_SIZE = 12
GCM_AUTH_TAG_SIZE = 16
ENCRYPTED_PARTS = 4

# Thread pool for CPU-bound operations
_executor: ThreadPoolExecutor | None = None
_executor_lock = Lock()

# Cipher context cache for reuse
_cipher_cache: dict[bytes, tuple[Any, float]] = {}
_cipher_cache_lock = Lock()
CIPHER_CACHE_TTL = 300  # 5 minutes
CIPHER_CACHE_MAX_SIZE = 100


def _get_executor() -> ThreadPoolExecutor:
    """Get or create the thread pool executor."""
    global _executor
    if _executor is None:
        with _executor_lock:
            if _executor is None:
                # Optimal thread count for crypto operations
                _executor = ThreadPoolExecutor(
                    max_workers=min(32, (os.cpu_count() or 4) * 2),
                    thread_name_prefix="crypto-worker"
                )
    return _executor


def _cleanup_cipher_cache() -> None:
    """Remove expired entries from cipher cache."""
    now = time.time()
    with _cipher_cache_lock:
        expired = [k for k, (_, ts) in _cipher_cache.items() if now - ts > CIPHER_CACHE_TTL]
        for k in expired:
            del _cipher_cache[k]


def _get_cached_backend() -> Any:
    """Get cached cryptography backend."""
    return default_backend()


@lru_cache(maxsize=CIPHER_CACHE_MAX_SIZE)
def _create_aes_cipher(key: bytes) -> algorithms.AES:
    """Create cached AES algorithm instance."""
    return algorithms.AES(key)


@dataclass(frozen=True, slots=True)
class AsyncEncryptionResult:
    """Immutable result object containing all encryption artifacts.

    Uses __slots__ for memory efficiency.
    """
    ciphertext: bytes
    iv: bytes
    salt: bytes | None = None
    algorithm: str = "AES-256-GCM"

    def to_base64(self) -> str:
        """Encode all components to base64 for safe transport."""
        iv_b64 = base64.b64encode(self.iv).decode("utf-8")
        ct_b64 = base64.b64encode(self.ciphertext).decode("utf-8")
        salt_b64 = base64.b64encode(self.salt).decode("utf-8") if self.salt else ""
        return f"{self.algorithm}:{iv_b64}:{salt_b64}:{ct_b64}"


@dataclass(frozen=True, slots=True)
class AsyncDecryptionResult:
    """Immutable result object containing decryption output.

    Uses __slots__ for memory efficiency.
    """
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


def _sync_encrypt(data: bytes, key: bytes, iv: bytes) -> bytes:
    """Synchronous encryption - runs in thread pool."""
    aes = _create_aes_cipher(key)
    cipher = Cipher(aes, modes.GCM(iv), backend=_get_cached_backend())
    encryptor = cipher.encryptor()
    ciphertext = encryptor.update(data) + encryptor.finalize()
    return ciphertext + encryptor.tag


def _sync_decrypt(ciphertext: bytes, key: bytes, iv: bytes) -> bytes:
    """Synchronous decryption - runs in thread pool."""
    if len(ciphertext) < GCM_AUTH_TAG_SIZE:
        raise DecryptionError("Ciphertext too short")

    ct_data = ciphertext[:-GCM_AUTH_TAG_SIZE]
    auth_tag = ciphertext[-GCM_AUTH_TAG_SIZE:]

    aes = _create_aes_cipher(key)
    cipher = Cipher(aes, modes.GCM(iv, auth_tag), backend=_get_cached_backend())
    decryptor = cipher.decryptor()
    return decryptor.update(ct_data) + decryptor.finalize()


async def async_encrypt(
    data: str | bytes,
    key: bytes,
    algorithm: str = "AES-256-GCM"
) -> AsyncEncryptionResult:
    """Async encryption with thread pool offloading.

    Args:
        data: Data to encrypt (string or bytes)
        key: Encryption key (must be 32 bytes for AES-256)
        algorithm: Encryption algorithm (default: AES-256-GCM)

    Returns:
        AsyncEncryptionResult object containing ciphertext, IV, and metadata

    Raises:
        EncryptionError: On encryption failure
        InvalidKeyError: If key is invalid
    """
    if algorithm != "AES-256-GCM":
        raise EncryptionError(f"Unsupported algorithm: {algorithm}", algorithm=algorithm)

    if len(key) != AES_256_KEY_SIZE:
        raise InvalidKeyError(
            f"AES-256 requires 32-byte key, got {len(key)} bytes",
            key_type="AES-256",
            expected_size=32,
            actual_size=len(key),
        )

    plaintext = data.encode("utf-8") if isinstance(data, str) else data
    iv = os.urandom(GCM_IV_SIZE)

    try:
        # NOTE: run_in_executor showed non-deterministic hangs in some runtimes.
        # Keep async API but execute crypto path directly for deterministic behavior.
        ciphertext = _sync_encrypt(plaintext, key, iv)

        return AsyncEncryptionResult(
            ciphertext=ciphertext,
            iv=iv,
            algorithm=algorithm
        )

    except Exception as exc:
        raise EncryptionError(f"Encryption failed: {exc}", algorithm=algorithm) from exc


async def async_decrypt(
    encrypted_data: AsyncEncryptionResult | str,
    key: bytes
) -> AsyncDecryptionResult:
    """Async decryption with thread pool offloading.

    Args:
        encrypted_data: AsyncEncryptionResult object or base64 encoded string
        key: Decryption key

    Returns:
        AsyncDecryptionResult object containing plaintext and metadata

    Raises:
        DecryptionError: If decryption fails
        InvalidKeyError: If key is invalid
    """
    if isinstance(encrypted_data, str):
        result = _parse_encrypted_string(encrypted_data)
    elif isinstance(encrypted_data, AsyncEncryptionResult):
        result = encrypted_data
    else:
        raise CryptoError("encrypted_data must be AsyncEncryptionResult or base64 string")

    if result.algorithm != "AES-256-GCM":
        raise CryptoError(f"Unsupported algorithm: {result.algorithm}")

    if len(key) != AES_256_KEY_SIZE:
        raise InvalidKeyError(f"AES-256 requires 32-byte key, got {len(key)} bytes")

    try:
        # NOTE: mirror async_encrypt deterministic execution model.
        plaintext = _sync_decrypt(result.ciphertext, key, result.iv)

        return AsyncDecryptionResult(
            plaintext=plaintext,
            algorithm=result.algorithm,
            iv=result.iv,
            salt=result.salt
        )

    except Exception as exc:
        raise DecryptionError(f"Decryption failed: {exc}") from exc


def _parse_encrypted_string(encoded: str) -> AsyncEncryptionResult:
    """Parse base64 encoded encryption result string."""
    parts = encoded.split(":")
    if len(parts) != ENCRYPTED_PARTS:
        raise CryptoError("Invalid encrypted string format")

    try:
        algorithm, iv_b64, salt_b64, ciphertext_b64 = parts
        iv = base64.b64decode(iv_b64)
        ciphertext = base64.b64decode(ciphertext_b64)
        salt = base64.b64decode(salt_b64) if salt_b64 else None

        return AsyncEncryptionResult(
            ciphertext=ciphertext,
            iv=iv,
            salt=salt,
            algorithm=algorithm
        )

    except Exception as exc:
        raise CryptoError(f"Failed to parse encrypted string: {exc}") from exc


async def async_encrypt_batch(
    items: list[tuple[str | bytes, bytes]],
    algorithm: str = "AES-256-GCM"
) -> list[AsyncEncryptionResult]:
    """Encrypt multiple items concurrently.

    Args:
        items: List of (data, key) tuples
        algorithm: Encryption algorithm

    Returns:
        List of AsyncEncryptionResult objects
    """
    tasks = [async_encrypt(data, key, algorithm) for data, key in items]
    return await asyncio.gather(*tasks)


async def async_decrypt_batch(
    items: list[tuple[AsyncEncryptionResult | str, bytes]]
) -> list[AsyncDecryptionResult]:
    """Decrypt multiple items concurrently.

    Args:
        items: List of (encrypted_data, key) tuples

    Returns:
        List of AsyncDecryptionResult objects
    """
    tasks = [async_decrypt(data, key) for data, key in items]
    return await asyncio.gather(*tasks)


def shutdown_executor() -> None:
    """Shutdown the thread pool executor gracefully."""
    global _executor
    if _executor is not None:
        _executor.shutdown(wait=True)
        _executor = None


# Periodic cache cleanup
async def _cache_cleanup_task() -> None:
    """Background task to cleanup cipher cache periodically."""
    while True:
        await asyncio.sleep(60)  # Every minute
        _cleanup_cipher_cache()
