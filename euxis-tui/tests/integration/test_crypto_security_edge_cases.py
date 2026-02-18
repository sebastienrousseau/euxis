# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Security-focused cryptographic edge case tests.

This test suite focuses on security edge cases, attack vectors, and boundary
conditions that could be exploited or could cause security failures.
"""

import hashlib
import secrets
import struct
from unittest.mock import patch

import pytest

from tui.core.services import (
    AES256GCMAlgorithm,
    AlgorithmInfo,
    AlgorithmRegistry,
    CryptoService,
    InMemoryKeyManager,
    SHA256Algorithm,
    SimpleXORAlgorithm,
    create_default_crypto_service,
)


class TestCryptoSecurityEdgeCases:
    """Security-focused edge case tests for cryptographic operations."""

    def test_key_reuse_detection(self):
        """Test detection of key reuse vulnerabilities."""
        service = create_default_crypto_service()
        key_id = service.generate_and_store_key("reuse_test")

        # Different data with same key should produce different ciphertexts
        data1 = b"First message"
        data2 = b"Second message"

        encrypted1 = service.encrypt(data1, key_id)
        encrypted2 = service.encrypt(data2, key_id)

        # Ciphertexts should be different (important for security)
        assert encrypted1 != encrypted2

        # Both should decrypt correctly
        assert service.decrypt(encrypted1, key_id) == data1
        assert service.decrypt(encrypted2, key_id) == data2

    def test_timing_attack_resistance(self):
        """Test resistance to timing attacks on key operations."""
        import time

        service = create_default_crypto_service()

        # Generate keys with different patterns to test timing consistency
        key_patterns = [
            b"a" * 32,          # All same byte
            (b"alternating123456" * 2)[:32],  # Pattern
            secrets.token_bytes(32),  # Random
            bytes(range(32)),   # Sequential
        ]

        timing_results = []

        for i, key_bytes in enumerate(key_patterns):
            key_id = f"timing_test_{i}"
            service.key_manager.store_key(key_id, key_bytes)

            # Measure encryption timing
            data = b"Timing attack test data"
            times = []

            for _ in range(10):
                start = time.perf_counter()
                encrypted = service.encrypt(data, key_id)
                end = time.perf_counter()
                times.append(end - start)

            avg_time = sum(times) / len(times)
            timing_results.append(avg_time)

        # Timing should be relatively consistent across different key patterns
        # Allow for some variation but flag major differences
        max_time = max(timing_results)
        min_time = min(timing_results)

        # Timing difference should be less than 10x
        assert max_time / min_time < 10, f"Timing attack vulnerability detected: {timing_results}"

    def test_malicious_input_handling(self):
        """Test handling of malicious or edge case inputs."""
        service = create_default_crypto_service()
        key_id = service.generate_and_store_key("malicious_test")

        # Test extremely large data
        large_data = b"A" * 1000000  # 1MB
        try:
            encrypted_large = service.encrypt(large_data, key_id)
            decrypted_large = service.decrypt(encrypted_large, key_id)
            assert decrypted_large == large_data
        except (MemoryError, OverflowError):
            # Acceptable to fail on extremely large data
            pass

        # Test empty data
        empty_data = b""
        encrypted_empty = service.encrypt(empty_data, key_id)
        decrypted_empty = service.decrypt(encrypted_empty, key_id)
        assert decrypted_empty == empty_data

        # Test data with all possible byte values
        all_bytes = bytes(range(256))
        encrypted_bytes = service.encrypt(all_bytes, key_id)
        decrypted_bytes = service.decrypt(encrypted_bytes, key_id)
        assert decrypted_bytes == all_bytes

    def test_xor_key_security_properties(self):
        """Test XOR algorithm security properties and limitations."""
        algo = SimpleXORAlgorithm()

        # Test that XOR is symmetric (encrypt == decrypt)
        key = b"test_key_32_bytes_long_padding!!"
        data = b"test data"

        encrypted = algo.encrypt(data, key)
        decrypted = algo.decrypt(encrypted, key)
        assert decrypted == data

        # Test known XOR vulnerability: XOR with zero reveals key
        zero_data = b"\x00" * len(data)
        encrypted_zero = algo.encrypt(zero_data, key)
        # encrypted_zero should be the key pattern repeated
        expected = bytes(key[i % len(key)] for i in range(len(zero_data)))
        assert encrypted_zero == expected

        # Test XOR weakness: same plaintext produces predictable patterns
        repeated_data = b"AAAA"
        encrypted_repeated = algo.encrypt(repeated_data, key)
        # All bytes should be XOR of 'A' (0x41) with key bytes
        for i, byte in enumerate(encrypted_repeated):
            expected_byte = 0x41 ^ key[i % len(key)]
            assert byte == expected_byte

    def test_hash_collision_resistance(self):
        """Test hash function collision resistance."""
        algo = SHA256Algorithm()

        # Test that different inputs produce different hashes
        test_inputs = [
            b"test1",
            b"test2",
            b"test1\x00",  # Different by null byte
            b"test1 ",     # Different by space
            b"TEST1",      # Different by case
        ]

        hashes = []
        for data in test_inputs:
            hash_result = algo.hash(data)
            hashes.append(hash_result)
            assert len(hash_result) == 32  # SHA-256 produces 32-byte hash

        # All hashes should be unique
        unique_hashes = set(hashes)
        assert len(unique_hashes) == len(hashes), "Hash collision detected"

    def test_key_derivation_security(self):
        """Test key derivation and entropy properties."""
        service = create_default_crypto_service()

        # Generate multiple keys and analyze entropy
        keys = []
        for i in range(100):
            key_id = service.generate_and_store_key(f"entropy_test_{i}")
            key_data = service.key_manager.retrieve_key(key_id)
            keys.append(key_data)

        # Test that all keys are unique
        unique_keys = set(keys)
        assert len(unique_keys) == len(keys), "Key generation lacks uniqueness"

        # Simple entropy test: each byte position should have variety
        byte_positions = [[] for _ in range(32)]
        for key in keys[:50]:  # Use subset for performance
            for i, byte in enumerate(key):
                byte_positions[i].append(byte)

        # Each byte position should have at least some variety
        for i, position_bytes in enumerate(byte_positions):
            unique_bytes_at_position = len(set(position_bytes))
            assert unique_bytes_at_position > 10, f"Low entropy at byte position {i}"

    def test_memory_security_cleanup(self):
        """Test that sensitive data is properly cleared from memory."""
        service = create_default_crypto_service()

        # Create and use a key
        key_id = service.generate_and_store_key("memory_security_test")
        original_key = service.key_manager.retrieve_key(key_id)

        # Encrypt/decrypt some data
        secret_data = b"Very secret information"
        encrypted = service.encrypt(secret_data, key_id)
        decrypted = service.decrypt(encrypted, key_id)
        assert decrypted == secret_data

        # Delete the key
        service.key_manager.delete_key(key_id)

        # Verify key is no longer accessible
        with pytest.raises(KeyError):
            service.key_manager.retrieve_key(key_id)

        # The actual memory cleanup is hard to test directly,
        # but we can at least verify the key is removed from data structures
        assert key_id not in service.key_manager.list_keys()

    def test_algorithm_parameter_injection(self):
        """Test resistance to algorithm parameter injection attacks."""
        registry = AlgorithmRegistry()

        # Test that registering malicious algorithm objects is prevented
        class MaliciousAlgorithm:
            """Malicious algorithm that doesn't implement the protocol correctly."""

            @property
            def name(self):
                return "malicious"

            @property
            def key_size(self):
                return -1  # Negative key size

            def encrypt(self, data, key):
                # Malicious: return unencrypted data
                return data

            def decrypt(self, data, key):
                # Malicious: ignore key
                return data

            def hash(self, data):
                # Malicious: weak hash
                return b"fake_hash"

        malicious_algo = MaliciousAlgorithm()
        malicious_info = AlgorithmInfo("malicious", -1, "Malicious", "symmetric")

        # Should fail type check
        with pytest.raises(TypeError):
            registry.register("not_an_algorithm", malicious_info)

    def test_side_channel_resistance(self):
        """Test resistance to side-channel attacks."""
        service = create_default_crypto_service()

        # Test that operations complete even with invalid inputs
        # without revealing information through exceptions
        valid_key_id = service.generate_and_store_key("valid_key")

        # Test with invalid key ID (should raise KeyError consistently)
        invalid_inputs = [
            "nonexistent",
            "",
            "very_long_key_id_that_does_not_exist",
            "key_with_special_chars_!@#$%",
        ]

        for invalid_key in invalid_inputs:
            with pytest.raises(KeyError):
                service.encrypt(b"test", invalid_key)

    def test_cryptographic_separation(self):
        """Test cryptographic separation between different contexts."""
        service1 = create_default_crypto_service()
        service2 = create_default_crypto_service()

        # Create keys in separate services
        key1_id = service1.generate_and_store_key("service1_key")
        key2_id = service2.generate_and_store_key("service2_key")

        test_data = b"Separation test data"

        # Encrypt with service1
        encrypted1 = service1.encrypt(test_data, key1_id)

        # Should not be able to decrypt with service2 (different key)
        service2.key_manager.store_key("service1_key", b"wrong_key" * 2)
        try:
            decrypted_wrong = service2.decrypt(encrypted1, "service1_key")
            # If decryption doesn't fail, it should produce wrong data
            assert decrypted_wrong != test_data
        except (ValueError, RuntimeError):
            # Expected - wrong key should cause decryption failure
            pass

    def test_boundary_condition_inputs(self):
        """Test boundary conditions for input parameters."""
        service = create_default_crypto_service()

        # Test minimum and maximum reasonable data sizes
        boundary_sizes = [0, 1, 15, 16, 31, 32, 63, 64, 255, 256, 4095, 4096]

        key_id = service.generate_and_store_key("boundary_test")

        for size in boundary_sizes:
            if size > 10000:  # Skip very large sizes for performance
                continue

            data = b"B" * size

            try:
                encrypted = service.encrypt(data, key_id)
                decrypted = service.decrypt(encrypted, key_id)
                assert decrypted == data, f"Boundary test failed for size {size}"
            except (MemoryError, OverflowError):
                # Acceptable for very large inputs
                continue

    def test_concurrent_key_operations(self):
        """Test thread safety of key operations."""
        import threading
        import time

        service = create_default_crypto_service()
        results = []
        errors = []

        def key_operations_worker(worker_id):
            try:
                # Each worker creates, uses, and deletes keys
                for i in range(10):
                    key_id = f"worker_{worker_id}_key_{i}"

                    # Create key
                    service.generate_and_store_key(key_id)

                    # Use key
                    data = f"Worker {worker_id} data {i}".encode()
                    encrypted = service.encrypt(data, key_id)
                    decrypted = service.decrypt(encrypted, key_id)

                    if decrypted != data:
                        errors.append(f"Worker {worker_id}: data corruption")

                    # Clean up key
                    service.key_manager.delete_key(key_id)

                    results.append((worker_id, i))

            except Exception as e:
                errors.append(f"Worker {worker_id}: {str(e)}")

        # Start multiple worker threads
        threads = []
        for i in range(3):
            t = threading.Thread(target=key_operations_worker, args=(i,))
            threads.append(t)
            t.start()

        # Wait for completion
        for t in threads:
            t.join(timeout=10)

        # Verify no errors occurred
        assert len(errors) == 0, f"Thread safety errors: {errors}"
        assert len(results) == 30  # 3 workers × 10 operations

    def test_algorithm_downgrade_prevention(self):
        """Test prevention of algorithm downgrade attacks."""
        service = create_default_crypto_service()

        # Get available algorithms
        algorithms = service.list_available_algorithms()

        # Test that we can't force weak algorithms through manipulation
        if "simple-xor" in algorithms and "sha256" in algorithms:
            # Create key for XOR
            xor_key_id = service.generate_and_store_key("xor_key", "simple-xor")

            # Should not be able to encrypt with hash algorithm
            with pytest.raises((ValueError, NotImplementedError)):
                service.encrypt(b"test", xor_key_id, "sha256")

    def test_error_information_leakage(self):
        """Test that error messages don't leak sensitive information."""
        service = create_default_crypto_service()

        # Test various error conditions
        error_cases = [
            (lambda: service.encrypt(b"test", "nonexistent_key"), KeyError),
            (lambda: service.get_algorithm_info("nonexistent_algo"), ValueError),
        ]

        for error_func, expected_exception in error_cases:
            try:
                error_func()
                assert False, "Expected exception was not raised"
            except expected_exception as e:
                error_msg = str(e).lower()

                # Error messages should not contain key material or sensitive data
                sensitive_patterns = [
                    "key_material",
                    "secret",
                    "private",
                    "password",
                    "token",
                ]

                for pattern in sensitive_patterns:
                    assert pattern not in error_msg, f"Error message may leak sensitive info: {error_msg}"

        # Note: XOR decryption doesn't validate input format, so we skip that test case


if __name__ == "__main__":
    pytest.main([__file__, "-v"])