#!/usr/bin/env python3
"""
Regression tests for crypto operations to ensure no functional regressions.

These tests validate that crypto operations produce consistent, expected outputs
and maintain backward compatibility.
"""

import pytest
import json
import os
import sys

# Add crypto_lib to path
crypto_lib_path = os.path.join(os.path.dirname(__file__), '../../../..')
sys.path.insert(0, crypto_lib_path)

from crypto_lib import encrypt, decrypt, EncryptionResult


class TestCryptoOperationsRegression:
    """Test suite for crypto operations regression validation"""

    def setup_method(self):
        """Set up test fixtures with consistent key"""
        # Use a fixed key for deterministic baseline comparison
        self.test_key = b'a' * 32  # 32-byte key for AES-256
        self.test_data = "Hello, World! This is test data."

    def test_encrypt_decrypt_roundtrip(self):
        """Test that encrypt->decrypt returns original data"""
        # Encrypt data
        result = encrypt(self.test_data, self.test_key)

        # Verify encryption result structure
        assert isinstance(result, EncryptionResult)
        assert result.algorithm == "AES-256-GCM"
        assert len(result.iv) == 12  # GCM IV should be 12 bytes
        assert len(result.ciphertext) >= 16  # At least auth tag length

        # Decrypt data
        decrypted = decrypt(result, self.test_key)

        # Verify roundtrip
        assert decrypted.to_string() == self.test_data
        assert decrypted.algorithm == result.algorithm

    def test_encrypt_output_format(self):
        """Test that encrypt output format is stable"""
        result = encrypt(self.test_data, self.test_key)

        # Test base64 encoding format
        encoded = result.to_base64()
        parts = encoded.split(':')

        assert len(parts) == 4  # algorithm:iv::ciphertext (salt empty)
        assert parts[0] == "AES-256-GCM"
        assert parts[2] == ""  # salt should be empty

    def test_deterministic_input_handling(self):
        """Test that same inputs produce valid but different outputs (due to random IV)"""
        result1 = encrypt(self.test_data, self.test_key)
        result2 = encrypt(self.test_data, self.test_key)

        # IVs should be different (random)
        assert result1.iv != result2.iv

        # But both should decrypt to same plaintext
        assert decrypt(result1, self.test_key).to_string() == self.test_data
        assert decrypt(result2, self.test_key).to_string() == self.test_data

    def test_error_handling_regression(self):
        """Test that error handling behavior is consistent"""
        # Test invalid key length
        with pytest.raises(Exception) as exc_info:
            encrypt(self.test_data, b'short_key')
        assert "32-byte key" in str(exc_info.value)

        # Test decryption with wrong key
        result = encrypt(self.test_data, self.test_key)
        wrong_key = b'b' * 32

        with pytest.raises(Exception):
            decrypt(result, wrong_key)


def test_generate_baseline_operations():
    """Generate baseline crypto operations for comparison"""
    baseline_dir = "baseline"
    os.makedirs(baseline_dir, exist_ok=True)

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
    with open(os.path.join(baseline_dir, "crypto-ops.json"), "w") as f:
        json.dump(operations_log, f, indent=2)


if __name__ == "__main__":
    # Generate baseline if run directly
    test_generate_baseline_operations()
    print("Baseline crypto operations generated")