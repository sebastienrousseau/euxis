# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Unit tests for crypto_lib.async_core — covering missing lines for >=95% coverage.

Covers:
- _get_executor: singleton creation
- _cleanup_cipher_cache: expired entry removal
- _get_cached_backend, _create_aes_cipher
- AsyncEncryptionResult.to_base64
- AsyncDecryptionResult.to_string (success and UnicodeDecodeError)
- _sync_encrypt and _sync_decrypt
- async_encrypt: success, unsupported algorithm, invalid key, internal error
- async_decrypt: from result object, from base64 string, invalid type, unsupported algo, invalid key, decrypt failure
- _parse_encrypted_string: valid, invalid format, malformed base64
- async_encrypt_batch and async_decrypt_batch
- shutdown_executor
- _cache_cleanup_task
"""

from __future__ import annotations

import asyncio
import base64
import os
import time
from unittest.mock import patch, MagicMock

import pytest

from crypto_lib.async_core import (
    AES_256_KEY_SIZE,
    CIPHER_CACHE_MAX_SIZE,
    CIPHER_CACHE_TTL,
    AsyncDecryptionResult,
    AsyncEncryptionResult,
    GCM_AUTH_TAG_SIZE,
    GCM_IV_SIZE,
    _cache_cleanup_task,
    _cleanup_cipher_cache,
    _create_aes_cipher,
    _get_cached_backend,
    _get_executor,
    _parse_encrypted_string,
    _sync_decrypt,
    _sync_encrypt,
    async_decrypt,
    async_decrypt_batch,
    async_encrypt,
    async_encrypt_batch,
    shutdown_executor,
)
from crypto_lib.exceptions import (
    CryptoError,
    DecryptionError,
    EncryptionError,
    InvalidKeyError,
)

# ============================================================================
# Helper
# ============================================================================

def _random_key() -> bytes:
    """Generate a random 32-byte key for AES-256."""
    return os.urandom(AES_256_KEY_SIZE)


# ============================================================================
# _get_executor
# ============================================================================


class TestGetExecutor:
    """Tests for _get_executor singleton."""

    def test_returns_executor(self):
        """Should return a ThreadPoolExecutor."""
        from concurrent.futures import ThreadPoolExecutor
        executor = _get_executor()
        assert isinstance(executor, ThreadPoolExecutor)

    def test_returns_same_instance(self):
        """Subsequent calls should return the same executor."""
        e1 = _get_executor()
        e2 = _get_executor()
        assert e1 is e2


# ============================================================================
# _cleanup_cipher_cache
# ============================================================================


class TestCleanupCipherCache:
    """Tests for _cleanup_cipher_cache."""

    def test_removes_expired_entries(self):
        """Expired entries should be removed from cache."""
        import crypto_lib.async_core as ac
        # Save original and set up test cache
        original_cache = ac._cipher_cache.copy()
        try:
            ac._cipher_cache.clear()
            expired_key = b"expired_key_1234567890123456789012"
            fresh_key = b"fresh_key_12345678901234567890123"
            ac._cipher_cache[expired_key] = (MagicMock(), time.time() - CIPHER_CACHE_TTL - 10)
            ac._cipher_cache[fresh_key] = (MagicMock(), time.time())

            _cleanup_cipher_cache()

            assert expired_key not in ac._cipher_cache
            assert fresh_key in ac._cipher_cache
        finally:
            ac._cipher_cache.clear()
            ac._cipher_cache.update(original_cache)

    def test_empty_cache(self):
        """Cleaning empty cache should not error."""
        import crypto_lib.async_core as ac
        original = ac._cipher_cache.copy()
        try:
            ac._cipher_cache.clear()
            _cleanup_cipher_cache()
            assert len(ac._cipher_cache) == 0
        finally:
            ac._cipher_cache.clear()
            ac._cipher_cache.update(original)


# ============================================================================
# _get_cached_backend / _create_aes_cipher
# ============================================================================


class TestCachedHelpers:
    """Tests for cached backend and cipher helpers."""

    def test_get_cached_backend(self):
        """Should return a backend object."""
        backend = _get_cached_backend()
        assert backend is not None

    def test_create_aes_cipher(self):
        """Should return an AES algorithm object."""
        from cryptography.hazmat.primitives.ciphers import algorithms
        key = _random_key()
        aes = _create_aes_cipher(key)
        assert isinstance(aes, algorithms.AES)

    def test_create_aes_cipher_cached(self):
        """Same key should return same cached AES object."""
        key = _random_key()
        a1 = _create_aes_cipher(key)
        a2 = _create_aes_cipher(key)
        assert a1 is a2


# ============================================================================
# AsyncEncryptionResult
# ============================================================================


class TestAsyncEncryptionResult:
    """Tests for AsyncEncryptionResult."""

    def test_construction(self):
        """Should store all fields."""
        result = AsyncEncryptionResult(
            ciphertext=b"ct", iv=b"iv", salt=b"salt",
            algorithm="AES-256-GCM",
        )
        assert result.ciphertext == b"ct"
        assert result.iv == b"iv"
        assert result.salt == b"salt"
        assert result.algorithm == "AES-256-GCM"

    def test_to_base64_with_salt(self):
        """to_base64 with salt should produce 4-part string."""
        iv = os.urandom(GCM_IV_SIZE)
        ct = os.urandom(32)
        salt = os.urandom(16)
        result = AsyncEncryptionResult(ciphertext=ct, iv=iv, salt=salt)
        b64_str = result.to_base64()

        parts = b64_str.split(":")
        assert len(parts) == 4
        assert parts[0] == "AES-256-GCM"
        assert base64.b64decode(parts[1]) == iv
        assert base64.b64decode(parts[2]) == salt
        assert base64.b64decode(parts[3]) == ct

    def test_to_base64_without_salt(self):
        """to_base64 without salt should produce empty salt part."""
        iv = os.urandom(GCM_IV_SIZE)
        ct = os.urandom(32)
        result = AsyncEncryptionResult(ciphertext=ct, iv=iv)
        b64_str = result.to_base64()

        parts = b64_str.split(":")
        assert len(parts) == 4
        assert parts[2] == ""  # empty salt


# ============================================================================
# AsyncDecryptionResult
# ============================================================================


class TestAsyncDecryptionResult:
    """Tests for AsyncDecryptionResult."""

    def test_to_string_utf8(self):
        """to_string should decode plaintext as UTF-8."""
        result = AsyncDecryptionResult(
            plaintext=b"hello world",
            algorithm="AES-256-GCM",
            iv=b"test_iv_1234",
        )
        assert result.to_string() == "hello world"

    def test_to_string_custom_encoding(self):
        """to_string with custom encoding should work."""
        result = AsyncDecryptionResult(
            plaintext="hello".encode("ascii"),
            algorithm="AES-256-GCM",
            iv=b"test_iv_1234",
        )
        assert result.to_string("ascii") == "hello"

    def test_to_string_unicode_error(self):
        """to_string with invalid encoding should raise CryptoError."""
        result = AsyncDecryptionResult(
            plaintext=b"\xff\xfe",
            algorithm="AES-256-GCM",
            iv=b"test_iv_1234",
        )
        with pytest.raises(CryptoError, match="Failed to decode"):
            result.to_string("ascii")


# ============================================================================
# _sync_encrypt / _sync_decrypt
# ============================================================================


class TestSyncCrypto:
    """Tests for _sync_encrypt and _sync_decrypt."""

    def test_encrypt_decrypt_round_trip(self):
        """Encrypt then decrypt should return original plaintext."""
        key = _random_key()
        iv = os.urandom(GCM_IV_SIZE)
        plaintext = b"hello world, this is a secret message"

        ciphertext = _sync_encrypt(plaintext, key, iv)
        assert ciphertext != plaintext
        assert len(ciphertext) == len(plaintext) + GCM_AUTH_TAG_SIZE

        decrypted = _sync_decrypt(ciphertext, key, iv)
        assert decrypted == plaintext

    def test_decrypt_short_ciphertext(self):
        """Decrypting ciphertext shorter than auth tag should raise."""
        key = _random_key()
        iv = os.urandom(GCM_IV_SIZE)
        with pytest.raises(DecryptionError, match="too short"):
            _sync_decrypt(b"short", key, iv)

    def test_decrypt_wrong_key(self):
        """Decrypting with wrong key should raise (auth tag mismatch)."""
        key1 = _random_key()
        key2 = _random_key()
        iv = os.urandom(GCM_IV_SIZE)
        ciphertext = _sync_encrypt(b"secret", key1, iv)

        with pytest.raises(Exception):
            _sync_decrypt(ciphertext, key2, iv)

    def test_decrypt_wrong_iv(self):
        """Decrypting with wrong IV should raise (auth tag mismatch)."""
        key = _random_key()
        iv1 = os.urandom(GCM_IV_SIZE)
        iv2 = os.urandom(GCM_IV_SIZE)
        ciphertext = _sync_encrypt(b"secret", key, iv1)

        with pytest.raises(Exception):
            _sync_decrypt(ciphertext, key, iv2)


# ============================================================================
# async_encrypt
# ============================================================================


class TestAsyncEncrypt:
    """Tests for async_encrypt."""

    @pytest.mark.asyncio
    async def test_encrypt_string(self):
        """Should encrypt a string and return AsyncEncryptionResult."""
        key = _random_key()
        result = await async_encrypt("hello world", key)
        assert isinstance(result, AsyncEncryptionResult)
        assert result.algorithm == "AES-256-GCM"
        assert len(result.iv) == GCM_IV_SIZE
        assert len(result.ciphertext) > 0

    @pytest.mark.asyncio
    async def test_encrypt_bytes(self):
        """Should encrypt bytes data."""
        key = _random_key()
        result = await async_encrypt(b"binary data", key)
        assert isinstance(result, AsyncEncryptionResult)

    @pytest.mark.asyncio
    async def test_encrypt_unsupported_algorithm(self):
        """Unsupported algorithm should raise EncryptionError."""
        key = _random_key()
        with pytest.raises(EncryptionError, match="Unsupported algorithm"):
            await async_encrypt("data", key, algorithm="DES-CBC")

    @pytest.mark.asyncio
    async def test_encrypt_invalid_key_size(self):
        """Invalid key size should raise InvalidKeyError."""
        key = b"short"
        with pytest.raises(InvalidKeyError, match="32-byte key"):
            await async_encrypt("data", key)

    @pytest.mark.asyncio
    async def test_encrypt_internal_error(self):
        """Internal encryption error should raise EncryptionError."""
        key = _random_key()
        with patch("crypto_lib.async_core._sync_encrypt", side_effect=RuntimeError("boom")):
            with pytest.raises(EncryptionError, match="Encryption failed"):
                await async_encrypt("data", key)


# ============================================================================
# async_decrypt
# ============================================================================


class TestAsyncDecrypt:
    """Tests for async_decrypt."""

    @pytest.mark.asyncio
    async def test_decrypt_from_result_object(self):
        """Should decrypt AsyncEncryptionResult object."""
        key = _random_key()
        encrypted = await async_encrypt("hello world", key)
        decrypted = await async_decrypt(encrypted, key)
        assert isinstance(decrypted, AsyncDecryptionResult)
        assert decrypted.plaintext == b"hello world"
        assert decrypted.algorithm == "AES-256-GCM"
        assert decrypted.iv == encrypted.iv

    @pytest.mark.asyncio
    async def test_decrypt_from_base64_string(self):
        """Should decrypt from base64-encoded string."""
        key = _random_key()
        encrypted = await async_encrypt("secret message", key)
        b64_str = encrypted.to_base64()
        decrypted = await async_decrypt(b64_str, key)
        assert decrypted.plaintext == b"secret message"

    @pytest.mark.asyncio
    async def test_decrypt_invalid_type(self):
        """Invalid encrypted_data type should raise CryptoError."""
        key = _random_key()
        with pytest.raises(CryptoError, match="must be"):
            await async_decrypt(12345, key)

    @pytest.mark.asyncio
    async def test_decrypt_unsupported_algorithm(self):
        """Unsupported algorithm in result should raise CryptoError."""
        key = _random_key()
        result = AsyncEncryptionResult(
            ciphertext=b"data", iv=b"iv" * 6,
            algorithm="UNSUPPORTED",
        )
        with pytest.raises(CryptoError, match="Unsupported"):
            await async_decrypt(result, key)

    @pytest.mark.asyncio
    async def test_decrypt_invalid_key_size(self):
        """Invalid key size should raise InvalidKeyError."""
        key = b"short"
        encrypted = await async_encrypt("data", _random_key())
        with pytest.raises(InvalidKeyError, match="32-byte key"):
            await async_decrypt(encrypted, key)

    @pytest.mark.asyncio
    async def test_decrypt_failure_wrong_key(self):
        """Decryption with wrong key should raise DecryptionError."""
        key1 = _random_key()
        key2 = _random_key()
        encrypted = await async_encrypt("secret", key1)
        with pytest.raises(DecryptionError, match="Decryption failed"):
            await async_decrypt(encrypted, key2)

    @pytest.mark.asyncio
    async def test_decrypt_with_salt_in_result(self):
        """Decryption result should propagate salt from encryption result."""
        key = _random_key()
        salt = os.urandom(16)
        encrypted = await async_encrypt("data", key)
        # Manually set salt
        encrypted_with_salt = AsyncEncryptionResult(
            ciphertext=encrypted.ciphertext,
            iv=encrypted.iv,
            salt=salt,
            algorithm=encrypted.algorithm,
        )
        decrypted = await async_decrypt(encrypted_with_salt, key)
        assert decrypted.salt == salt


# ============================================================================
# _parse_encrypted_string
# ============================================================================


class TestParseEncryptedString:
    """Tests for _parse_encrypted_string."""

    def test_valid_format(self):
        """Valid 4-part base64 string should parse correctly."""
        iv = os.urandom(GCM_IV_SIZE)
        ct = os.urandom(32)
        salt = os.urandom(16)
        iv_b64 = base64.b64encode(iv).decode()
        ct_b64 = base64.b64encode(ct).decode()
        salt_b64 = base64.b64encode(salt).decode()
        encoded = f"AES-256-GCM:{iv_b64}:{salt_b64}:{ct_b64}"

        result = _parse_encrypted_string(encoded)
        assert result.algorithm == "AES-256-GCM"
        assert result.iv == iv
        assert result.ciphertext == ct
        assert result.salt == salt

    def test_valid_format_no_salt(self):
        """4-part string with empty salt should parse with salt=None."""
        iv = os.urandom(GCM_IV_SIZE)
        ct = os.urandom(32)
        iv_b64 = base64.b64encode(iv).decode()
        ct_b64 = base64.b64encode(ct).decode()
        encoded = f"AES-256-GCM:{iv_b64}::{ct_b64}"

        result = _parse_encrypted_string(encoded)
        assert result.salt is None

    def test_invalid_format_wrong_parts(self):
        """String with wrong number of parts should raise CryptoError."""
        with pytest.raises(CryptoError, match="Invalid encrypted string"):
            _parse_encrypted_string("part1:part2")

    def test_invalid_format_too_many_parts(self):
        """String with too many parts should raise CryptoError."""
        with pytest.raises(CryptoError, match="Invalid encrypted string"):
            _parse_encrypted_string("a:b:c:d:e")

    def test_malformed_base64(self):
        """Malformed base64 should raise CryptoError."""
        with pytest.raises(CryptoError, match="Failed to parse"):
            _parse_encrypted_string("AES-256-GCM:!!!invalid:salt:ct")


# ============================================================================
# async_encrypt_batch / async_decrypt_batch
# ============================================================================


class TestBatchOperations:
    """Tests for batch encrypt/decrypt operations."""

    @pytest.mark.asyncio
    async def test_encrypt_batch(self):
        """async_encrypt_batch should encrypt multiple items concurrently."""
        key1 = _random_key()
        key2 = _random_key()
        items = [("message one", key1), ("message two", key2)]
        results = await async_encrypt_batch(items)
        assert len(results) == 2
        assert all(isinstance(r, AsyncEncryptionResult) for r in results)

    @pytest.mark.asyncio
    async def test_encrypt_batch_empty(self):
        """async_encrypt_batch with empty list should return empty list."""
        results = await async_encrypt_batch([])
        assert results == []

    @pytest.mark.asyncio
    async def test_decrypt_batch(self):
        """async_decrypt_batch should decrypt multiple items concurrently."""
        key1 = _random_key()
        key2 = _random_key()
        enc1 = await async_encrypt("msg1", key1)
        enc2 = await async_encrypt("msg2", key2)
        items = [(enc1, key1), (enc2, key2)]
        results = await async_decrypt_batch(items)
        assert len(results) == 2
        assert results[0].plaintext == b"msg1"
        assert results[1].plaintext == b"msg2"

    @pytest.mark.asyncio
    async def test_decrypt_batch_from_strings(self):
        """async_decrypt_batch should accept base64 strings."""
        key = _random_key()
        enc = await async_encrypt("hello", key)
        b64_str = enc.to_base64()
        results = await async_decrypt_batch([(b64_str, key)])
        assert results[0].plaintext == b"hello"


# ============================================================================
# shutdown_executor
# ============================================================================


class TestShutdownExecutor:
    """Tests for shutdown_executor."""

    def test_shutdown_active_executor(self):
        """shutdown_executor should shutdown and None-ify the executor."""
        import crypto_lib.async_core as ac
        # Ensure executor exists
        _get_executor()
        assert ac._executor is not None

        shutdown_executor()
        assert ac._executor is None

    def test_shutdown_already_none(self):
        """shutdown_executor when already None should be a no-op."""
        import crypto_lib.async_core as ac
        ac._executor = None
        shutdown_executor()
        assert ac._executor is None


# ============================================================================
# _cache_cleanup_task
# ============================================================================


class TestCacheCleanupTask:
    """Tests for _cache_cleanup_task background coroutine."""

    @pytest.mark.asyncio
    async def test_cache_cleanup_task_runs(self):
        """_cache_cleanup_task should run cleanup periodically."""
        import crypto_lib.async_core as ac
        original = ac._cipher_cache.copy()
        try:
            ac._cipher_cache.clear()
            # Add an expired entry
            expired_key = b"expired_key_1234567890123456789012"
            ac._cipher_cache[expired_key] = (MagicMock(), time.time() - CIPHER_CACHE_TTL - 100)

            # Run the task briefly by patching sleep to raise after first iteration
            call_count = 0

            async def mock_sleep(seconds):
                nonlocal call_count
                call_count += 1
                if call_count >= 2:
                    raise asyncio.CancelledError()

            with patch("crypto_lib.async_core.asyncio.sleep", side_effect=mock_sleep):
                with pytest.raises(asyncio.CancelledError):
                    await _cache_cleanup_task()

            # The expired entry should have been cleaned up
            assert expired_key not in ac._cipher_cache
        finally:
            ac._cipher_cache.clear()
            ac._cipher_cache.update(original)


# ============================================================================
# End-to-end round-trip
# ============================================================================


class TestEndToEnd:
    """End-to-end integration tests."""

    @pytest.mark.asyncio
    async def test_full_encrypt_to_base64_decrypt(self):
        """Full cycle: encrypt -> to_base64 -> parse -> decrypt."""
        key = _random_key()
        message = "This is a secret message for end-to-end testing"

        # Encrypt
        encrypted = await async_encrypt(message, key)

        # Serialize to string
        b64_str = encrypted.to_base64()
        assert isinstance(b64_str, str)

        # Decrypt from string
        decrypted = await async_decrypt(b64_str, key)
        assert decrypted.to_string() == message

    @pytest.mark.asyncio
    async def test_encrypt_empty_string(self):
        """Encrypting empty string should work."""
        key = _random_key()
        encrypted = await async_encrypt("", key)
        decrypted = await async_decrypt(encrypted, key)
        assert decrypted.plaintext == b""

    @pytest.mark.asyncio
    async def test_encrypt_large_data(self):
        """Encrypting larger data should work."""
        key = _random_key()
        data = "A" * 10000
        encrypted = await async_encrypt(data, key)
        decrypted = await async_decrypt(encrypted, key)
        assert decrypted.to_string() == data

    @pytest.mark.asyncio
    async def test_encrypt_binary_data(self):
        """Encrypting binary data should work."""
        key = _random_key()
        data = os.urandom(256)
        encrypted = await async_encrypt(data, key)
        decrypted = await async_decrypt(encrypted, key)
        assert decrypted.plaintext == data
