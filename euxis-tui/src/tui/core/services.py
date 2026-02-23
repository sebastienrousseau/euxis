# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Core service infrastructure for dependency injection and CryptoService."""

from __future__ import annotations

import abc
import hashlib
import secrets
from dataclasses import dataclass
from typing import Any, Protocol, runtime_checkable

# Import crypto bridge for production-grade AES implementation
try:
    from shared.crypto_bridge import decrypt as crypto_decrypt
    from shared.crypto_bridge import encrypt as crypto_encrypt
    from shared.crypto_bridge import crypto_errors, is_available
    _errors = crypto_errors()
    if not isinstance(_errors, tuple) or len(_errors) != 3:
        raise RuntimeError("crypto_errors() returned unexpected shape")  # pragma: no cover
except Exception:  # pragma: no cover - fallback for docs/minimal environments
    class _FallbackCryptoError(Exception):
        """Fallback crypto exception when bridge is unavailable."""

    CryptoError = _FallbackCryptoError
    DecryptionError = _FallbackCryptoError
    InvalidKeyError = _FallbackCryptoError

    def crypto_encrypt(*_args: object, **_kwargs: object) -> Any:
        raise RuntimeError("shared.crypto_bridge is unavailable")

    def crypto_decrypt(*_args: object, **_kwargs: object) -> Any:
        raise RuntimeError("shared.crypto_bridge is unavailable")

    def is_available() -> bool:
        return False
else:
    CryptoError, DecryptionError, InvalidKeyError = _errors

CRYPTO_LIB_AVAILABLE = is_available()


@runtime_checkable
class CryptographicAlgorithm(Protocol):
    """Protocol defining interface for cryptographic algorithms."""

    def encrypt(self, data: bytes, key: bytes) -> bytes:
        """Encrypt data with the given key."""
        ...  # pragma: no cover

    def decrypt(self, encrypted_data: bytes, key: bytes) -> bytes:
        """Decrypt data with the given key."""
        ...  # pragma: no cover

    def hash(self, data: bytes) -> bytes:
        """Hash data and return digest."""
        ...  # pragma: no cover

    @property
    def name(self) -> str:
        """Return algorithm name."""
        ...  # pragma: no cover

    @property
    def key_size(self) -> int:
        """Return required key size in bytes."""
        ...  # pragma: no cover


@dataclass(frozen=True)
class AlgorithmInfo:
    """Metadata about a cryptographic algorithm."""

    name: str
    key_size: int
    description: str
    algorithm_type: str  # "symmetric", "asymmetric", "hash"


class AlgorithmRegistry:
    """Registry for managing cryptographic algorithms."""

    def __init__(self) -> None:
        self._algorithms: dict[str, CryptographicAlgorithm] = {}
        self._metadata: dict[str, AlgorithmInfo] = {}

    def register(self, algorithm: CryptographicAlgorithm, metadata: AlgorithmInfo) -> None:
        """Register a cryptographic algorithm with metadata."""
        if not isinstance(algorithm, CryptographicAlgorithm):
            msg = "Algorithm must implement CryptographicAlgorithm protocol"
            raise TypeError(msg)

        self._algorithms[metadata.name] = algorithm
        self._metadata[metadata.name] = metadata

    def get_algorithm(self, name: str) -> CryptographicAlgorithm:
        """Retrieve algorithm by name."""
        if name not in self._algorithms:
            available = ", ".join(self._algorithms.keys())
            msg = f"Algorithm '{name}' not found. Available: {available}"
            raise ValueError(msg)

        return self._algorithms[name]

    def get_metadata(self, name: str) -> AlgorithmInfo:
        """Get metadata for algorithm by name."""
        if name not in self._metadata:
            msg = f"Algorithm metadata for '{name}' not found"
            raise ValueError(msg)

        return self._metadata[name]

    def list_algorithms(self) -> list[str]:
        """Return list of available algorithm names."""
        return list(self._algorithms.keys())

    def list_by_type(self, algorithm_type: str) -> list[str]:
        """Return algorithms of specific type."""
        return [
            name for name, metadata in self._metadata.items()
            if metadata.algorithm_type == algorithm_type
        ]


