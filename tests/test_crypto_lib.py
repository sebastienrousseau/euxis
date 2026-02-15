"""Tests for crypto_lib pure functional implementation.

Verifies that all crypto operations are pure functions without filesystem dependencies.
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


class TestPureFunctionality:
    """Test that crypto functions are pure (no side effects)."""

    def test_encrypt_is_deterministic_with_same_inputs(self):
        """Encrypt with same inputs should produce different results due to random IV."""
        key = generate_key()
        data = "test data"

        result1 = encrypt(data, key)
        result2 = encrypt(data, key)

        # Results should be different due to random IV
        assert result1.ciphertext != result2.ciphertext
        assert result1.iv != result2.iv

    def test_encrypt_decrypt_roundtrip(self):
        """Basic encrypt/decrypt roundtrip."""
        key = generate_key()
        original_data = "Hello, World! 🔒"

        # Encrypt
        encrypted = encrypt(original_data, key)
        assert isinstance(encrypted, EncryptionResult)

        # Decrypt
        decrypted = decrypt(encrypted, key)
        assert isinstance(decrypted, DecryptionResult)

        # Verify data integrity
        assert decrypted.to_string() == original_data

    def test_encrypt_bytes_data(self):
        """Test encryption with bytes input."""
        key = generate_key()
        original_data = b"Binary data \x00\x01\x02\xff"

        encrypted = encrypt(original_data, key)
        decrypted = decrypt(encrypted, key)

        assert decrypted.plaintext == original_data

    def test_no_filesystem_access(self, tmp_path, monkeypatch):
        """Verify no filesystem access during crypto operations."""
        # Mock file operations to raise if accessed
        def mock_open(*args, **kwargs):
            msg = "Filesystem access detected"
            raise AssertionError(msg)

        def mock_exists(*args, **kwargs):
            msg = "Filesystem access detected"
            raise AssertionError(msg)

        monkeypatch.setattr("builtins.open", mock_open)
        monkeypatch.setattr("os.path.exists", mock_exists)

        # These operations should work without filesystem access
        key = generate_key()
        data = "test data"

        encrypted = encrypt(data, key)
        decrypted = decrypt(encrypted, key)

        assert decrypted.to_string() == data


class TestEncryptionResult:
    """Test EncryptionResult immutability and serialization."""

    def test_encryption_result_immutable(self):
        """EncryptionResult should be immutable."""
        result = EncryptionResult(
            ciphertext=b"test",
            iv=b"iv",
            algorithm="AES-256-GCM"
        )

        with pytest.raises(AttributeError):
            result.ciphertext = b"modified"

    def test_to_base64_serialization(self):
        """Test base64 serialization and parsing."""
        key = generate_key()
        data = "test data"

        encrypted = encrypt(data, key)
        serialized = encrypted.to_base64()

        # Should be parseable string
        assert isinstance(serialized, str)
        assert "AES-256-GCM" in serialized

        # Should decrypt from serialized form
        decrypted = decrypt(serialized, key)
        assert decrypted.to_string() == data


class TestKeyManagement:
    """Test key generation and derivation functions."""

    def test_generate_key_default_size(self):
        """Test default key generation."""
        key = generate_key()
        assert len(key) == 32
        assert isinstance(key, bytes)

    def test_generate_key_different_sizes(self):
        """Test key generation with different sizes."""
        for size in [16, 24, 32]:
            key = generate_key(size)
            assert len(key) == size

    def test_generate_key_invalid_size(self):
        """Test key generation with invalid size."""
        with pytest.raises(InvalidKeyError):
            generate_key(0)

        with pytest.raises(InvalidKeyError):
            generate_key(15)  # Unsupported size

    def test_derive_key_deterministic(self):
        """Key derivation should be deterministic with same inputs."""
        key_material = "test-secret"
        salt = b"fixed salt 12345"

        key1, _ = derive_key(key_material, salt)
        key2, _ = derive_key(key_material, salt)

        assert key1 == key2

    def test_derive_key_different_salts(self):
        """Different salts should produce different keys."""
        key_material = "test-secret"

        key1, salt1 = derive_key(key_material)
        key2, salt2 = derive_key(key_material)

        assert key1 != key2
        assert salt1 != salt2

    def test_derive_key_invalid_params(self):
        """Test key derivation parameter validation."""
        with pytest.raises(InvalidKeyError):
            derive_key("")  # Empty password

        with pytest.raises(InvalidKeyError):
            derive_key("password", iterations=100)  # Too few iterations


class TestErrorHandling:
    """Test error handling and validation."""

    def test_encrypt_invalid_key_size(self):
        """Test encryption with wrong key size."""
        wrong_key = b"too short"
        data = "test"

        with pytest.raises(InvalidKeyError):
            encrypt(data, wrong_key)

    def test_decrypt_invalid_key_size(self):
        """Test decryption with wrong key size."""
        key = generate_key()
        wrong_key = b"wrong"
        data = "test"

        encrypted = encrypt(data, key)

        with pytest.raises(InvalidKeyError):
            decrypt(encrypted, wrong_key)

    def test_decrypt_wrong_key(self):
        """Test decryption with wrong key (correct size)."""
        key1 = generate_key()
        key2 = generate_key()
        data = "test"

        encrypted = encrypt(data, key1)

        with pytest.raises(DecryptionError):
            decrypt(encrypted, key2)

    def test_decrypt_corrupted_data(self):
        """Test decryption with corrupted ciphertext."""
        key = generate_key()
        data = "test"

        encrypted = encrypt(data, key)

        # Corrupt the ciphertext
        corrupted = EncryptionResult(
            ciphertext=b"corrupted data",
            iv=encrypted.iv,
            algorithm=encrypted.algorithm
        )

        with pytest.raises(DecryptionError):
            decrypt(corrupted, key)

    def test_unsupported_algorithm(self):
        """Test unsupported algorithm error."""
        key = generate_key()
        data = "test"

        with pytest.raises(CryptoError):
            encrypt(data, key, algorithm="UNSUPPORTED")


class TestCompatibility:
    """Test compatibility and edge cases."""

    def test_empty_data(self):
        """Test encryption/decryption of empty data."""
        key = generate_key()
        data = ""

        encrypted = encrypt(data, key)
        decrypted = decrypt(encrypted, key)

        assert decrypted.to_string() == data

    def test_large_data(self):
        """Test encryption/decryption of large data."""
        key = generate_key()
        data = "A" * 1000000  # 1MB of data

        encrypted = encrypt(data, key)
        decrypted = decrypt(encrypted, key)

        assert decrypted.to_string() == data

    def test_unicode_data(self):
        """Test encryption/decryption of unicode data."""
        key = generate_key()
        data = "Unicode: 🔒🔑🛡️ テスト 测试"

        encrypted = encrypt(data, key)
        decrypted = decrypt(encrypted, key)

        assert decrypted.to_string() == data
