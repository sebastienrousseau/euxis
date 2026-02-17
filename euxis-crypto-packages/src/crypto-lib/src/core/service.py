# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Dependency injection architecture for crypto services."""
from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Any


# Configuration class for crypto services
@dataclass
class CryptoConfig:
    """Configuration for crypto services."""

    default_encryption_algorithm: str
    default_hash_algorithm: str
    default_signature_algorithm: str
    key_rotation_interval: int
    enable_audit_logging: bool

# Algorithm registry interface for runtime algorithm selection
class AlgorithmRegistry(ABC):
    """Registry interface for cryptographic algorithms."""

    @abstractmethod
    def register_encryption_algorithm(self, name: str, algorithm: Any) -> None:
        """Register an encryption algorithm."""

    @abstractmethod
    def register_hash_algorithm(self, name: str, algorithm: Any) -> None:
        """Register a hash algorithm."""

    @abstractmethod
    def register_signature_algorithm(self, name: str, algorithm: Any) -> None:
        """Register a signature algorithm."""

    @abstractmethod
    def get_encryption_algorithm(self, name: str) -> Any | None:
        """Return an encryption algorithm by name."""

    @abstractmethod
    def get_hash_algorithm(self, name: str) -> Any | None:
        """Return a hash algorithm by name."""

    @abstractmethod
    def get_signature_algorithm(self, name: str) -> Any | None:
        """Return a signature algorithm by name."""

    @abstractmethod
    def list_available_algorithms(self) -> dict[str, list[str]]:
        """List all available algorithms by category."""

# Key management service interface
class KeyManagementService(ABC):
    """Key management service interface."""

    @abstractmethod
    async def generate_key(self) -> str:
        """Generate a new key."""

    @abstractmethod
    async def generate_key_pair(self) -> tuple[str, str]:
        """Generate a new key pair."""

    @abstractmethod
    async def rotate_key(self, key_id: str) -> str:
        """Rotate a key and return the new key ID."""

    @abstractmethod
    async def get_key(self, key_id: str) -> str | None:
        """Return a key by ID."""

    @abstractmethod
    async def revoke_key(self, key_id: str) -> None:
        """Revoke a key by ID."""

    @abstractmethod
    async def list_active_keys(self) -> list[str]:
        """List active key IDs."""

# Main crypto service with dependency injection
class CryptoService:
    """Crypto service orchestrating algorithms and keys."""

    def __init__(self,
                 config: CryptoConfig,
                 algorithm_registry: AlgorithmRegistry,
                 key_management_service: KeyManagementService) -> None:
        """Initialize the crypto service."""
        self.config = config
        self.algorithm_registry = algorithm_registry
        self.key_management_service = key_management_service

    async def encrypt(self, data: str, algorithm_name: str | None = None) -> dict[str, Any]:
        """Encrypt data using the configured algorithm."""
        algorithm = (algorithm_name if algorithm_name
                    else self.config.default_encryption_algorithm)

        if not self.algorithm_registry.get_encryption_algorithm(algorithm):
            msg = f"Encryption algorithm not found: {algorithm}"
            raise ValueError(msg)

        await self.key_management_service.generate_key()

        # Implementation would use actual crypto operations here
        return {
            "cipher_text": f"encrypted_{data}_{algorithm}",
            "algorithm": algorithm,
            "iv": "generated_iv",
            "auth_tag": "generated_auth_tag"
        }

    async def decrypt(self, cipher_text: str, algorithm_name: str | None = None) -> dict[str, Any]:
        """Decrypt data using the configured algorithm."""
        algorithm = (algorithm_name if algorithm_name
                    else self.config.default_encryption_algorithm)

        if not self.algorithm_registry.get_encryption_algorithm(algorithm):
            msg = f"Encryption algorithm not found: {algorithm}"
            raise ValueError(msg)

        # Implementation would use actual crypto operations here
        return {
            "plain_text": f"decrypted_{cipher_text}",
            "algorithm": algorithm
        }

    async def hash_data(self, input_data: str, algorithm_name: str | None = None) -> dict[str, Any]:
        """Hash data using the configured algorithm."""
        algorithm = (algorithm_name if algorithm_name
                    else self.config.default_hash_algorithm)

        if not self.algorithm_registry.get_hash_algorithm(algorithm):
            msg = f"Hash algorithm not found: {algorithm}"
            raise ValueError(msg)

        # Implementation would use actual hash operations here
        return {
            "hash": f"hash_{input_data}_{algorithm}",
            "algorithm": algorithm
        }

    async def sign(
        self,
        data: str,
        _private_key: str,
        algorithm_name: str | None = None,
    ) -> dict[str, Any]:
        """Sign data using the configured algorithm."""
        algorithm = (algorithm_name if algorithm_name
                    else self.config.default_signature_algorithm)

        if not self.algorithm_registry.get_signature_algorithm(algorithm):
            msg = f"Signature algorithm not found: {algorithm}"
            raise ValueError(msg)

        # Implementation would use actual signing operations here
        return {
            "signature": f"signature_{data}_{algorithm}",
            "algorithm": algorithm,
            "data": data
        }

    async def verify(
        self,
        signature: str,
        _public_key: str,
        algorithm_name: str | None = None,
    ) -> dict[str, Any]:
        """Verify a signature using the configured algorithm."""
        algorithm = (algorithm_name if algorithm_name
                    else self.config.default_signature_algorithm)

        if not self.algorithm_registry.get_signature_algorithm(algorithm):
            msg = f"Signature algorithm not found: {algorithm}"
            raise ValueError(msg)

        # Implementation would use actual verification here
        return {
            "is_valid": True,
            "signature": signature,
            "algorithm": algorithm
        }

    async def rotate_keys(self) -> None:
        """Rotate all active keys."""
        active_keys = await self.key_management_service.list_active_keys()
        for key_id in active_keys:
            await self.key_management_service.rotate_key(key_id)

    def get_available_algorithms(self) -> dict[str, list[str]]:
        """Return available algorithms by category."""
        return self.algorithm_registry.list_available_algorithms()

    def update_config(self, **kwargs: Any) -> None:
        """Update configuration with allowed values."""
        for key, value in kwargs.items():
            if hasattr(self.config, key):
                setattr(self.config, key, value)