class KeyManagementService(abc.ABC):
    """Abstract base class for key management services."""

    @abc.abstractmethod
    def generate_key(self, algorithm: str) -> bytes:
        """Generate a new key for the specified algorithm."""

    @abc.abstractmethod
    def store_key(self, key_id: str, key: bytes) -> None:
        """Store a key with given identifier."""

    @abc.abstractmethod
    def retrieve_key(self, key_id: str) -> bytes:
        """Retrieve stored key by identifier."""

    @abc.abstractmethod
    def delete_key(self, key_id: str) -> None:
        """Delete stored key."""

    @abc.abstractmethod
    def list_keys(self) -> list[str]:
        """Return list of stored key identifiers."""


class InMemoryKeyManager(KeyManagementService):
    """Simple in-memory key management implementation."""

    def __init__(self) -> None:
        self._keys: dict[str, bytes] = {}
        self._algorithm_registry: AlgorithmRegistry | None = None

    def set_algorithm_registry(self, registry: AlgorithmRegistry) -> None:
        """Set reference to algorithm registry for key generation."""
        self._algorithm_registry = registry

    def generate_key(self, algorithm: str) -> bytes:
        """Generate a new key for the specified algorithm."""
        if not self._algorithm_registry:
            msg = "Algorithm registry not set"
            raise RuntimeError(msg)

        metadata = self._algorithm_registry.get_metadata(algorithm)
        return secrets.token_bytes(metadata.key_size)

    def store_key(self, key_id: str, key: bytes) -> None:
        """Store a key with given identifier."""
        self._keys[key_id] = key

    def retrieve_key(self, key_id: str) -> bytes:
        """Retrieve stored key by identifier."""
        if key_id not in self._keys:
            msg = f"Key '{key_id}' not found"
            raise KeyError(msg)
        return self._keys[key_id]

    def delete_key(self, key_id: str) -> None:
        """Delete stored key."""
        if key_id not in self._keys:
            msg = f"Key '{key_id}' not found"
            raise KeyError(msg)
        del self._keys[key_id]

    def list_keys(self) -> list[str]:
        """Return list of stored key identifiers."""
        return list(self._keys.keys())


# Concrete algorithm implementations
class SimpleXORAlgorithm:
    """Simple XOR encryption for demonstration purposes."""

    @property
    def name(self) -> str:
        """Return algorithm name."""
        return "simple-xor"

    @property
    def key_size(self) -> int:
        """Return required key size in bytes."""
        return 32  # 256 bits

    def encrypt(self, data: bytes, key: bytes) -> bytes:
        """XOR encrypt data with key."""
        if len(key) != self.key_size:
            msg = f"Key must be {self.key_size} bytes"
            raise ValueError(msg)

        result = bytearray(data)
        for i in range(len(result)):
            result[i] ^= key[i % len(key)]
        return bytes(result)

    def decrypt(self, encrypted_data: bytes, key: bytes) -> bytes:
        """XOR decrypt (same as encrypt for XOR)."""
        return self.encrypt(encrypted_data, key)

    def hash(self, data: bytes) -> bytes:
        """SHA-256 hash of data."""
        return hashlib.sha256(data).digest()


class SHA256Algorithm:
    """SHA-256 hashing algorithm."""

    @property
    def name(self) -> str:
        """Return algorithm name."""
        return "sha256"

    @property
    def key_size(self) -> int:
        """Return required key size in bytes."""
        return 0  # Hash algorithms don't use keys

    def encrypt(self, data: bytes, key: bytes) -> bytes:
        """Not applicable for hash algorithms."""
        msg = "Hash algorithms do not support encryption"
        raise NotImplementedError(msg)

    def decrypt(self, encrypted_data: bytes, key: bytes) -> bytes:
        """Not applicable for hash algorithms."""
        msg = "Hash algorithms do not support decryption"
        raise NotImplementedError(msg)

    def hash(self, data: bytes) -> bytes:
        """SHA-256 hash of data."""
        return hashlib.sha256(data).digest()


