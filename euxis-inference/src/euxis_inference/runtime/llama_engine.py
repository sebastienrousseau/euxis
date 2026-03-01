"""LlamaCpp inference engine wrapper."""

from __future__ import annotations

import time
from typing import Any

from euxis_inference.core.config import LocalModelConfig
from euxis_inference.core.engine import InferenceResult, InferenceUnavailableError


class LlamaEngine:
    """Wraps llama-cpp-python for local inference."""

    def __init__(self, config: LocalModelConfig) -> None:
        self.config = config
        self._model: Any = None

    def _ensure_model(self) -> Any:
        if self._model is not None:
            return self._model
        try:
            from llama_cpp import Llama  # type: ignore[import-untyped]
        except ImportError as exc:
            raise InferenceUnavailableError(
                "llama-cpp-python is not installed. Install with: pip install llama-cpp-python"
            ) from exc
        self._model = Llama(
            model_path=self.config.model_path,
            n_ctx=self.config.context_length,
            n_gpu_layers=self.config.gpu_layers,
        )
        return self._model

    def generate(self, prompt: str, max_tokens: int = 512) -> InferenceResult:
        model = self._ensure_model()
        start = time.monotonic()
        output = model(prompt, max_tokens=max_tokens)
        elapsed = time.monotonic() - start
        text = output["choices"][0]["text"]
        tokens = output["usage"]["completion_tokens"]
        tps = tokens / elapsed if elapsed > 0 else 0.0
        return InferenceResult(
            text=text, tokens_generated=tokens, tokens_per_second=round(tps, 2),
            engine_name="llama-cpp", model_name=self.config.model_path,
        )

    def supports_model(self, model_name: str) -> bool:
        return model_name.endswith(".gguf")

    def health(self) -> dict[str, Any]:
        return {"engine": "llama-cpp", "model_loaded": self._model is not None, "model_path": self.config.model_path}
