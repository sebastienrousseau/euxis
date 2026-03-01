"""Tests for LlamaEngine with mocked llama_cpp."""

from __future__ import annotations

import sys
from types import ModuleType
from unittest.mock import MagicMock, patch

import pytest

from euxis_inference.core.config import LocalModelConfig
from euxis_inference.core.engine import InferenceUnavailableError


class TestLlamaEngine:
    def _make_engine(self, mock_llama_class: MagicMock) -> object:
        """Create a LlamaEngine with mocked llama_cpp module."""
        fake_module = ModuleType("llama_cpp")
        fake_module.Llama = mock_llama_class  # type: ignore[attr-defined]
        with patch.dict(sys.modules, {"llama_cpp": fake_module}):
            from euxis_inference.runtime.llama_engine import LlamaEngine
            config = LocalModelConfig(model_path="/models/test.gguf")
            return LlamaEngine(config)

    def test_generate_returns_result(self) -> None:
        mock_llama_cls = MagicMock()
        mock_instance = MagicMock()
        mock_llama_cls.return_value = mock_instance
        mock_instance.return_value = {
            "choices": [{"text": "Generated text output"}],
            "usage": {"completion_tokens": 10},
        }
        engine = self._make_engine(mock_llama_cls)
        fake_module = ModuleType("llama_cpp")
        fake_module.Llama = mock_llama_cls  # type: ignore[attr-defined]
        with patch.dict(sys.modules, {"llama_cpp": fake_module}):
            result = engine.generate("Hello", max_tokens=100)  # type: ignore[union-attr]
        assert result.text == "Generated text output"
        assert result.tokens_generated == 10
        assert result.engine_name == "llama-cpp"
        assert result.model_name == "/models/test.gguf"
        assert result.tokens_per_second >= 0

    def test_supports_gguf_model(self) -> None:
        mock_llama_cls = MagicMock()
        engine = self._make_engine(mock_llama_cls)
        assert engine.supports_model("model.gguf") is True  # type: ignore[union-attr]
        assert engine.supports_model("model.bin") is False  # type: ignore[union-attr]

    def test_health_before_load(self) -> None:
        mock_llama_cls = MagicMock()
        engine = self._make_engine(mock_llama_cls)
        health = engine.health()  # type: ignore[union-attr]
        assert health["engine"] == "llama-cpp"
        assert health["model_loaded"] is False

    def test_health_after_load(self) -> None:
        mock_llama_cls = MagicMock()
        mock_instance = MagicMock()
        mock_llama_cls.return_value = mock_instance
        mock_instance.return_value = {
            "choices": [{"text": "ok"}],
            "usage": {"completion_tokens": 1},
        }
        engine = self._make_engine(mock_llama_cls)
        fake_module = ModuleType("llama_cpp")
        fake_module.Llama = mock_llama_cls  # type: ignore[attr-defined]
        with patch.dict(sys.modules, {"llama_cpp": fake_module}):
            engine.generate("test")  # type: ignore[union-attr]
        health = engine.health()  # type: ignore[union-attr]
        assert health["model_loaded"] is True

    def test_unavailable_when_import_fails(self) -> None:
        with patch.dict(sys.modules, {"llama_cpp": None}):
            from euxis_inference.runtime.llama_engine import LlamaEngine
            config = LocalModelConfig(model_path="/models/test.gguf")
            engine = LlamaEngine(config)
            with pytest.raises(InferenceUnavailableError, match="llama-cpp-python is not installed"):
                engine.generate("test")
