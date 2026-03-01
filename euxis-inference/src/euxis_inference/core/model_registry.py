"""Local model discovery and registry."""

from __future__ import annotations

import hashlib
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True, slots=True)
class ModelMetadata:
    name: str
    size_bytes: int
    quantization: str
    context_length: int
    sha256: str


class ModelRegistry:
    """Discover and manage local GGUF models."""

    def __init__(self, model_dir: Path) -> None:
        self._dir = model_dir
        self._models: dict[str, ModelMetadata] = {}

    def discover(self) -> list[ModelMetadata]:
        self._models.clear()
        if not self._dir.exists():
            return []
        found: list[ModelMetadata] = []
        for path in sorted(self._dir.glob("*.gguf")):
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            quant = self._detect_quantization(path.stem)
            meta = ModelMetadata(
                name=path.stem,
                size_bytes=path.stat().st_size,
                quantization=quant,
                context_length=4096,
                sha256=digest,
            )
            self._models[path.stem] = meta
            found.append(meta)
        return found

    def _detect_quantization(self, stem: str) -> str:
        for q in ("Q8_0", "Q6_K", "Q5_K_M", "Q5_K_S", "Q4_K_M", "Q4_K_S", "Q4_0", "Q3_K_M"):
            if q.lower() in stem.lower():
                return q
        return "unknown"

    def get(self, name: str) -> ModelMetadata | None:
        return self._models.get(name)

    def list_models(self) -> list[ModelMetadata]:
        return list(self._models.values())

    def verify_checksum(self, name: str, expected_sha256: str) -> bool:
        meta = self._models.get(name)
        if meta is None:
            return False
        return meta.sha256 == expected_sha256
