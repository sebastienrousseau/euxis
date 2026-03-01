"""Ollama inference engine wrapper."""

from __future__ import annotations

import time
from typing import Any

from euxis_inference.core.engine import InferenceResult, InferenceUnavailableError


class OllamaEngine:
    """Wraps Ollama REST API via httpx."""

    def __init__(self, model_name: str = "llama3", base_url: str = "http://localhost:11434") -> None:
        self.model_name = model_name
        self.base_url = base_url
        self._client: Any = None

    def _ensure_client(self) -> Any:
        if self._client is not None:
            return self._client
        try:
            import httpx  # type: ignore[import-untyped]
        except ImportError as exc:
            raise InferenceUnavailableError(
                "httpx is not installed. Install with: pip install httpx"
            ) from exc
        self._client = httpx.Client(base_url=self.base_url, timeout=120.0)
        return self._client

    def generate(self, prompt: str, max_tokens: int = 512) -> InferenceResult:
        client = self._ensure_client()
        start = time.monotonic()
        resp = client.post("/api/generate", json={
            "model": self.model_name,
            "prompt": prompt,
            "stream": False,
            "options": {"num_predict": max_tokens},
        })
        resp.raise_for_status()
        data = resp.json()
        elapsed = time.monotonic() - start
        text = data.get("response", "")
        tokens = data.get("eval_count", len(text.split()))
        tps = tokens / elapsed if elapsed > 0 else 0.0
        return InferenceResult(
            text=text, tokens_generated=tokens, tokens_per_second=round(tps, 2),
            engine_name="ollama", model_name=self.model_name,
        )

    def supports_model(self, model_name: str) -> bool:
        return True

    def health(self) -> dict[str, Any]:
        try:
            client = self._ensure_client()
            resp = client.get("/api/tags")
            return {"engine": "ollama", "status": "ok", "models": resp.json().get("models", [])}
        except Exception as exc:
            return {"engine": "ollama", "status": "error", "error": str(exc)}
