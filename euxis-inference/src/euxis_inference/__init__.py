"""Public package API for euxis-inference."""

from .core.config import LocalModelConfig
from .core.engine import InferenceEngine, InferenceResult, InferenceUnavailableError
from .core.model_registry import ModelMetadata, ModelRegistry
from .core.quality_gate import QualityGate, QualityScore
from .core.service import CoreService
from .platform.factory import resolve_platform_ops
from .runtime.llama_engine import LlamaEngine
from .runtime.ollama_engine import OllamaEngine

__all__ = [
    "CoreService", "InferenceEngine", "InferenceResult",
    "InferenceUnavailableError", "LlamaEngine", "LocalModelConfig",
    "ModelMetadata", "ModelRegistry", "OllamaEngine", "QualityGate",
    "QualityScore", "resolve_platform_ops",
]
