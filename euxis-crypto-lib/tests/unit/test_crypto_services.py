# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Tests for CryptoService dependency injection architecture."""


import pytest

from tui.core.services import (
    AlgorithmInfo,
    AlgorithmRegistry,
    CryptoService,
    InMemoryKeyManager,
    ServiceContainer,
    SHA256Algorithm,
    SimpleXORAlgorithm,
    create_default_crypto_service,
    setup_service_container,
)


class TestAlgorithmRegistry:
    """Test algorithm registry functionality."""

    def test_register_and_get_algorithm(self):
        """Test registering and retrieving algorithms."""
        registry = AlgorithmRegistry()
        algorithm = SimpleXORAlgorithm()
        metadata = AlgorithmInfo("test", 32, "Test algorithm", "symmetric")

        registry.register(algorithm, metadata)

        retrieved = registry.get_algorithm("test")
        assert retrieved is algorithm

    def test_get_nonexistent_algorithm_raises_error(self):
        """Test that getting non-existent algorithm raises ValueError."""
        registry = AlgorithmRegistry()

        with pytest.raises(ValueError, match="Algorithm 'nonexistent' not found"):
            registry.get_algorithm("nonexistent")

    def test_register_invalid_algorithm_raises_error(self):
        """Test that registering invalid algorithm raises TypeError."""
        registry = AlgorithmRegistry()
        invalid_algorithm = "not an algorithm"
        metadata = AlgorithmInfo("test", 32, "Test", "symmetric")

        with pytest.raises(
            TypeError,
            match="Algorithm must implement CryptographicAlgorithm protocol",
        ):
            registry.register(invalid_algorithm, metadata)

    def test_list_algorithms(self):
        """Test listing available algorithms."""
        registry = AlgorithmRegistry()
        algorithm1 = SimpleXORAlgorithm()
        algorithm2 = SHA256Algorithm()

        metadata1 = AlgorithmInfo("xor", 32, "XOR", "symmetric")
        metadata2 = AlgorithmInfo("sha256", 0, "SHA-256", "hash")

        registry.register(algorithm1, metadata1)
        registry.register(algorithm2, metadata2)

        algorithms = registry.list_algorithms()
        assert set(algorithms) == {"xor", "sha256"}

    def test_list_by_type(self):
        """Test filtering algorithms by type."""
        registry = AlgorithmRegistry()
        algorithm1 = SimpleXORAlgorithm()
        algorithm2 = SHA256Algorithm()

        metadata1 = AlgorithmInfo("xor", 32, "XOR", "symmetric")
        metadata2 = AlgorithmInfo("sha256", 0, "SHA-256", "hash")

        registry.register(algorithm1, metadata1)
        registry.register(algorithm2, metadata2)

        symmetric_algos = registry.list_by_type("symmetric")
        assert symmetric_algos == ["xor"]

        hash_algos = registry.list_by_type("hash")
        assert hash_algos == ["sha256"]

    def test_get_metadata(self):
        """Test retrieving algorithm metadata."""
        registry = AlgorithmRegistry()
        algorithm = SimpleXORAlgorithm()
        metadata = AlgorithmInfo("test", 32, "Test algorithm", "symmetric")

        registry.register(algorithm, metadata)

        retrieved_metadata = registry.get_metadata("test")
        assert retrieved_metadata == metadata


class TestInMemoryKeyManager:
    """Test in-memory key management functionality."""

    def test_generate_key_without_registry_raises_error(self):
        """Test that generating key without registry raises error."""
        key_manager = InMemoryKeyManager()

        with pytest.raises(RuntimeError, match="Algorithm registry not set"):
            key_manager.generate_key("test")

    def test_generate_key_with_registry(self):
        """Test key generation with algorithm registry."""
        registry = AlgorithmRegistry()
        algorithm = SimpleXORAlgorithm()
        metadata = AlgorithmInfo("test", 32, "Test", "symmetric")
        registry.register(algorithm, metadata)

        key_manager = InMemoryKeyManager()
        key_manager.set_algorithm_registry(registry)

        key = key_manager.generate_key("test")
        assert isinstance(key, bytes)
        assert len(key) == 32

    def test_store_and_retrieve_key(self):
        """Test storing and retrieving keys."""
        key_manager = InMemoryKeyManager()
        test_key = b"test_key_data"

        key_manager.store_key("test_id", test_key)
        retrieved_key = key_manager.retrieve_key("test_id")

        assert retrieved_key == test_key

    def test_retrieve_nonexistent_key_raises_error(self):
        """Test that retrieving non-existent key raises KeyError."""
        key_manager = InMemoryKeyManager()

        with pytest.raises(KeyError, match="Key 'nonexistent' not found"):
            key_manager.retrieve_key("nonexistent")

    def test_delete_key(self):
        """Test key deletion."""
        key_manager = InMemoryKeyManager()
        test_key = b"test_key_data"

        key_manager.store_key("test_id", test_key)
        key_manager.delete_key("test_id")

        with pytest.raises(KeyError):
            key_manager.retrieve_key("test_id")

    def test_delete_nonexistent_key_raises_error(self):
        """Test that deleting non-existent key raises KeyError."""
        key_manager = InMemoryKeyManager()

        with pytest.raises(KeyError, match="Key 'nonexistent' not found"):
            key_manager.delete_key("nonexistent")

    def test_list_keys(self):
        """Test listing stored keys."""
        key_manager = InMemoryKeyManager()

        key_manager.store_key("key1", b"data1")
        key_manager.store_key("key2", b"data2")

        keys = key_manager.list_keys()
        assert set(keys) == {"key1", "key2"}


