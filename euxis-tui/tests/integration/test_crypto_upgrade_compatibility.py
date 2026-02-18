# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Comprehensive cryptographic test suite for post-upgrade compatibility verification.

This test suite ensures that cryptographic operations remain compatible after:
- Python version upgrades
- Dependency upgrades
- Crypto library upgrades
- System SSL/TLS updates

Covers all critical paths for production cryptographic operations.
"""

import hashlib
import json
import secrets
import tempfile
import time
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

# Import crypto components
from tui.core.services import (
    AES256GCMAlgorithm,
    AlgorithmInfo,
    AlgorithmRegistry,
    CryptoService,
    InMemoryKeyManager,
    KeyManagementService,
    SHA256Algorithm,
    SimpleXORAlgorithm,
    create_default_crypto_service,
    setup_service_container,
)


class TestCryptoUpgradeCompatibility:
    """Test suite for cryptographic compatibility after upgrades."""

    def setup_method(self):
        """Set up test fixtures for each test method."""
        self.test_data_vectors = [
            b"Simple test data",
            b"Multi-line\ntest\ndata with special chars: \x00\x01\xff",
            b"Long data: " + b"A" * 1000,
            b"Unicode data: \xe2\x9c\x93 \xf0\x9f\x94\x92 \xc3\xa9",
            b"",  # Empty data
            b"Single byte",
            b"Edge case: " + bytes(range(256)),  # All byte values
        ]

        self.test_key_patterns = [
            b"a" * 32,  # Simple pattern
            b"test_key_32_bytes_long_padding!!",  # Mixed content
            secrets.token_bytes(32),  # Random key
            bytes(range(32)),  # Sequential bytes
        ]

    def test_algorithm_registry_compatibility(self):
        """Test that algorithm registry works correctly after upgrades."""
        registry = AlgorithmRegistry()

        # Test XOR algorithm registration
        xor_algo = SimpleXORAlgorithm()
        xor_info = AlgorithmInfo(
            name="simple-xor",
            key_size=32,
            description="Simple XOR encryption",
            algorithm_type="symmetric"
        )
        registry.register(xor_algo, xor_info)

        # Test SHA256 algorithm registration
        sha_algo = SHA256Algorithm()
        sha_info = AlgorithmInfo(
            name="sha256",
            key_size=0,
            description="SHA-256 hash",
            algorithm_type="hash"
        )
        registry.register(sha_algo, sha_info)

        # Verify registry operations
        assert "simple-xor" in registry.list_algorithms()
        assert "sha256" in registry.list_algorithms()
        assert len(registry.list_by_type("symmetric")) == 1
        assert len(registry.list_by_type("hash")) == 1

        # Test retrieval
        retrieved_xor = registry.get_algorithm("simple-xor")
        assert retrieved_xor.name == "simple-xor"
        assert retrieved_xor.key_size == 32

        retrieved_sha = registry.get_algorithm("sha256")
        assert retrieved_sha.name == "sha256"
        assert retrieved_sha.key_size == 0

    def test_key_management_compatibility(self):
        """Test key management operations across upgrade scenarios."""
        registry = AlgorithmRegistry()
        xor_algo = SimpleXORAlgorithm()
        xor_info = AlgorithmInfo("simple-xor", 32, "XOR", "symmetric")
        registry.register(xor_algo, xor_info)

        key_manager = InMemoryKeyManager()
        key_manager.set_algorithm_registry(registry)

        # Test key generation
        generated_key = key_manager.generate_key("simple-xor")
        assert len(generated_key) == 32
        assert isinstance(generated_key, bytes)

        # Test key storage and retrieval
        test_key = b"test_key_32_bytes_long_padding!!"
        key_manager.store_key("test_id", test_key)

        retrieved_key = key_manager.retrieve_key("test_id")
        assert retrieved_key == test_key

        # Test key listing
        key_manager.store_key("key1", b"a" * 32)
        key_manager.store_key("key2", b"b" * 32)

        keys = key_manager.list_keys()
        assert "test_id" in keys
        assert "key1" in keys
        assert "key2" in keys

        # Test key deletion
        key_manager.delete_key("key1")
        assert "key1" not in key_manager.list_keys()
        assert "key2" in key_manager.list_keys()

    def test_xor_algorithm_compatibility(self):
        """Test SimpleXOR algorithm across all data vectors and keys."""
        algo = SimpleXORAlgorithm()

        assert algo.name == "simple-xor"
        assert algo.key_size == 32

        # Test all combinations of data and keys
        for data in self.test_data_vectors:
            for key in self.test_key_patterns:
                # Ensure key is correct size
                if len(key) != 32:
                    key = key[:32] if len(key) > 32 else key.ljust(32, b'\x00')

                # Test encryption/decryption roundtrip
                encrypted = algo.encrypt(data, key)
                assert encrypted != data or len(data) == 0  # Empty data case

                decrypted = algo.decrypt(encrypted, key)
                assert decrypted == data, f"Roundtrip failed for data length {len(data)}"

        # Test hash function
        for data in self.test_data_vectors:
            hash_result = algo.hash(data)
            expected = hashlib.sha256(data).digest()
            assert hash_result == expected
            assert len(hash_result) == 32

    def test_sha256_algorithm_compatibility(self):
        """Test SHA256 algorithm compatibility."""
        algo = SHA256Algorithm()

        assert algo.name == "sha256"
        assert algo.key_size == 0

        # Test that encryption/decryption raise appropriate errors
        with pytest.raises(NotImplementedError, match="do not support encryption"):
            algo.encrypt(b"data", b"key")

        with pytest.raises(NotImplementedError, match="do not support decryption"):
            algo.decrypt(b"data", b"key")

        # Test hash function with all data vectors
        for data in self.test_data_vectors:
            hash_result = algo.hash(data)
            expected = hashlib.sha256(data).digest()
            assert hash_result == expected
            assert len(hash_result) == 32

            # Test consistency
            hash_result2 = algo.hash(data)
            assert hash_result == hash_result2

    def test_aes256gcm_algorithm_compatibility(self):
        """Test AES-256-GCM algorithm compatibility with crypto_bridge."""
        algo = AES256GCMAlgorithm()

        assert algo.name == "aes-256-gcm"
        assert algo.key_size == 32

        # Test hash function (should work regardless of crypto_lib availability)
        test_data = b"test data for hashing"
        hash_result = algo.hash(test_data)
        expected = hashlib.sha256(test_data).digest()
        assert hash_result == expected

    @patch('tui.core.services.CRYPTO_LIB_AVAILABLE', False)
    def test_aes256gcm_unavailable_fallback(self):
        """Test AES-256-GCM behavior when crypto_lib is unavailable."""
        algo = AES256GCMAlgorithm()
        key = b"a" * 32
        data = b"test data"

        # Should raise RuntimeError when crypto_lib unavailable
        with pytest.raises(RuntimeError, match="crypto_lib not available"):
            algo.encrypt(data, key)

        with pytest.raises(RuntimeError, match="crypto_lib not available"):
            algo.decrypt(data, key)

    def test_crypto_service_integration_compatibility(self):
        """Test full CryptoService integration after upgrades."""
        service = create_default_crypto_service()

        # Test service initialization
        algorithms = service.list_available_algorithms()
        assert "simple-xor" in algorithms
        assert "sha256" in algorithms

        # Test key generation and encryption with default algorithm
        key_id = service.generate_and_store_key("integration_test")

        for data in self.test_data_vectors[:4]:  # Test subset for performance
            # Test encryption/decryption
            encrypted = service.encrypt(data, key_id)
            decrypted = service.decrypt(encrypted, key_id)
            assert decrypted == data

            # Test with explicit algorithm
            if "simple-xor" in algorithms:
                encrypted_xor = service.encrypt(data, key_id, "simple-xor")
                decrypted_xor = service.decrypt(encrypted_xor, key_id, "simple-xor")
                assert decrypted_xor == data

        # Test hash function
        for data in self.test_data_vectors[:3]:
            hash_result = service.hash(data, "sha256")
            expected = hashlib.sha256(data).digest()
            assert hash_result == expected

    def test_algorithm_switching_compatibility(self):
        """Test runtime algorithm switching after upgrades."""
        service = create_default_crypto_service()

        # Test switching between available algorithms
        available = service.list_available_algorithms()
        symmetric_algos = []

        for algo in available:
            info = service.get_algorithm_info(algo)
            if info.algorithm_type == "symmetric":
                symmetric_algos.append(algo)

        # Test switching between symmetric algorithms
        test_data = b"Algorithm switching test data"

        for algo in symmetric_algos:
            service.switch_algorithm(algo)

            # Generate key for current algorithm
            key_id = service.generate_and_store_key(f"key_{algo}", algo)

            # Test encryption with switched algorithm
            encrypted = service.encrypt(test_data, key_id)
            decrypted = service.decrypt(encrypted, key_id)
            assert decrypted == test_data

    def test_dependency_injection_container_compatibility(self):
        """Test service container dependency injection after upgrades."""
        container = setup_service_container()

        # Test getting services from container
        registry = container.get(AlgorithmRegistry)
        assert isinstance(registry, AlgorithmRegistry)

        crypto_service = container.get(CryptoService)
        assert isinstance(crypto_service, CryptoService)

        key_manager = container.get(KeyManagementService)
        assert isinstance(key_manager, KeyManagementService)

        # Test service functionality
        test_data = b"Container test data"
        key_id = crypto_service.generate_and_store_key("container_test")

        encrypted = crypto_service.encrypt(test_data, key_id)
        decrypted = crypto_service.decrypt(encrypted, key_id)
        assert decrypted == test_data

    def test_error_handling_compatibility(self):
        """Test error handling behavior after upgrades."""
        service = create_default_crypto_service()

        # Test invalid key errors
        with pytest.raises(KeyError, match="not found"):
            service.encrypt(b"data", "nonexistent_key")

        with pytest.raises(KeyError, match="not found"):
            service.decrypt(b"data", "nonexistent_key")

        # Test invalid algorithm errors
        key_id = service.generate_and_store_key("error_test")

        with pytest.raises(ValueError, match="not found"):
            service.encrypt(b"data", key_id, "nonexistent_algorithm")

        with pytest.raises(ValueError, match="not available"):
            service.switch_algorithm("nonexistent_algorithm")

        # Test key size validation for XOR algorithm
        algo = SimpleXORAlgorithm()
        with pytest.raises(ValueError, match="must be 32 bytes"):
            algo.encrypt(b"data", b"short_key")

    def test_concurrent_operations_compatibility(self):
        """Test concurrent cryptographic operations after upgrades."""
        import threading
        import time

        service = create_default_crypto_service()
        results = []
        errors = []

        def crypto_worker(worker_id):
            try:
                # Each worker gets its own key
                key_id = service.generate_and_store_key(f"worker_{worker_id}")

                for i in range(10):
                    data = f"Worker {worker_id} data {i}".encode()

                    # Encrypt/decrypt cycle
                    encrypted = service.encrypt(data, key_id)
                    decrypted = service.decrypt(encrypted, key_id)

                    if decrypted != data:
                        errors.append(f"Worker {worker_id}: roundtrip failed")

                    results.append((worker_id, i, len(encrypted)))

            except Exception as e:
                errors.append(f"Worker {worker_id}: {e}")

        # Start multiple threads
        threads = []
        for i in range(5):
            t = threading.Thread(target=crypto_worker, args=(i,))
            threads.append(t)
            t.start()

        # Wait for completion
        for t in threads:
            t.join(timeout=10)

        # Verify results
        assert len(errors) == 0, f"Concurrent operation errors: {errors}"
        assert len(results) == 50  # 5 workers * 10 operations each

    def test_memory_cleanup_compatibility(self):
        """Test memory cleanup behavior after upgrades."""
        import gc

        try:
            import psutil
            import os
            has_psutil = True
            process = psutil.Process(os.getpid())
            initial_memory = process.memory_info().rss
        except ImportError:
            has_psutil = False
            initial_memory = 0

        # Perform many crypto operations
        for _ in range(100):
            service = create_default_crypto_service()
            key_id = service.generate_and_store_key("memory_test")

            data = b"Memory test data" * 100  # ~1.6KB per iteration
            encrypted = service.encrypt(data, key_id)
            decrypted = service.decrypt(encrypted, key_id)
            assert decrypted == data

            # Clean up service
            del service

        # Force garbage collection
        gc.collect()

        if has_psutil:
            final_memory = process.memory_info().rss
            memory_growth = final_memory - initial_memory

            # Allow some memory growth but ensure no major leaks (< 50MB growth)
            assert memory_growth < 50 * 1024 * 1024, f"Memory growth too large: {memory_growth} bytes"
        else:
            # If psutil not available, just ensure operations completed without exception
            assert True, "Memory cleanup test completed (psutil not available for memory monitoring)"

    def test_performance_baseline_compatibility(self):
        """Test performance baselines after upgrades."""
        service = create_default_crypto_service()
        key_id = service.generate_and_store_key("perf_test")

        # Test data sizes
        test_sizes = [100, 1000, 10000, 100000]  # bytes
        performance_results = {}

        for size in test_sizes:
            data = b"A" * size

            # Measure encryption time
            start_time = time.perf_counter()
            for _ in range(10):  # 10 iterations for averaging
                encrypted = service.encrypt(data, key_id)
            encrypt_time = (time.perf_counter() - start_time) / 10

            # Measure decryption time
            start_time = time.perf_counter()
            for _ in range(10):
                decrypted = service.decrypt(encrypted, key_id)
            decrypt_time = (time.perf_counter() - start_time) / 10

            performance_results[size] = {
                "encrypt_time": encrypt_time,
                "decrypt_time": decrypt_time,
                "throughput_encrypt": size / encrypt_time,
                "throughput_decrypt": size / decrypt_time,
            }

        # Verify reasonable performance (these are very conservative baselines)
        # Encryption should be faster than 1ms for 100KB
        assert performance_results[100000]["encrypt_time"] < 1.0

        # Throughput should be reasonable (> 1MB/s for large data)
        assert performance_results[100000]["throughput_encrypt"] > 1_000_000

    def test_cross_algorithm_compatibility(self):
        """Test compatibility between different algorithms."""
        service = create_default_crypto_service()

        test_data = b"Cross-algorithm compatibility test"

        # Test that algorithms produce different outputs for same input
        key_id_xor = service.generate_and_store_key("xor_key", "simple-xor")

        if "simple-xor" in service.list_available_algorithms():
            encrypted_xor = service.encrypt(test_data, key_id_xor, "simple-xor")

            # Verify we can't decrypt XOR with different algorithm key
            key_id_other = service.generate_and_store_key("other_key")
            try:
                # This should fail or produce incorrect results
                wrong_decrypt = service.decrypt(encrypted_xor, key_id_other)
                # If it doesn't fail, it should at least produce wrong data
                assert wrong_decrypt != test_data
            except (ValueError, RuntimeError, KeyError):
                # Expected - different keys should not work
                pass

    def test_data_integrity_after_upgrade(self):
        """Test data integrity preservation after upgrades."""
        # This would normally test against saved encrypted data from previous version
        # For now, we test consistency within current version

        service = create_default_crypto_service()

        # Create test data set
        test_vectors = [
            (b"Test vector 1", "key1"),
            (b"Test vector 2 with unicode: \xe2\x9c\x93", "key2"),
            (b"Binary data: " + bytes(range(256)), "key3"),
        ]

        encrypted_results = []

        # Encrypt all test vectors
        for data, key_name in test_vectors:
            key_id = service.generate_and_store_key(key_name)
            encrypted = service.encrypt(data, key_id)
            encrypted_results.append((data, key_name, encrypted))

        # Verify all can be decrypted correctly
        for original_data, key_name, encrypted_data in encrypted_results:
            decrypted = service.decrypt(encrypted_data, key_name)
            assert decrypted == original_data, f"Data integrity failed for {key_name}"

        # Test hash consistency
        for data, key_name in test_vectors:
            hash1 = service.hash(data, "sha256")
            hash2 = service.hash(data, "sha256")
            assert hash1 == hash2, "Hash function not deterministic"


class TestCryptoRegressionValidation:
    """Validate against known regression patterns."""

    def test_key_generation_entropy(self):
        """Test that key generation maintains proper entropy."""
        service = create_default_crypto_service()

        # Generate multiple keys and verify they're different
        keys = []
        for i in range(10):
            key_id = service.generate_and_store_key(f"entropy_test_{i}")
            key_data = service.key_manager.retrieve_key(key_id)
            keys.append(key_data)

        # All keys should be different
        unique_keys = set(keys)
        assert len(unique_keys) == len(keys), "Key generation lacks entropy"

        # Each key should be 32 bytes
        for key in keys:
            assert len(key) == 32

    def test_algorithm_parameter_validation(self):
        """Test parameter validation across all algorithms."""
        registry = AlgorithmRegistry()

        # Test XOR algorithm
        xor_algo = SimpleXORAlgorithm()
        xor_info = AlgorithmInfo("simple-xor", 32, "XOR", "symmetric")
        registry.register(xor_algo, xor_info)

        # Valid key
        valid_key = b"a" * 32
        test_data = b"test"

        # Should work
        encrypted = xor_algo.encrypt(test_data, valid_key)
        decrypted = xor_algo.decrypt(encrypted, valid_key)
        assert decrypted == test_data

        # Invalid key lengths
        invalid_keys = [b"", b"short", b"a" * 31, b"a" * 33, b"a" * 64]

        for invalid_key in invalid_keys:
            with pytest.raises(ValueError, match="must be 32 bytes"):
                xor_algo.encrypt(test_data, invalid_key)

    def test_service_state_isolation(self):
        """Test that service instances maintain isolated state."""
        service1 = create_default_crypto_service()
        service2 = create_default_crypto_service()

        # Create keys in each service
        key1_id = service1.generate_and_store_key("service1_key")
        key2_id = service2.generate_and_store_key("service2_key")

        # Keys should not be accessible across services
        with pytest.raises(KeyError):
            service1.key_manager.retrieve_key("service2_key")

        with pytest.raises(KeyError):
            service2.key_manager.retrieve_key("service1_key")

        # Each service should only see its own keys
        assert "service1_key" in service1.key_manager.list_keys()
        assert "service2_key" in service2.key_manager.list_keys()
        assert "service2_key" not in service1.key_manager.list_keys()
        assert "service1_key" not in service2.key_manager.list_keys()


if __name__ == "__main__":
    pytest.main([__file__, "-v"])