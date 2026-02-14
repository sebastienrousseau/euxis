# Dependency injection architecture for crypto services (Python version)
from abc import ABC, abstractmethod
from typing import Dict, List, Optional, Tuple, Any
from dataclasses import dataclass

# Configuration class for crypto services
@dataclass
class CryptoConfig:
    default_encryption_algorithm: str
    default_hash_algorithm: str
    default_signature_algorithm: str
    key_rotation_interval: int
    enable_audit_logging: bool

# Algorithm registry interface for runtime algorithm selection
class AlgorithmRegistry(ABC):
    @abstractmethod
    def register_encryption_algorithm(self, name: str, algorithm: Any) -> None:
        pass

    @abstractmethod
    def register_hash_algorithm(self, name: str, algorithm: Any) -> None:
        pass

    @abstractmethod
    def register_signature_algorithm(self, name: str, algorithm: Any) -> None:
        pass

    @abstractmethod
    def get_encryption_algorithm(self, name: str) -> Optional[Any]:
        pass

    @abstractmethod
    def get_hash_algorithm(self, name: str) -> Optional[Any]:
        pass

    @abstractmethod
    def get_signature_algorithm(self, name: str) -> Optional[Any]:
        pass

    @abstractmethod
    def list_available_algorithms(self) -> Dict[str, List[str]]:
        pass

# Key management service interface
class KeyManagementService(ABC):
    @abstractmethod
    async def generate_key(self) -> str:
        pass

    @abstractmethod
    async def generate_key_pair(self) -> Tuple[str, str]:
        pass

    @abstractmethod
    async def rotate_key(self, key_id: str) -> str:
        pass

    @abstractmethod
    async def get_key(self, key_id: str) -> Optional[str]:
        pass

    @abstractmethod
    async def revoke_key(self, key_id: str) -> None:
        pass

    @abstractmethod
    async def list_active_keys(self) -> List[str]:
        pass

# Main crypto service with dependency injection
class CryptoService:
    def __init__(self,
                 config: CryptoConfig,
                 algorithm_registry: AlgorithmRegistry,
                 key_management_service: KeyManagementService):
        self.config = config
        self.algorithm_registry = algorithm_registry
        self.key_management_service = key_management_service

    async def encrypt(self, data: str, algorithm_name: Optional[str] = None) -> Dict[str, Any]:
        algorithm = (algorithm_name if algorithm_name
                    else self.config.default_encryption_algorithm)

        if not self.algorithm_registry.get_encryption_algorithm(algorithm):
            raise ValueError(f"Encryption algorithm not found: {algorithm}")

        key = await self.key_management_service.generate_key()

        # Implementation would use actual crypto operations here
        return {
            "cipher_text": f"encrypted_{data}_{algorithm}",
            "algorithm": algorithm,
            "iv": "generated_iv",
            "auth_tag": "generated_auth_tag"
        }

    async def decrypt(self, cipher_text: str, algorithm_name: Optional[str] = None) -> Dict[str, Any]:
        algorithm = (algorithm_name if algorithm_name
                    else self.config.default_encryption_algorithm)

        if not self.algorithm_registry.get_encryption_algorithm(algorithm):
            raise ValueError(f"Encryption algorithm not found: {algorithm}")

        # Implementation would use actual crypto operations here
        return {
            "plain_text": f"decrypted_{cipher_text}",
            "algorithm": algorithm
        }

    async def hash_data(self, input_data: str, algorithm_name: Optional[str] = None) -> Dict[str, Any]:
        algorithm = (algorithm_name if algorithm_name
                    else self.config.default_hash_algorithm)

        if not self.algorithm_registry.get_hash_algorithm(algorithm):
            raise ValueError(f"Hash algorithm not found: {algorithm}")

        # Implementation would use actual hash operations here
        return {
            "hash": f"hash_{input_data}_{algorithm}",
            "algorithm": algorithm
        }

    async def sign(self, data: str, private_key: str, algorithm_name: Optional[str] = None) -> Dict[str, Any]:
        algorithm = (algorithm_name if algorithm_name
                    else self.config.default_signature_algorithm)

        if not self.algorithm_registry.get_signature_algorithm(algorithm):
            raise ValueError(f"Signature algorithm not found: {algorithm}")

        # Implementation would use actual signing operations here
        return {
            "signature": f"signature_{data}_{algorithm}",
            "algorithm": algorithm,
            "data": data
        }

    async def verify(self, signature: str, public_key: str, algorithm_name: Optional[str] = None) -> Dict[str, Any]:
        algorithm = (algorithm_name if algorithm_name
                    else self.config.default_signature_algorithm)

        if not self.algorithm_registry.get_signature_algorithm(algorithm):
            raise ValueError(f"Signature algorithm not found: {algorithm}")

        # Implementation would use actual verification here
        return {
            "is_valid": True,
            "signature": signature,
            "algorithm": algorithm
        }

    async def rotate_keys(self) -> None:
        active_keys = await self.key_management_service.list_active_keys()
        for key_id in active_keys:
            await self.key_management_service.rotate_key(key_id)

    def get_available_algorithms(self) -> Dict[str, List[str]]:
        return self.algorithm_registry.list_available_algorithms()

    def update_config(self, **kwargs) -> None:
        for key, value in kwargs.items():
            if hasattr(self.config, key):
                setattr(self.config, key, value)