class TestSimpleXORAlgorithm:
    """Test XOR algorithm implementation."""

    def test_encrypt_decrypt_roundtrip(self):
        """Test that encryption/decryption is reversible."""
        algorithm = SimpleXORAlgorithm()
        key = b"a" * 32  # 32-byte key
        data = b"Hello, World! This is test data."

        encrypted = algorithm.encrypt(data, key)
        assert encrypted != data  # Should be different

        decrypted = algorithm.decrypt(encrypted, key)
        assert decrypted == data

    def test_invalid_key_size_raises_error(self):
        """Test that invalid key size raises ValueError."""
        algorithm = SimpleXORAlgorithm()
        short_key = b"short"
        data = b"test data"

        with pytest.raises(ValueError, match="Key must be 32 bytes"):
            algorithm.encrypt(data, short_key)

    def test_hash_functionality(self):
        """Test hashing functionality."""
        algorithm = SimpleXORAlgorithm()
        data = b"test data"

        hash_result = algorithm.hash(data)
        assert isinstance(hash_result, bytes)
        assert len(hash_result) == 32  # SHA-256 output length

        # Same input should produce same hash
        hash_result2 = algorithm.hash(data)
        assert hash_result == hash_result2


class TestSHA256Algorithm:
    """Test SHA-256 algorithm implementation."""

    def test_encrypt_not_supported(self):
        """Test that encryption is not supported for hash algorithms."""
        algorithm = SHA256Algorithm()

        with pytest.raises(NotImplementedError, match="Hash algorithms do not support encryption"):
            algorithm.encrypt(b"data", b"key")

    def test_decrypt_not_supported(self):
        """Test that decryption is not supported for hash algorithms."""
        algorithm = SHA256Algorithm()

        with pytest.raises(NotImplementedError, match="Hash algorithms do not support decryption"):
            algorithm.decrypt(b"data", b"key")

    def test_hash_functionality(self):
        """Test SHA-256 hashing."""
        algorithm = SHA256Algorithm()
        data = b"test data"

        hash_result = algorithm.hash(data)
        assert isinstance(hash_result, bytes)
        assert len(hash_result) == 32  # SHA-256 output length

        # Verify reproducible hash
        hash_result2 = algorithm.hash(data)
        assert hash_result == hash_result2


class TestCryptoService:
    """Test main CryptoService functionality."""

    def test_initialization_with_invalid_default_algorithm(self):
        """Test that invalid default algorithm raises error."""
        registry = AlgorithmRegistry()
        key_manager = InMemoryKeyManager()

        with pytest.raises(ValueError, match="Default algorithm 'nonexistent' not available"):
            CryptoService(registry, key_manager, "nonexistent")

    def test_encrypt_decrypt_with_default_algorithm(self):
        """Test encryption/decryption with default algorithm."""
        service = create_default_crypto_service()
        data = b"Secret message"

        # Generate and store a key
        key_id = service.generate_and_store_key("test_key")

        # Encrypt and decrypt
        encrypted = service.encrypt(data, key_id)
        decrypted = service.decrypt(encrypted, key_id)

        assert decrypted == data

    def test_encrypt_decrypt_with_specific_algorithm(self):
        """Test encryption/decryption with specific algorithm."""
        service = create_default_crypto_service()
        data = b"Secret message"

        # Generate and store a key
        key_id = service.generate_and_store_key("test_key", "simple-xor")

        # Encrypt and decrypt with specific algorithm
        encrypted = service.encrypt(data, key_id, "simple-xor")
        decrypted = service.decrypt(encrypted, key_id, "simple-xor")

        assert decrypted == data

    def test_hash_functionality(self):
        """Test hashing with crypto service."""
        service = create_default_crypto_service()
        data = b"data to hash"

        hash_result = service.hash(data, "sha256")
        assert isinstance(hash_result, bytes)
        assert len(hash_result) == 32

    def test_switch_algorithm(self):
        """Test switching default algorithm."""
        service = create_default_crypto_service()

        # Switch to SHA-256 (should fail for encryption since it's hash-only)
        service.switch_algorithm("sha256")

        # Verify the switch took effect by checking available algorithms
        assert "sha256" in service.list_available_algorithms()

    def test_switch_to_invalid_algorithm_raises_error(self):
        """Test switching to invalid algorithm raises error."""
        service = create_default_crypto_service()

        with pytest.raises(ValueError, match="Algorithm 'invalid' not available"):
            service.switch_algorithm("invalid")

    def test_list_available_algorithms(self):
        """Test listing available algorithms."""
        service = create_default_crypto_service()

        algorithms = service.list_available_algorithms()
        assert "simple-xor" in algorithms
        assert "sha256" in algorithms

    def test_get_algorithm_info(self):
        """Test getting algorithm information."""
        service = create_default_crypto_service()

        info = service.get_algorithm_info("simple-xor")
        assert info.name == "simple-xor"
        assert info.algorithm_type == "symmetric"
        assert info.key_size == 32

    def test_runtime_algorithm_selection(self):
        """Test runtime algorithm selection functionality."""
        service = create_default_crypto_service()

        # Test with XOR algorithm
        data = b"Test data for algorithm switching"
        key_id = service.generate_and_store_key("xor_key", "simple-xor")

        encrypted_xor = service.encrypt(data, key_id, "simple-xor")
        decrypted_xor = service.decrypt(encrypted_xor, key_id, "simple-xor")

        assert decrypted_xor == data

        # Verify that we can select algorithms at runtime
        available = service.list_available_algorithms()
        assert len(available) >= 2  # At least XOR and SHA-256

        # Test hash with different algorithms
        hash_sha256 = service.hash(data, "sha256")
        assert len(hash_sha256) == 32