class AES256GCMAlgorithm:
    """AES-256-GCM encryption using crypto_lib core functions."""

    @property
    def name(self) -> str:
        """Return algorithm name."""
        return "aes-256-gcm"

    @property
    def key_size(self) -> int:
        """Return required key size in bytes."""
        return 32  # 256 bits

    def encrypt(self, data: bytes, key: bytes) -> bytes:
        """Encrypt data using AES-256-GCM from crypto_lib."""
        if not CRYPTO_LIB_AVAILABLE:
            msg = "crypto_lib not available for AES-256-GCM encryption"
            raise RuntimeError(msg)

        try:
            result = crypto_encrypt(data, key, "AES-256-GCM")
            # Convert EncryptionResult to bytes for the protocol interface
            # Use the base64 format for serialization
            return result.to_base64().encode("utf-8")
        except (CryptoError, InvalidKeyError) as exc:
            msg = f"AES-256-GCM encryption failed: {exc}"
            raise RuntimeError(msg) from exc

    def decrypt(self, encrypted_data: bytes, key: bytes) -> bytes:
        """Decrypt data using AES-256-GCM from crypto_lib."""
        if not CRYPTO_LIB_AVAILABLE:
            msg = "crypto_lib not available for AES-256-GCM decryption"
            raise RuntimeError(msg)

        try:
            # Convert bytes back to string for crypto_lib parsing
            encrypted_str = encrypted_data.decode("utf-8")
            result = crypto_decrypt(encrypted_str, key)
        except (CryptoError, InvalidKeyError, DecryptionError, UnicodeDecodeError) as exc:
            msg = f"AES-256-GCM decryption failed: {exc}"
            raise RuntimeError(msg) from exc
        else:
            return result.plaintext

    def hash(self, data: bytes) -> bytes:
        """SHA-256 hash of data."""
        return hashlib.sha256(data).digest()


class CryptoService:
    """Main cryptographic service with dependency injection."""

    def __init__(
        self,
        algorithm_registry: AlgorithmRegistry,
        key_manager: KeyManagementService,
        default_algorithm: str = "simple-xor"
    ) -> None:
        self._algorithm_registry = algorithm_registry
        self._key_manager = key_manager
        self._default_algorithm = default_algorithm

        # Validate default algorithm exists
        if default_algorithm not in algorithm_registry.list_algorithms():
            available = ", ".join(algorithm_registry.list_algorithms())
            msg = f"Default algorithm '{default_algorithm}' not available. Available: {available}"
            raise ValueError(msg)

    @property
    def algorithm_registry(self) -> AlgorithmRegistry:
        """Access to algorithm registry."""
        return self._algorithm_registry

    @property
    def key_manager(self) -> KeyManagementService:
        """Access to key management service."""
        return self._key_manager

    def encrypt(
        self,
        data: bytes,
        key_id: str,
        algorithm: str | None = None
    ) -> bytes:
        """Encrypt data using specified algorithm and key."""
        algo_name = algorithm or self._default_algorithm
        algorithm_impl = self._algorithm_registry.get_algorithm(algo_name)
        key = self._key_manager.retrieve_key(key_id)

        return algorithm_impl.encrypt(data, key)

    def decrypt(
        self,
        encrypted_data: bytes,
        key_id: str,
        algorithm: str | None = None
    ) -> bytes:
        """Decrypt data using specified algorithm and key."""
        algo_name = algorithm or self._default_algorithm
        algorithm_impl = self._algorithm_registry.get_algorithm(algo_name)
        key = self._key_manager.retrieve_key(key_id)

        return algorithm_impl.decrypt(encrypted_data, key)

    def hash(self, data: bytes, algorithm: str = "sha256") -> bytes:
        """Hash data using specified algorithm."""
        algorithm_impl = self._algorithm_registry.get_algorithm(algorithm)
        return algorithm_impl.hash(data)

    def generate_and_store_key(self, key_id: str, algorithm: str | None = None) -> str:
        """Generate and store a new key for the specified algorithm."""
        algo_name = algorithm or self._default_algorithm
        key = self._key_manager.generate_key(algo_name)
        self._key_manager.store_key(key_id, key)
        return key_id

    def switch_algorithm(self, new_default: str) -> None:
        """Switch the default algorithm."""
        if new_default not in self._algorithm_registry.list_algorithms():
            available = ", ".join(self._algorithm_registry.list_algorithms())
            msg = f"Algorithm '{new_default}' not available. Available: {available}"
            raise ValueError(msg)

        self._default_algorithm = new_default

    def list_available_algorithms(self) -> list[str]:
        """Return list of available algorithms."""
        return self._algorithm_registry.list_algorithms()

    def get_algorithm_info(self, algorithm: str) -> AlgorithmInfo:
        """Get information about a specific algorithm."""
        return self._algorithm_registry.get_metadata(algorithm)


