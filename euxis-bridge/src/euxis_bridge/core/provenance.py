"""Supply chain provenance tracking (SLSA v1.0 compatible)."""

from __future__ import annotations

import hashlib
import json
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


@dataclass(frozen=True, slots=True)
class ProvenanceRecord:
    subject_name: str
    subject_digest: str
    builder_id: str
    source_uri: str
    timestamp: str
    materials: tuple[dict[str, str], ...] = ()


class ProvenanceChain:
    """Generate and verify SLSA v1.0 compatible provenance records."""

    def __init__(self, builder_id: str = "euxis-bridge") -> None:
        self.builder_id = builder_id

    def _hash_directory(self, skill_dir: Path) -> str:
        hasher = hashlib.sha256()
        for path in sorted(skill_dir.rglob("*")):
            if path.is_file():
                hasher.update(path.read_bytes())
        return hasher.hexdigest()

    def _collect_materials(self, skill_dir: Path) -> tuple[dict[str, str], ...]:
        materials: list[dict[str, str]] = []
        for path in sorted(skill_dir.rglob("*")):
            if path.is_file():
                digest = hashlib.sha256(path.read_bytes()).hexdigest()
                materials.append({"uri": str(path.relative_to(skill_dir)), "digest": f"sha256:{digest}"})
        return tuple(materials)

    def generate(self, skill_name: str, skill_dir: Path, source_uri: str = "") -> ProvenanceRecord:
        return ProvenanceRecord(
            subject_name=skill_name,
            subject_digest=f"sha256:{self._hash_directory(skill_dir)}",
            builder_id=self.builder_id,
            source_uri=source_uri,
            timestamp=datetime.now(tz=timezone.utc).isoformat(),
            materials=self._collect_materials(skill_dir),
        )

    def verify(self, record: ProvenanceRecord, skill_dir: Path) -> bool:
        current_digest = f"sha256:{self._hash_directory(skill_dir)}"
        return record.subject_digest == current_digest

    def export_bundle(self, record: ProvenanceRecord) -> dict[str, Any]:
        return {
            "_type": "https://in-toto.io/Statement/v1",
            "predicateType": "https://slsa.dev/provenance/v1",
            "subject": [{"name": record.subject_name, "digest": {"sha256": record.subject_digest.removeprefix("sha256:")}}],
            "predicate": {
                "buildDefinition": {"buildType": "euxis-bridge-import", "externalParameters": {"source": record.source_uri}},
                "runDetails": {"builder": {"id": record.builder_id}, "metadata": {"invocationId": record.timestamp}},
            },
            "materials": [asdict(m) if hasattr(m, '__dataclass_fields__') else m for m in record.materials],
        }
