# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Additional tests to achieve 95%+ coverage for crypto_lib.

Covers:
- validate_key() function
- Result type methods (unwrap, unwrap_or, unwrap_error)
- ErrorContext.with_detail()
- All error subclass constructors
- Edge cases in core.py (parsing, encoding)
"""

import pytest

from crypto_lib import (
    CryptoError,
    DecryptionError,
    DecryptionResult,
    EncryptionResult,
    InvalidKeyError,
    decrypt,
    derive_key,
    encrypt,
    generate_key,
)
from crypto_lib.exceptions import (
    AlgorithmError,
    CryptoErrorCategory,
    EncryptionError,
    ErrorContext,
    HashingError,
    ParsingError,
    Result,
    ValidationError,
)
from crypto_lib.key_management import validate_key


class TestValidateKey:
    """Test validate_key() function."""

    def test_validate_key_success(self):
        """Valid key should return True."""
        key = generate_key(32)
        assert validate_key(key, expected_size=32) is True

    def test_validate_key_16_bytes(self):
        """16-byte key should validate."""
        key = generate_key(16)
        assert validate_key(key, expected_size=16) is True

    def test_validate_key_wrong_type(self):
        """Non-bytes key should raise InvalidKeyError."""
        with pytest.raises(InvalidKeyError, match="must be bytes"):
            validate_key("not bytes", expected_size=32)

    def test_validate_key_wrong_size(self):
        """Wrong size key should raise InvalidKeyError."""
        key = generate_key(16)
        with pytest.raises(InvalidKeyError, match="must be 32 bytes"):
            validate_key(key, expected_size=32)

    def test_validate_key_all_zeros(self):
        """All-zeros key should raise InvalidKeyError."""
        weak_key = b"\x00" * 32
        with pytest.raises(InvalidKeyError, match="cannot be all zeros"):
            validate_key(weak_key, expected_size=32)

    def test_validate_key_all_ones(self):
        """All-ones key should raise InvalidKeyError."""
        weak_key = b"\xff" * 32
        with pytest.raises(InvalidKeyError, match="cannot be all ones"):
            validate_key(weak_key, expected_size=32)


class TestDeriveKeyEdgeCases:
    """Test derive_key() edge cases."""

    def test_derive_key_short_salt(self):
        """Short salt should raise InvalidKeyError."""
        with pytest.raises(InvalidKeyError, match="at least 8 bytes"):
            derive_key("password", salt=b"short")

    def test_derive_key_negative_key_size(self):
        """Negative key size should raise InvalidKeyError."""
        with pytest.raises(InvalidKeyError, match="must be positive"):
            derive_key("password", key_size=-1)

    def test_derive_key_with_valid_salt(self):
        """Valid explicit salt should work."""
        salt = b"valid_salt_1234567"
        key, returned_salt = derive_key("password", salt=salt)
        assert returned_salt == salt
        assert len(key) == 32


class TestResultType:
    """Test Result<T, E> type methods."""

    def test_result_success(self):
        """Test Result.success() creation."""
        result = Result.success("value")
        assert result.is_success is True
        assert result.is_error is False
        assert result.unwrap() == "value"

    def test_result_error(self):
        """Test Result.error() creation."""
        result = Result.error("error message")
        assert result.is_success is False
        assert result.is_error is True
        assert result.unwrap_error() == "error message"

    def test_result_unwrap_on_error(self):
        """unwrap() on error should raise ValueError."""
        result = Result.error("error")
        with pytest.raises(ValueError, match="Called unwrap\\(\\) on error result"):
            result.unwrap()

    def test_result_unwrap_error_on_success(self):
        """unwrap_error() on success should raise ValueError."""
        result = Result.success("value")
        with pytest.raises(ValueError, match="Called unwrap_error\\(\\) on success result"):
            result.unwrap_error()

    def test_result_unwrap_or_success(self):
        """unwrap_or() on success should return value."""
        result = Result.success("value")
        assert result.unwrap_or("default") == "value"

    def test_result_unwrap_or_error(self):
        """unwrap_or() on error should return default."""
        result = Result.error("error")
        assert result.unwrap_or("default") == "default"


class TestErrorContext:
    """Test ErrorContext methods."""

    def test_error_context_with_detail(self):
        """Test with_detail() creates new context."""
        ctx = ErrorContext(
            operation="test",
            category=CryptoErrorCategory.ENCRYPTION,
            details={"key": "value"},
            recoverable=False
        )
        new_ctx = ctx.with_detail("new_key", "new_value")

        # Original should be unchanged
        assert "new_key" not in ctx.details

        # New context should have both
        assert new_ctx.details["key"] == "value"
        assert new_ctx.details["new_key"] == "new_value"
        assert new_ctx.operation == "test"
        assert new_ctx.category == CryptoErrorCategory.ENCRYPTION


class TestErrorSubclasses:
    """Test all error subclass constructors."""

    def test_crypto_error_with_context(self):
        """Test CryptoError with explicit context."""
        ctx = ErrorContext(
            operation="test_op",
            category=CryptoErrorCategory.ALGORITHM,
            details={"key": "value"},
            recoverable=True
        )
        error = CryptoError("test message", context=ctx)
        assert error.message == "test message"
        assert error.context.operation == "test_op"
        assert "key=value" in str(error)

    def test_crypto_error_without_context(self):
        """Test CryptoError without context uses default."""
        error = CryptoError("test message")
        assert error.context.operation == "unknown"
        str(error)  # Should not raise

    def test_invalid_key_error_full_params(self):
        """Test InvalidKeyError with all parameters."""
        error = InvalidKeyError(
            "key error",
            key_type="AES-256",
            expected_size=32,
            actual_size=16
        )
        assert error.context.category == CryptoErrorCategory.KEY_MANAGEMENT
        assert error.context.details["key_type"] == "AES-256"
        assert error.context.details["expected_size"] == 32
        assert error.context.details["actual_size"] == 16

    def test_encryption_error_full_params(self):
        """Test EncryptionError with all parameters."""
        error = EncryptionError("enc error", algorithm="AES-256", data_size=1024)
        assert error.context.category == CryptoErrorCategory.ENCRYPTION
        assert error.context.details["algorithm"] == "AES-256"
        assert error.context.details["data_size"] == 1024

    def test_decryption_error_full_params(self):
        """Test DecryptionError with all parameters."""
        error = DecryptionError("dec error", algorithm="AES-256", corruption_detected=True)
        assert error.context.category == CryptoErrorCategory.DECRYPTION
        assert error.context.details["corruption_detected"] is True

    def test_hashing_error_full_params(self):
        """Test HashingError with all parameters."""
        error = HashingError("hash error", algorithm="SHA-256", input_size=100)
        assert error.context.category == CryptoErrorCategory.HASHING
        assert error.context.recoverable is True  # Hashing errors are recoverable

    def test_validation_error_full_params(self):
        """Test ValidationError with all parameters."""
        error = ValidationError(
            "validation error",
            validation_type="signature",
            expected="abc",
            actual="def"
        )
        assert error.context.category == CryptoErrorCategory.VALIDATION
        assert error.context.details["validation_type"] == "signature"

    def test_parsing_error_full_params(self):
        """Test ParsingError with all parameters."""
        error = ParsingError("parse error", format_type="base64", position=10)
        assert error.context.category == CryptoErrorCategory.PARSING
        assert error.context.recoverable is True

    def test_algorithm_error_full_params(self):
        """Test AlgorithmError with all parameters."""
        error = AlgorithmError(
            "algo error",
            algorithm="ChaCha20",
            supported_algorithms=["AES-256", "AES-128"]
        )
        assert error.context.category == CryptoErrorCategory.ALGORITHM
        assert error.context.recoverable is True


class TestEncryptionResultEdgeCases:
    """Test EncryptionResult edge cases."""

    def test_to_base64_with_salt(self):
        """Test to_base64() with salt included."""
        # Derive a key with salt
        key, salt = derive_key("password")
        data = "test data"

        # Create encrypted result with salt
        encrypted = encrypt(data, key)
        result_with_salt = EncryptionResult(
            ciphertext=encrypted.ciphertext,
            iv=encrypted.iv,
            salt=salt,
            algorithm=encrypted.algorithm
        )

        serialized = result_with_salt.to_base64()
        assert isinstance(serialized, str)
        parts = serialized.split(":")
        assert len(parts) == 4
        assert parts[2] != ""  # Salt should be present


class TestDecryptionResultEdgeCases:
    """Test DecryptionResult edge cases."""

    def test_to_string_invalid_encoding(self):
        """Test to_string() with invalid encoding raises CryptoError."""
        # Create a DecryptionResult with non-UTF8 bytes
        result = DecryptionResult(
            plaintext=b"\xff\xfe",  # Invalid UTF-8
            algorithm="AES-256-GCM",
            iv=b"test_iv_1234"
        )

        with pytest.raises(CryptoError, match="Failed to decode"):
            result.to_string("utf-8")


class TestDecryptEdgeCases:
    """Test decrypt() edge cases."""

    def test_decrypt_invalid_type(self):
        """Test decrypt() with invalid encrypted_data type."""
        key = generate_key()

        with pytest.raises(CryptoError, match="must be EncryptionResult or base64 string"):
            decrypt(12345, key)  # Integer instead of valid type

    def test_decrypt_unsupported_algorithm_in_result(self):
        """Test decrypt() with unsupported algorithm in EncryptionResult."""
        key = generate_key()
        invalid_result = EncryptionResult(
            ciphertext=b"test" * 10,
            iv=b"test_iv_1234",
            algorithm="UNSUPPORTED-ALGO"
        )

        with pytest.raises(CryptoError, match="Unsupported algorithm"):
            decrypt(invalid_result, key)

    def test_decrypt_short_ciphertext(self):
        """Test decrypt() with ciphertext too short for auth tag."""
        key = generate_key()
        short_result = EncryptionResult(
            ciphertext=b"short",  # Less than 16 bytes
            iv=b"test_iv_1234",
            algorithm="AES-256-GCM"
        )

        with pytest.raises(DecryptionError, match="too short"):
            decrypt(short_result, key)


class TestParseEncryptedString:
    """Test _parse_encrypted_string() edge cases."""

    def test_parse_invalid_format(self):
        """Test parsing string with wrong number of parts."""
        key = generate_key()

        with pytest.raises(CryptoError, match="Invalid encrypted string format"):
            decrypt("only:two:parts", key)

    def test_parse_invalid_base64(self):
        """Test parsing string with invalid base64."""
        key = generate_key()

        with pytest.raises(CryptoError, match="Failed to parse"):
            decrypt("AES-256-GCM:invalid!base64:salt:ciphertext", key)

    def test_parse_valid_without_salt(self):
        """Test parsing valid string without salt."""
        key = generate_key()
        data = "test"

        encrypted = encrypt(data, key)
        serialized = encrypted.to_base64()

        # Should work
        decrypted = decrypt(serialized, key)
        assert decrypted.to_string() == data


class TestGenerateKeyEdgeCases:
    """Test generate_key() edge cases."""

    def test_generate_key_negative_size(self):
        """Test generate_key() with negative size."""
        with pytest.raises(InvalidKeyError, match="must be positive"):
            generate_key(-1)

    def test_generate_key_zero_size(self):
        """Test generate_key() with zero size."""
        with pytest.raises(InvalidKeyError, match="must be positive"):
            generate_key(0)

    def test_generate_key_unsupported_size(self):
        """Test generate_key() with unsupported size (e.g., 20)."""
        with pytest.raises(InvalidKeyError, match="Unsupported key size"):
            generate_key(20)
