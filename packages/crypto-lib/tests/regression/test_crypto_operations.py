#!/usr/bin/env python3
"""Regression tests for crypto operations to ensure no functional regressions.

These tests validate that crypto operations produce consistent, expected outputs
and maintain backward compatibility.
"""

import importlib
import json
import sys
from pathlib import Path

import pytest

# Add crypto_lib to path
crypto_lib_path = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(crypto_lib_path))

crypto_lib = importlib.import_module("crypto_lib")
EncryptionResult = crypto_lib.EncryptionResult
decrypt = crypto_lib.decrypt
encrypt = crypto_lib.encrypt
crypto_exceptions = importlib.import_module("crypto_lib.exceptions")
DecryptionError = crypto_exceptions.DecryptionError
InvalidKeyError = crypto_exceptions.InvalidKeyError

GCM_IV_LENGTH = 12
AUTH_TAG_LENGTH = 16
ENCODED_PARTS = 4


class TestCryptoOperationsRegression:
    """Test suite for crypto operations regression validation."""

    def setup_method(self) -> None:
        """Set up test fixtures with consistent key."""
        # Use a fixed key for deterministic baseline comparison
        self.test_key = b"a" * 32  # 32-byte key for AES-256
        self.test_data = "Hello, World! This is test data."

    def test_encrypt_decrypt_roundtrip(self) -> None:
        """Test that encrypt->decrypt returns original data."""
        # Encrypt data
        result = encrypt(self.test_data, self.test_key)

        # Verify encryption result structure
        assert isinstance(result, EncryptionResult)  # noqa: S101
        assert result.algorithm == "AES-256-GCM"  # noqa: S101
        assert len(result.iv) == GCM_IV_LENGTH  # noqa: S101
        assert len(result.ciphertext) >= AUTH_TAG_LENGTH  # noqa: S101

        # Decrypt data
        decrypted = decrypt(result, self.test_key)

        # Verify roundtrip
        assert decrypted.to_string() == self.test_data  # noqa: S101
        assert decrypted.algorithm == result.algorithm  # noqa: S101

    def test_encrypt_output_format(self) -> None:
        """Test that encrypt output format is stable."""
        result = encrypt(self.test_data, self.test_key)

        # Test base64 encoding format
        encoded = result.to_base64()
        parts = encoded.split(":")

        assert len(parts) == ENCODED_PARTS  # noqa: S101
        assert parts[0] == "AES-256-GCM"  # noqa: S101
        assert parts[2] == ""  # noqa: S101  # salt should be empty

    def test_deterministic_input_handling(self) -> None:
        """Test that same inputs produce valid but different outputs (due to random IV)."""
        result1 = encrypt(self.test_data, self.test_key)
        result2 = encrypt(self.test_data, self.test_key)

        # IVs should be different (random)
        assert result1.iv != result2.iv  # noqa: S101

        # But both should decrypt to same plaintext
        assert decrypt(result1, self.test_key).to_string() == self.test_data  # noqa: S101
        assert decrypt(result2, self.test_key).to_string() == self.test_data  # noqa: S101

    def test_error_handling_regression(self) -> None:
        """Test that error handling behavior is consistent."""
        # Test invalid key length
        with pytest.raises(InvalidKeyError, match="32-byte") as exc_info:
            encrypt(self.test_data, b"short_key")
        assert "32-byte key" in str(exc_info.value)  # noqa: S101

        # Test decryption with wrong key
        result = encrypt(self.test_data, self.test_key)
        wrong_key = b"b" * 32

        with pytest.raises(DecryptionError, match=r"Decryption failed|Ciphertext"):
            decrypt(result, wrong_key)


def test_generate_baseline_operations() -> None:
    """Generate baseline crypto operations for comparison."""
    baseline_dir = Path("baseline")
    baseline_dir.mkdir(parents=True, exist_ok=True)

    # Fixed test vectors for baseline comparison
    test_vectors = [
        {"data": "test", "key_pattern": "a"},
        {"data": "Hello, World!", "key_pattern": "b"},
        {"data": "Multi-line\ntest\ndata", "key_pattern": "c"},
        {"data": "Special chars: àáâãäå αβγδεζ", "key_pattern": "d"}
    ]

    operations_log = []

    for i, vector in enumerate(test_vectors):
        key = (vector["key_pattern"] * 32).encode()[:32]

        # Encrypt
        result = encrypt(vector["data"], key)

        # Record operation (without random elements like IV)
        op_record = {
            "test_id": i,
            "input_data": vector["data"],
            "key_pattern": vector["key_pattern"],
            "algorithm": result.algorithm,
            "iv_length": len(result.iv),
            "ciphertext_length": len(result.ciphertext),
            "successful_roundtrip": decrypt(result, key).to_string() == vector["data"]
        }

        operations_log.append(op_record)

    # Write baseline file
    with (baseline_dir / "crypto-ops.json").open("w") as f:
        json.dump(operations_log, f, indent=2)


if __name__ == "__main__":
    # Generate baseline if run directly
    test_generate_baseline_operations()
    sys.stdout.write("Baseline crypto operations generated\n")
