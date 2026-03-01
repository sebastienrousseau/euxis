"""Local model configuration."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class LocalModelConfig:
    model_path: str
    context_length: int = 4096
    gpu_layers: int = 0
    quantization: str = "Q4_K_M"
    max_tokens: int = 512
