"""Inference engine protocol and result types."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Protocol


@dataclass(frozen=True, slots=True)
class InferenceResult:
    text: str
    tokens_generated: int
    tokens_per_second: float
    engine_name: str
    model_name: str


class InferenceEngine(Protocol):
    def generate(self, prompt: str, max_tokens: int = 512) -> InferenceResult: ...
    def supports_model(self, model_name: str) -> bool: ...
    def health(self) -> dict[str, Any]: ...


class InferenceUnavailableError(RuntimeError):
    """Raised when an inference backend is not available."""
