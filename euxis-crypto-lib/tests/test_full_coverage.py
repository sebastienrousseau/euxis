# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Comprehensive tests to achieve >= 95% coverage across all crypto_lib modules.

Covers missing lines and branches in:
- __init__.py: async proxy functions (_get_async, async_encrypt/decrypt/batch)
- key_management.py: os.urandom failure (L45-47), PBKDF2 derive failure (L103-105)
- core.py: encrypt exception handler (L133-135), parse salt decoding (L221)
- exceptions.py: branch coverage for optional None parameters
"""

from __future__ import annotations

import base64
import os
from unittest.mock import MagicMock, patch

import pytest

# ---------------------------------------------------------------------------
# __init__.py: test lazy-loaded async proxy functions
# ---------------------------------------------------------------------------


class TestInitAsyncProxies:
    """Test the async proxy functions exported from crypto_lib.__init__."""

    def test_get_async_lazy_load(self):
        """_get_async should lazily import async_core and cache it."""
        import crypto_lib

        # Reset the cached module to force a fresh lazy load
        original = crypto_lib._async_module
        try:
            crypto_lib._async_module = None
            mod = crypto_lib._get_async()
            assert mod is not None
            # Second call should return the cached module
            mod2 = crypto_lib._get_async()
            assert mod is mod2
        finally:
            crypto_lib._async_module = original

    def test_async_encrypt_proxy_delegates(self):
        """crypto_lib.async_encrypt should delegate to async_core.async_encrypt."""
        import crypto_lib

        mock_module = MagicMock()
        mock_module.async_encrypt.return_value = "encrypted_sentinel"

        original = crypto_lib._async_module
        try:
            crypto_lib._async_module = mock_module
            result = crypto_lib.async_encrypt("data", b"key")
            mock_module.async_encrypt.assert_called_once_with("data", b"key")
            assert result == "encrypted_sentinel"
        finally:
            crypto_lib._async_module = original

    def test_async_decrypt_proxy_delegates(self):
        """crypto_lib.async_decrypt should delegate to async_core.async_decrypt."""
        import crypto_lib

        mock_module = MagicMock()
        mock_module.async_decrypt.return_value = "decrypted_sentinel"

        original = crypto_lib._async_module
        try:
            crypto_lib._async_module = mock_module
            result = crypto_lib.async_decrypt("data", b"key")
            mock_module.async_decrypt.assert_called_once_with("data", b"key")
            assert result == "decrypted_sentinel"
        finally:
            crypto_lib._async_module = original

    def test_async_encrypt_batch_proxy_delegates(self):
        """crypto_lib.async_encrypt_batch should delegate to async_core."""
        import crypto_lib

        mock_module = MagicMock()
        mock_module.async_encrypt_batch.return_value = ["r1", "r2"]

        original = crypto_lib._async_module
        try:
            crypto_lib._async_module = mock_module
            items = [("a", b"k1"), ("b", b"k2")]
            result = crypto_lib.async_encrypt_batch(items)
            mock_module.async_encrypt_batch.assert_called_once_with(items)
            assert result == ["r1", "r2"]
        finally:
            crypto_lib._async_module = original

    def test_async_decrypt_batch_proxy_delegates(self):
        """crypto_lib.async_decrypt_batch should delegate to async_core."""
        import crypto_lib

        mock_module = MagicMock()
        mock_module.async_decrypt_batch.return_value = ["d1", "d2"]

        original = crypto_lib._async_module
        try:
            crypto_lib._async_module = mock_module
            items = [("enc1", b"k1"), ("enc2", b"k2")]
            result = crypto_lib.async_decrypt_batch(items)
            mock_module.async_decrypt_batch.assert_called_once_with(items)
            assert result == ["d1", "d2"]
        finally:
            crypto_lib._async_module = original

    def test_async_encrypt_proxy_with_kwargs(self):
        """Proxy should forward keyword arguments correctly."""
        import crypto_lib

        mock_module = MagicMock()
        mock_module.async_encrypt.return_value = "result"

        original = crypto_lib._async_module
        try:
            crypto_lib._async_module = mock_module
            crypto_lib.async_encrypt("data", b"key", algorithm="AES-256-GCM")
            mock_module.async_encrypt.assert_called_once_with(
                "data", b"key", algorithm="AES-256-GCM"
            )
        finally:
            crypto_lib._async_module = original

    def test_async_decrypt_proxy_with_kwargs(self):
        """Proxy should forward keyword arguments correctly."""
        import crypto_lib

        mock_module = MagicMock()
        mock_module.async_decrypt.return_value = "result"

        original = crypto_lib._async_module
        try:
            crypto_lib._async_module = mock_module
            crypto_lib.async_decrypt("data", key=b"key")
            mock_module.async_decrypt.assert_called_once_with("data", key=b"key")
        finally:
            crypto_lib._async_module = original


# ---------------------------------------------------------------------------
# key_management.py: test os.urandom and PBKDF2 failure paths
# ---------------------------------------------------------------------------


class TestGenerateKeyOsUrandomFailure:
    """Test generate_key when os.urandom raises an exception (L45-47)."""

    def test_generate_key_urandom_failure(self):
        """generate_key should raise CryptoError if os.urandom fails."""
        from crypto_lib import CryptoError, generate_key

        with patch("crypto_lib.key_management.os.urandom", side_effect=OSError("entropy source unavailable")):
            with pytest.raises(CryptoError, match="Key generation failed"):
                generate_key(32)

    def test_generate_key_urandom_runtime_error(self):
        """generate_key should wrap RuntimeError from os.urandom."""
        from crypto_lib import CryptoError, generate_key

        with patch("crypto_lib.key_management.os.urandom", side_effect=RuntimeError("system error")):
            with pytest.raises(CryptoError, match="Key generation failed"):
                generate_key(16)


class TestDeriveKeyPBKDF2Failure:
    """Test derive_key when PBKDF2 derivation raises an exception (L103-105)."""

    def test_derive_key_kdf_derive_failure(self):
        """derive_key should raise CryptoError if PBKDF2HMAC.derive() fails."""
        from crypto_lib import CryptoError, derive_key

        with patch(
            "crypto_lib.key_management.PBKDF2HMAC",
            side_effect=RuntimeError("PBKDF2 init failed"),
        ):
            with pytest.raises(CryptoError, match="Key derivation failed"):
                derive_key("password", salt=b"salt12345678")

    def test_derive_key_derive_method_failure(self):
        """derive_key should raise CryptoError if .derive() method raises."""
        from crypto_lib import CryptoError, derive_key

        mock_kdf = MagicMock()
        mock_kdf.derive.side_effect = ValueError("derivation error")

        with patch("crypto_lib.key_management.PBKDF2HMAC", return_value=mock_kdf):
            with pytest.raises(CryptoError, match="Key derivation failed"):
                derive_key("password", salt=b"salt12345678")


# ---------------------------------------------------------------------------
# core.py: test encrypt exception handler (L133-135) and salt parsing (L221)
# ---------------------------------------------------------------------------


class TestEncryptGenericException:
    """Test encrypt() generic exception handler (lines 133-135 in core.py)."""

    def test_encrypt_internal_cipher_failure(self):
        """encrypt should raise EncryptionError on internal cipher failure."""
        from crypto_lib import encrypt, generate_key
        from crypto_lib.exceptions import EncryptionError

        key = generate_key(32)
        with patch(
            "crypto_lib.core.Cipher",
            side_effect=RuntimeError("low-level cipher failure"),
        ):
            with pytest.raises(EncryptionError, match="Encryption failed"):
                encrypt("test data", key)

    def test_encrypt_internal_encryptor_failure(self):
        """encrypt should wrap exception when encryptor.update fails."""
        from crypto_lib import encrypt, generate_key
        from crypto_lib.exceptions import EncryptionError

        key = generate_key(32)

        mock_encryptor = MagicMock()
        mock_encryptor.update.side_effect = RuntimeError("update failed")

        mock_cipher = MagicMock()
        mock_cipher.encryptor.return_value = mock_encryptor

        with patch("crypto_lib.core.Cipher", return_value=mock_cipher):
            with pytest.raises(EncryptionError, match="Encryption failed"):
                encrypt("data", key)


class TestParseSaltDecoding:
    """Test _parse_encrypted_string salt decoding branch (line 221 in core.py)."""

    def test_parse_string_with_real_salt(self):
        """Parsing a string with actual base64 salt should decode it."""
        from crypto_lib.core import _parse_encrypted_string

        iv = os.urandom(12)
        ct = os.urandom(32)
        salt = os.urandom(16)

        iv_b64 = base64.b64encode(iv).decode("utf-8")
        ct_b64 = base64.b64encode(ct).decode("utf-8")
        salt_b64 = base64.b64encode(salt).decode("utf-8")

        encoded = f"AES-256-GCM:{iv_b64}:{salt_b64}:{ct_b64}"
        result = _parse_encrypted_string(encoded)

        assert result.iv == iv
        assert result.ciphertext == ct
        assert result.salt == salt
        assert result.algorithm == "AES-256-GCM"

    def test_parse_string_empty_salt(self):
        """Parsing a string with empty salt should set salt to None."""
        from crypto_lib.core import _parse_encrypted_string

        iv = os.urandom(12)
        ct = os.urandom(32)

        iv_b64 = base64.b64encode(iv).decode("utf-8")
        ct_b64 = base64.b64encode(ct).decode("utf-8")

        encoded = f"AES-256-GCM:{iv_b64}::{ct_b64}"
        result = _parse_encrypted_string(encoded)

        assert result.salt is None

    def test_decrypt_roundtrip_with_salt(self):
        """Encrypt with salt, serialize, parse, decrypt should work end-to-end."""
        from crypto_lib import decrypt, derive_key, encrypt
        from crypto_lib.core import EncryptionResult

        key, salt = derive_key("my-password")
        encrypted = encrypt("secret message", key)

        # Re-create the result with a salt to force the salt branch during parsing
        result_with_salt = EncryptionResult(
            ciphertext=encrypted.ciphertext,
            iv=encrypted.iv,
            salt=salt,
            algorithm=encrypted.algorithm,
        )
        serialized = result_with_salt.to_base64()

        # Decrypt from serialized string (exercises L221 salt decoding)
        decrypted = decrypt(serialized, key)
        assert decrypted.to_string() == "secret message"
        assert decrypted.salt == salt


# ---------------------------------------------------------------------------
# exceptions.py: branch coverage for optional None parameters
# ---------------------------------------------------------------------------


class TestExceptionBranchCoverage:
    """Cover conditional branches when optional params are None vs provided."""

    def test_invalid_key_error_no_optional_params(self):
        """InvalidKeyError with no optional params should have empty details."""
        from crypto_lib.exceptions import InvalidKeyError

        error = InvalidKeyError("bare error")
        assert error.context.details == {}
        assert error.message == "bare error"

    def test_invalid_key_error_only_key_type(self):
        """InvalidKeyError with only key_type."""
        from crypto_lib.exceptions import InvalidKeyError

        error = InvalidKeyError("error", key_type="RSA")
        assert error.context.details == {"key_type": "RSA"}

    def test_invalid_key_error_only_expected_size(self):
        """InvalidKeyError with only expected_size."""
        from crypto_lib.exceptions import InvalidKeyError

        error = InvalidKeyError("error", expected_size=32)
        assert error.context.details == {"expected_size": 32}

    def test_invalid_key_error_only_actual_size(self):
        """InvalidKeyError with only actual_size."""
        from crypto_lib.exceptions import InvalidKeyError

        error = InvalidKeyError("error", actual_size=16)
        assert error.context.details == {"actual_size": 16}

    def test_encryption_error_no_optional_params(self):
        """EncryptionError with no optional params."""
        from crypto_lib.exceptions import EncryptionError

        error = EncryptionError("bare encryption error")
        assert error.context.details == {}

    def test_encryption_error_only_algorithm(self):
        """EncryptionError with only algorithm."""
        from crypto_lib.exceptions import EncryptionError

        error = EncryptionError("error", algorithm="AES")
        assert error.context.details == {"algorithm": "AES"}

    def test_encryption_error_only_data_size(self):
        """EncryptionError with only data_size."""
        from crypto_lib.exceptions import EncryptionError

        error = EncryptionError("error", data_size=512)
        assert error.context.details == {"data_size": 512}

    def test_decryption_error_no_optional_params(self):
        """DecryptionError with no optional params (only corruption_detected default)."""
        from crypto_lib.exceptions import DecryptionError

        error = DecryptionError("bare decryption error")
        # corruption_detected always present with default False
        assert error.context.details == {"corruption_detected": False}

    def test_decryption_error_only_algorithm(self):
        """DecryptionError with only algorithm."""
        from crypto_lib.exceptions import DecryptionError

        error = DecryptionError("error", algorithm="AES-GCM")
        assert error.context.details["algorithm"] == "AES-GCM"

    def test_hashing_error_no_optional_params(self):
        """HashingError with no optional params."""
        from crypto_lib.exceptions import HashingError

        error = HashingError("bare hashing error")
        assert error.context.details == {}
        assert error.context.recoverable is True

    def test_hashing_error_only_algorithm(self):
        """HashingError with only algorithm."""
        from crypto_lib.exceptions import HashingError

        error = HashingError("error", algorithm="MD5")
        assert error.context.details == {"algorithm": "MD5"}

    def test_hashing_error_only_input_size(self):
        """HashingError with only input_size."""
        from crypto_lib.exceptions import HashingError

        error = HashingError("error", input_size=0)
        assert error.context.details == {"input_size": 0}

    def test_validation_error_no_optional_params(self):
        """ValidationError with no optional params."""
        from crypto_lib.exceptions import ValidationError

        error = ValidationError("bare validation error")
        assert error.context.details == {}

    def test_validation_error_only_validation_type(self):
        """ValidationError with only validation_type."""
        from crypto_lib.exceptions import ValidationError

        error = ValidationError("error", validation_type="checksum")
        assert error.context.details == {"validation_type": "checksum"}

    def test_validation_error_only_expected(self):
        """ValidationError with only expected."""
        from crypto_lib.exceptions import ValidationError

        error = ValidationError("error", expected="abc")
        assert error.context.details == {"expected": "abc"}

    def test_validation_error_only_actual(self):
        """ValidationError with only actual."""
        from crypto_lib.exceptions import ValidationError

        error = ValidationError("error", actual="xyz")
        assert error.context.details == {"actual": "xyz"}

    def test_parsing_error_no_optional_params(self):
        """ParsingError with no optional params."""
        from crypto_lib.exceptions import ParsingError

        error = ParsingError("bare parsing error")
        assert error.context.details == {}
        assert error.context.recoverable is True

    def test_parsing_error_only_format_type(self):
        """ParsingError with only format_type."""
        from crypto_lib.exceptions import ParsingError

        error = ParsingError("error", format_type="PEM")
        assert error.context.details == {"format_type": "PEM"}

    def test_parsing_error_only_position(self):
        """ParsingError with only position."""
        from crypto_lib.exceptions import ParsingError

        error = ParsingError("error", position=0)
        assert error.context.details == {"position": 0}

    def test_algorithm_error_no_optional_params(self):
        """AlgorithmError with no optional params."""
        from crypto_lib.exceptions import AlgorithmError

        error = AlgorithmError("bare algorithm error")
        assert error.context.details == {}
        assert error.context.recoverable is True

    def test_algorithm_error_only_algorithm(self):
        """AlgorithmError with only algorithm."""
        from crypto_lib.exceptions import AlgorithmError

        error = AlgorithmError("error", algorithm="DES")
        assert error.context.details == {"algorithm": "DES"}

    def test_algorithm_error_only_supported_algorithms(self):
        """AlgorithmError with only supported_algorithms."""
        from crypto_lib.exceptions import AlgorithmError

        error = AlgorithmError("error", supported_algorithms=["AES-128", "AES-256"])
        assert error.context.details == {"supported_algorithms": ["AES-128", "AES-256"]}

    def test_crypto_error_str_without_details(self):
        """CryptoError.__str__ without details should omit details section."""
        from crypto_lib.exceptions import CryptoError

        error = CryptoError("simple message")
        result = str(error)
        assert "simple message" in result
        assert "operation=unknown" in result

    def test_crypto_error_str_with_details(self):
        """CryptoError.__str__ with details should include key=value pairs."""
        from crypto_lib.exceptions import CryptoError, CryptoErrorCategory, ErrorContext

        ctx = ErrorContext(
            operation="test_op",
            category=CryptoErrorCategory.ENCRYPTION,
            details={"foo": "bar", "baz": 42},
            recoverable=False,
        )
        error = CryptoError("detailed message", context=ctx)
        result = str(error)
        assert "detailed message" in result
        assert "foo=bar" in result
        assert "baz=42" in result


# ---------------------------------------------------------------------------
# Legacy aliases
# ---------------------------------------------------------------------------


class TestLegacyAliases:
    """Test backward-compatibility aliases in exceptions module."""

    def test_unsupported_algorithm_error_alias(self):
        """UnsupportedAlgorithmError should be an alias for AlgorithmError."""
        from crypto_lib.exceptions import AlgorithmError, UnsupportedAlgorithmError

        assert UnsupportedAlgorithmError is AlgorithmError

    def test_key_derivation_error_alias(self):
        """KeyDerivationError should be an alias for InvalidKeyError."""
        from crypto_lib.exceptions import InvalidKeyError, KeyDerivationError

        assert KeyDerivationError is InvalidKeyError


# ---------------------------------------------------------------------------
# CryptoResult type alias
# ---------------------------------------------------------------------------


class TestCryptoResultTypeAlias:
    """Test the CryptoResult type alias is accessible."""

    def test_crypto_result_importable(self):
        """CryptoResult type alias should be importable."""
        from crypto_lib.exceptions import CryptoResult  # noqa: F401