class ServiceContainer:
    """Simple dependency injection container."""

    def __init__(self) -> None:
        self._services: dict[type, Any] = {}
        self._singletons: dict[type, Any] = {}

    def register_singleton(self, service_type: type, instance: Any) -> None:
        """Register a singleton instance."""
        self._singletons[service_type] = instance

    def register_transient(self, service_type: type, factory: callable) -> None:
        """Register a transient service factory."""
        self._services[service_type] = factory

    def get(self, service_type: type) -> Any:
        """Get service instance."""
        # Check singletons first
        if service_type in self._singletons:
            return self._singletons[service_type]

        # Check transient services
        if service_type in self._services:
            return self._services[service_type]()

        msg = f"Service {service_type} not registered"
        raise ValueError(msg)


def create_default_crypto_service() -> CryptoService:
    """Create a CryptoService with default implementations."""
    # Create algorithm registry and register default algorithms
    registry = AlgorithmRegistry()

    # Register XOR algorithm
    xor_algo = SimpleXORAlgorithm()
    xor_info = AlgorithmInfo(
        name="simple-xor",
        key_size=32,
        description="Simple XOR encryption for demonstration",
        algorithm_type="symmetric"
    )
    registry.register(xor_algo, xor_info)

    # Register SHA-256 hash
    sha256_algo = SHA256Algorithm()
    sha256_info = AlgorithmInfo(
        name="sha256",
        key_size=0,
        description="SHA-256 cryptographic hash function",
        algorithm_type="hash"
    )
    registry.register(sha256_algo, sha256_info)

    # Register AES-256-GCM if available
    if CRYPTO_LIB_AVAILABLE:
        aes_algo = AES256GCMAlgorithm()
        aes_info = AlgorithmInfo(
            name="aes-256-gcm",
            key_size=32,
            description="AES-256-GCM authenticated encryption",
            algorithm_type="symmetric"
        )
        registry.register(aes_algo, aes_info)

    # Create key manager
    key_manager = InMemoryKeyManager()
    key_manager.set_algorithm_registry(registry)

    # Prefer AES-256-GCM as default if available, otherwise use XOR
    default_algorithm = "aes-256-gcm" if CRYPTO_LIB_AVAILABLE else "simple-xor"

    # Create and return crypto service
    return CryptoService(registry, key_manager, default_algorithm)


def setup_service_container() -> ServiceContainer:
    """Set up the service container with crypto services."""
    container = ServiceContainer()

    # Create and register algorithm registry as singleton
    registry = AlgorithmRegistry()

    # Register default algorithms in the registry
    xor_algo = SimpleXORAlgorithm()
    xor_info = AlgorithmInfo(
        name="simple-xor",
        key_size=32,
        description="Simple XOR encryption for demonstration",
        algorithm_type="symmetric"
    )
    registry.register(xor_algo, xor_info)

    sha256_algo = SHA256Algorithm()
    sha256_info = AlgorithmInfo(
        name="sha256",
        key_size=0,
        description="SHA-256 cryptographic hash function",
        algorithm_type="hash"
    )
    registry.register(sha256_algo, sha256_info)

    # Register AES-256-GCM if crypto_lib is available
    if CRYPTO_LIB_AVAILABLE:
        aes_algo = AES256GCMAlgorithm()
        aes_info = AlgorithmInfo(
            name="aes-256-gcm",
            key_size=32,
            description="AES-256-GCM authenticated encryption",
            algorithm_type="symmetric"
        )
        registry.register(aes_algo, aes_info)

    container.register_singleton(AlgorithmRegistry, registry)

    # Create and register key manager as singleton
    key_manager = InMemoryKeyManager()
    key_manager.set_algorithm_registry(registry)
    container.register_singleton(KeyManagementService, key_manager)

    # Register crypto service factory that uses container dependencies
    def crypto_service_factory() -> CryptoService:
        registry = container.get(AlgorithmRegistry)
        key_manager = container.get(KeyManagementService)
        # Use AES-256-GCM as default if available, otherwise simple-xor
        default_algorithm = "aes-256-gcm" if CRYPTO_LIB_AVAILABLE else "simple-xor"
        return CryptoService(registry, key_manager, default_algorithm)

    container.register_transient(CryptoService, crypto_service_factory)

    return container