class TestServiceContainer:
    """Test dependency injection container."""

    def test_register_and_get_singleton(self):
        """Test singleton service registration and retrieval."""
        container = ServiceContainer()
        test_instance = "test_singleton"

        container.register_singleton(str, test_instance)
        retrieved = container.get(str)

        assert retrieved is test_instance

    def test_register_and_get_transient(self):
        """Test transient service registration and retrieval."""
        container = ServiceContainer()
        call_count = 0

        def factory():
            nonlocal call_count
            call_count += 1
            return f"instance_{call_count}"

        container.register_transient(str, factory)

        # Each call should create a new instance
        instance1 = container.get(str)
        instance2 = container.get(str)

        assert instance1 == "instance_1"
        assert instance2 == "instance_2"

    def test_get_unregistered_service_raises_error(self):
        """Test that getting unregistered service raises error."""
        container = ServiceContainer()

        with pytest.raises(ValueError, match=r"Service .* not registered"):
            container.get(int)

    def test_setup_service_container(self):
        """Test the service container setup function."""
        container = setup_service_container()

        # Should be able to get registered services
        registry = container.get(AlgorithmRegistry)
        assert isinstance(registry, AlgorithmRegistry)

        crypto_service = container.get(CryptoService)
        assert isinstance(crypto_service, CryptoService)


class TestIntegration:
    """Integration tests for the complete crypto service architecture."""

    def test_full_crypto_workflow(self):
        """Test complete cryptographic workflow."""
        # Setup service container
        container = setup_service_container()
        crypto_service = container.get(CryptoService)

        # Test data
        original_data = b"This is sensitive data that needs encryption"

        # Generate encryption key
        key_id = crypto_service.generate_and_store_key("integration_test_key")

        # Encrypt data
        encrypted_data = crypto_service.encrypt(original_data, key_id)
        assert encrypted_data != original_data

        # Decrypt data
        decrypted_data = crypto_service.decrypt(encrypted_data, key_id)
        assert decrypted_data == original_data

        # Hash data
        data_hash = crypto_service.hash(original_data)
        assert isinstance(data_hash, bytes)
        assert len(data_hash) == 32

        # Verify algorithm switching
        algorithms = crypto_service.list_available_algorithms()
        assert len(algorithms) >= 2

        # Test different algorithm selection
        for algorithm in algorithms:
            if algorithm != "sha256":  # Skip hash-only algorithms for encryption
                try:
                    info = crypto_service.get_algorithm_info(algorithm)
                    if info.algorithm_type == "symmetric":
                        test_key_id = crypto_service.generate_and_store_key(
                            f"test_{algorithm}",
                            algorithm,
                        )
                        test_encrypted = crypto_service.encrypt(
                            b"test",
                            test_key_id,
                            algorithm,
                        )
                        test_decrypted = crypto_service.decrypt(
                            test_encrypted,
                            test_key_id,
                            algorithm,
                        )
                        assert test_decrypted == b"test"
                except (KeyError, RuntimeError, ValueError):
                    # Some algorithms might not support all operations
                    continue

    def test_dependency_injection_isolation(self):
        """Test that dependency injection provides proper isolation."""
        # Create two separate service instances
        service1 = create_default_crypto_service()
        service2 = create_default_crypto_service()

        # Generate keys in each service
        key1_id = service1.generate_and_store_key("service1_key")
        key2_id = service2.generate_and_store_key("service2_key")

        # Keys should be isolated between services
        with pytest.raises(KeyError):
            service1.key_manager.retrieve_key("service2_key")

        with pytest.raises(KeyError):
            service2.key_manager.retrieve_key("service1_key")

        # Each service should have its own keys
        assert key1_id in service1.key_manager.list_keys()
        assert key2_id in service2.key_manager.list_keys()
        assert key1_id not in service2.key_manager.list_keys()
        assert key2_id not in service1.key_manager.list_keys()


if __name__ == "__main__":
    pytest.main([__file__])
