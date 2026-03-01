"""Tests for ModelRegistry discovery and checksum verification."""

import hashlib
from pathlib import Path

from euxis_inference.core.model_registry import ModelRegistry


class TestModelRegistry:
    def test_discover_finds_gguf_files(self, tmp_path: Path) -> None:
        model_file = tmp_path / "llama-q4_k_m.gguf"
        model_file.write_bytes(b"fake model data")
        registry = ModelRegistry(tmp_path)
        models = registry.discover()
        assert len(models) == 1
        assert models[0].name == "llama-q4_k_m"
        assert models[0].quantization == "Q4_K_M"
        assert models[0].size_bytes == len(b"fake model data")

    def test_discover_empty_dir(self, tmp_path: Path) -> None:
        registry = ModelRegistry(tmp_path)
        models = registry.discover()
        assert models == []

    def test_discover_nonexistent_dir(self, tmp_path: Path) -> None:
        registry = ModelRegistry(tmp_path / "nonexistent")
        models = registry.discover()
        assert models == []

    def test_discover_detects_quantizations(self, tmp_path: Path) -> None:
        for q in ("q8_0", "q5_k_m", "q4_0"):
            (tmp_path / f"model-{q}.gguf").write_bytes(b"data")
        registry = ModelRegistry(tmp_path)
        models = registry.discover()
        quants = {m.quantization for m in models}
        assert "Q8_0" in quants
        assert "Q5_K_M" in quants
        assert "Q4_0" in quants

    def test_discover_unknown_quantization(self, tmp_path: Path) -> None:
        (tmp_path / "custom-model.gguf").write_bytes(b"data")
        registry = ModelRegistry(tmp_path)
        models = registry.discover()
        assert models[0].quantization == "unknown"

    def test_get_after_discover(self, tmp_path: Path) -> None:
        (tmp_path / "test-model.gguf").write_bytes(b"data")
        registry = ModelRegistry(tmp_path)
        registry.discover()
        meta = registry.get("test-model")
        assert meta is not None
        assert meta.name == "test-model"

    def test_get_missing(self, tmp_path: Path) -> None:
        registry = ModelRegistry(tmp_path)
        registry.discover()
        assert registry.get("nonexistent") is None

    def test_list_models(self, tmp_path: Path) -> None:
        (tmp_path / "a.gguf").write_bytes(b"aa")
        (tmp_path / "b.gguf").write_bytes(b"bb")
        registry = ModelRegistry(tmp_path)
        registry.discover()
        names = [m.name for m in registry.list_models()]
        assert "a" in names
        assert "b" in names

    def test_verify_checksum_pass(self, tmp_path: Path) -> None:
        content = b"model bytes"
        (tmp_path / "verified.gguf").write_bytes(content)
        expected = hashlib.sha256(content).hexdigest()
        registry = ModelRegistry(tmp_path)
        registry.discover()
        assert registry.verify_checksum("verified", expected) is True

    def test_verify_checksum_fail(self, tmp_path: Path) -> None:
        (tmp_path / "verified.gguf").write_bytes(b"model bytes")
        registry = ModelRegistry(tmp_path)
        registry.discover()
        assert registry.verify_checksum("verified", "wrong_hash") is False

    def test_verify_checksum_missing_model(self, tmp_path: Path) -> None:
        registry = ModelRegistry(tmp_path)
        registry.discover()
        assert registry.verify_checksum("nonexistent", "abc") is False

    def test_sha256_computed_correctly(self, tmp_path: Path) -> None:
        content = b"hello world gguf"
        (tmp_path / "check.gguf").write_bytes(content)
        expected = hashlib.sha256(content).hexdigest()
        registry = ModelRegistry(tmp_path)
        models = registry.discover()
        assert models[0].sha256 == expected
