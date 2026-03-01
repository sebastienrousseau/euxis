"""Tests for OllamaEngine with mocked httpx."""

from __future__ import annotations

import sys
from types import ModuleType
from unittest.mock import MagicMock, patch

import pytest

from euxis_inference.core.engine import InferenceUnavailableError


class TestOllamaEngine:
    def _make_engine(self, mock_client_cls: MagicMock) -> object:
        """Create an OllamaEngine with mocked httpx module."""
        fake_module = ModuleType("httpx")
        fake_module.Client = mock_client_cls  # type: ignore[attr-defined]
        with patch.dict(sys.modules, {"httpx": fake_module}):
            from euxis_inference.runtime.ollama_engine import OllamaEngine
            return OllamaEngine(model_name="llama3", base_url="http://localhost:11434")

    def test_generate_calls_api(self) -> None:
        mock_client_cls = MagicMock()
        mock_client = MagicMock()
        mock_client_cls.return_value = mock_client
        mock_resp = MagicMock()
        mock_resp.json.return_value = {
            "response": "Hello from Ollama",
            "eval_count": 8,
        }
        mock_client.post.return_value = mock_resp
        engine = self._make_engine(mock_client_cls)
        fake_module = ModuleType("httpx")
        fake_module.Client = mock_client_cls  # type: ignore[attr-defined]
        with patch.dict(sys.modules, {"httpx": fake_module}):
            result = engine.generate("Hi there", max_tokens=256)  # type: ignore[union-attr]
        assert result.text == "Hello from Ollama"
        assert result.tokens_generated == 8
        assert result.engine_name == "ollama"
        assert result.model_name == "llama3"
        mock_client.post.assert_called_once()
        call_args = mock_client.post.call_args
        assert call_args[0][0] == "/api/generate"
        payload = call_args[1]["json"]
        assert payload["model"] == "llama3"
        assert payload["prompt"] == "Hi there"
        assert payload["stream"] is False
        assert payload["options"]["num_predict"] == 256

    def test_supports_any_model(self) -> None:
        mock_client_cls = MagicMock()
        engine = self._make_engine(mock_client_cls)
        assert engine.supports_model("llama3") is True  # type: ignore[union-attr]
        assert engine.supports_model("anything") is True  # type: ignore[union-attr]

    def test_health_success(self) -> None:
        mock_client_cls = MagicMock()
        mock_client = MagicMock()
        mock_client_cls.return_value = mock_client
        mock_resp = MagicMock()
        mock_resp.json.return_value = {"models": [{"name": "llama3"}]}
        mock_client.get.return_value = mock_resp
        engine = self._make_engine(mock_client_cls)
        fake_module = ModuleType("httpx")
        fake_module.Client = mock_client_cls  # type: ignore[attr-defined]
        with patch.dict(sys.modules, {"httpx": fake_module}):
            health = engine.health()  # type: ignore[union-attr]
        assert health["engine"] == "ollama"
        assert health["status"] == "ok"
        assert health["models"] == [{"name": "llama3"}]

    def test_health_error(self) -> None:
        mock_client_cls = MagicMock()
        mock_client = MagicMock()
        mock_client_cls.return_value = mock_client
        mock_client.get.side_effect = ConnectionError("refused")
        engine = self._make_engine(mock_client_cls)
        fake_module = ModuleType("httpx")
        fake_module.Client = mock_client_cls  # type: ignore[attr-defined]
        with patch.dict(sys.modules, {"httpx": fake_module}):
            health = engine.health()  # type: ignore[union-attr]
        assert health["engine"] == "ollama"
        assert health["status"] == "error"
        assert "refused" in health["error"]

    def test_unavailable_when_import_fails(self) -> None:
        with patch.dict(sys.modules, {"httpx": None}):
            from euxis_inference.runtime.ollama_engine import OllamaEngine
            engine = OllamaEngine()
            with pytest.raises(InferenceUnavailableError, match="httpx is not installed"):
                engine.generate("test")
