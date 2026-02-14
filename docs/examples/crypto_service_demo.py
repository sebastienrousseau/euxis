#!/usr/bin/env python3
# (c) 2026 Euxis Fleet. All rights reserved.
"""Demonstration of CryptoService dependency injection and runtime algorithm selection."""

import sys
import os
from pathlib import Path

# Add parent directories to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from tui.core.services import (
    create_default_crypto_service,
    setup_service_container,
    AlgorithmRegistry,
    AlgorithmInfo,
    SimpleXORAlgorithm,
    SHA256Algorithm,
    CryptoService,
)


def demonstrate_basic_usage():
    """Demonstrate basic CryptoService usage."""
    print("=== Basic CryptoService Usage ===")

    # Create crypto service with default configuration
    crypto_service = create_default_crypto_service()

    # Display available algorithms
    algorithms = crypto_service.list_available_algorithms()
    print(f"Available algorithms: {', '.join(algorithms)}")

    # Generate a key for encryption
    key_id = crypto_service.generate_and_store_key("demo_key")
    print(f"Generated key with ID: {key_id}")

    # Test data
    secret_data = b"This is confidential information that needs protection"
    print(f"Original data: {secret_data.decode()}")

    # Encrypt using default algorithm
    encrypted = crypto_service.encrypt(secret_data, key_id)
    print(f"Encrypted data (hex): {encrypted.hex()}")

    # Decrypt back to original
    decrypted = crypto_service.decrypt(encrypted, key_id)
    print(f"Decrypted data: {decrypted.decode()}")
    print(f"Decryption successful: {decrypted == secret_data}")
    print()


def demonstrate_runtime_algorithm_selection():
    """Demonstrate runtime algorithm selection capabilities."""
    print("=== Runtime Algorithm Selection ===")

    crypto_service = create_default_crypto_service()

    # Test data
    test_data = b"Data for algorithm selection demo"
    print(f"Test data: {test_data.decode()}")

    # Test with different algorithms
    algorithms = crypto_service.list_available_algorithms()

    for algorithm in algorithms:
        print(f"\n--- Testing algorithm: {algorithm} ---")

        # Get algorithm information
        try:
            info = crypto_service.get_algorithm_info(algorithm)
            print(f"Algorithm type: {info.algorithm_type}")
            print(f"Key size: {info.key_size} bytes")
            print(f"Description: {info.description}")

            if info.algorithm_type == "hash":
                # Hash algorithms don't use keys for hashing
                hash_result = crypto_service.hash(test_data, algorithm)
                print(f"Hash result (hex): {hash_result.hex()}")

            elif info.algorithm_type == "symmetric":
                # Generate key for this algorithm
                key_id = crypto_service.generate_and_store_key(f"key_{algorithm}", algorithm)

                # Encrypt and decrypt
                encrypted = crypto_service.encrypt(test_data, key_id, algorithm)
                decrypted = crypto_service.decrypt(encrypted, key_id, algorithm)

                print(f"Encryption successful: {encrypted != test_data}")
                print(f"Decryption successful: {decrypted == test_data}")
                print(f"Encrypted data (first 32 bytes, hex): {encrypted[:32].hex()}")

        except Exception as e:
            print(f"Error with algorithm {algorithm}: {e}")

    print()


