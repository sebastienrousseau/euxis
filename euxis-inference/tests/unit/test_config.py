"""Tests for LocalModelConfig."""

from euxis_inference.core.config import LocalModelConfig


class TestLocalModelConfig:
    def test_defaults(self) -> None:
        cfg = LocalModelConfig(model_path="/models/llama.gguf")
        assert cfg.model_path == "/models/llama.gguf"
        assert cfg.context_length == 4096
        assert cfg.gpu_layers == 0
        assert cfg.quantization == "Q4_K_M"
        assert cfg.max_tokens == 512

    def test_custom_values(self) -> None:
        cfg = LocalModelConfig(
            model_path="/tmp/model.gguf",
            context_length=8192,
            gpu_layers=35,
            quantization="Q8_0",
            max_tokens=1024,
        )
        assert cfg.context_length == 8192
        assert cfg.gpu_layers == 35
        assert cfg.quantization == "Q8_0"
        assert cfg.max_tokens == 1024

    def test_is_frozen(self) -> None:
        cfg = LocalModelConfig(model_path="/models/test.gguf")
        try:
            cfg.model_path = "changed"  # type: ignore[misc]
            raise AssertionError("Expected FrozenInstanceError")
        except AttributeError:
            pass