def demonstrate_dependency_injection():
    """Demonstrate dependency injection container usage."""
    print("=== Dependency Injection Container ===")

    # Setup container with all services
    container = setup_service_container()

    # Get services from container
    registry = container.get(AlgorithmRegistry)
    crypto_service = container.get(CryptoService)

    print(f"Algorithm registry: {type(registry).__name__}")
    print(f"Crypto service: {type(crypto_service).__name__}")

    # Register a custom algorithm
    class ROT13Algorithm:
        """Simple ROT13 algorithm for demonstration."""

        @property
        def name(self) -> str:
            return "rot13"

        @property
        def key_size(self) -> int:
            return 0  # ROT13 doesn't use keys

        def encrypt(self, data: bytes, key: bytes) -> bytes:
            """ROT13 encode."""
            return self._rot13_transform(data)

        def decrypt(self, encrypted_data: bytes, key: bytes) -> bytes:
            """ROT13 decode (same as encode)."""
            return self._rot13_transform(encrypted_data)

        def hash(self, data: bytes) -> bytes:
            """Not applicable for ROT13."""
            raise NotImplementedError("ROT13 is not a hash algorithm")

        def _rot13_transform(self, data: bytes) -> bytes:
            """Apply ROT13 transformation to data."""
            result = bytearray()
            for byte in data:
                if 65 <= byte <= 90:  # A-Z
                    result.append(((byte - 65 + 13) % 26) + 65)
                elif 97 <= byte <= 122:  # a-z
                    result.append(((byte - 97 + 13) % 26) + 97)
                else:
                    result.append(byte)
            return bytes(result)

    # Register the custom algorithm
    rot13_algo = ROT13Algorithm()
    rot13_info = AlgorithmInfo(
        name="rot13",
        key_size=0,
        description="ROT13 character substitution cipher",
        algorithm_type="symmetric"
    )

    registry.register(rot13_algo, rot13_info)

    print("Registered custom ROT13 algorithm")

    # Test the custom algorithm
    test_text = b"Hello World"
    print(f"Original: {test_text.decode()}")

    # ROT13 doesn't use keys, but we still need to call it through the service
    # Since we're using CryptoService.encrypt/decrypt, we need a dummy key
    dummy_key_id = "dummy"
    crypto_service.key_manager.store_key(dummy_key_id, b"")

    encoded = crypto_service.encrypt(test_text, dummy_key_id, "rot13")
    decoded = crypto_service.decrypt(encoded, dummy_key_id, "rot13")

    print(f"ROT13 encoded: {encoded.decode()}")
    print(f"ROT13 decoded: {decoded.decode()}")
    print(f"Round-trip successful: {decoded == test_text}")

    print()


def demonstrate_algorithm_switching():
    """Demonstrate dynamic algorithm switching."""
    print("=== Dynamic Algorithm Switching ===")

    crypto_service = create_default_crypto_service()

    # Display current default algorithm
    print(f"Available algorithms: {crypto_service.list_available_algorithms()}")

    # Test data
    data = b"Switch between algorithms dynamically"
    print(f"Test data: {data.decode()}")

    # Generate keys for different algorithms
    symmetric_algorithms = []
    for algo in crypto_service.list_available_algorithms():
        info = crypto_service.get_algorithm_info(algo)
        if info.algorithm_type == "symmetric":
            symmetric_algorithms.append(algo)

    print(f"Symmetric algorithms for testing: {symmetric_algorithms}")

    # Test switching between algorithms
    for algorithm in symmetric_algorithms:
        print(f"\n--- Switching to {algorithm} ---")

        try:
            # Switch default algorithm
            crypto_service.switch_algorithm(algorithm)

            # Generate key for current algorithm
            key_id = crypto_service.generate_and_store_key(f"switch_test_{algorithm}")

            # Encrypt with current default (no algorithm parameter)
            encrypted = crypto_service.encrypt(data, key_id)
            decrypted = crypto_service.decrypt(encrypted, key_id)

            print(f"Algorithm switch successful: {algorithm}")
            print(f"Encryption/decryption successful: {decrypted == data}")

        except Exception as e:
            print(f"Error switching to {algorithm}: {e}")

    print()


def demonstrate_key_management():
    """Demonstrate key management features."""
    print("=== Key Management Features ===")

    crypto_service = create_default_crypto_service()

    # Generate multiple keys
    key_ids = []
    for i in range(3):
        key_id = crypto_service.generate_and_store_key(f"managed_key_{i}")
        key_ids.append(key_id)

    print(f"Generated keys: {key_ids}")
    print(f"Stored keys: {crypto_service.key_manager.list_keys()}")

    # Test encryption with different keys
    data = b"Testing key management"

    for key_id in key_ids:
        encrypted = crypto_service.encrypt(data, key_id)
        decrypted = crypto_service.decrypt(encrypted, key_id)
        print(f"Key {key_id}: encryption/decryption successful = {decrypted == data}")

    # Clean up keys
    for key_id in key_ids[:2]:  # Keep one key
        crypto_service.key_manager.delete_key(key_id)

    print(f"After cleanup: {crypto_service.key_manager.list_keys()}")
    print()


def main():
    """Main demonstration function."""
    print("CryptoService Dependency Injection and Runtime Algorithm Selection Demo")
    print("=" * 70)

    try:
        demonstrate_basic_usage()
        demonstrate_runtime_algorithm_selection()
        demonstrate_dependency_injection()
        demonstrate_algorithm_switching()
        demonstrate_key_management()

        print("Demo completed successfully!")

    except Exception as e:
        print(f"Demo failed with error: {e}")
        import traceback
        traceback.print_exc()
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())