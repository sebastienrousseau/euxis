"""Import OpenClaw/ClawHub skill bundles into Euxis bridge records."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from euxis_bridge.core.models import BridgedSkill
from euxis_bridge.core.parser import parse_openclaw_json, parse_skill_md


@dataclass(slots=True, frozen=True)
class ImportReport:
    imported: int
    skipped: int
    source: str
    output: str


class ClawHubImporter:
    """Import skill bundles and emit normalized registry records."""

    def __init__(self, source_root: Path) -> None:
        self.source_root = source_root

    def _entrypoint_from_manifest(self, manifest: dict[str, Any], base_dir: Path) -> str:
        entrypoint = manifest.get("entrypoint") or manifest.get("main") or "index.js"
        return str((base_dir / str(entrypoint)).resolve())

    def _from_skill_md(self, skill_md: Path) -> BridgedSkill:
        manifest = parse_skill_md(skill_md)
        name = str(manifest.get("name") or skill_md.parent.name)
        slug = str(manifest.get("slug") or skill_md.parent.name)
        description = str(manifest.get("description") or "OpenClaw skill import")
        runtime = str(manifest.get("runtime") or "node")
        entrypoint = self._entrypoint_from_manifest(manifest, skill_md.parent)
        tags_raw = manifest.get("tags")
        tags: tuple[str, ...] = tuple(tags_raw) if isinstance(tags_raw, list) else tuple()

        command = ["node", entrypoint] if runtime == "node" else []
        return BridgedSkill(
            name=name,
            slug=slug,
            source_dir=str(skill_md.parent.resolve()),
            description=description,
            runtime=runtime,
            entrypoint=entrypoint,
            command=command,
            tags=tags,
            metadata={"origin": "SKILL.md", "raw": manifest},
        )

    def _from_openclaw_json(self, path: Path) -> BridgedSkill:
        manifest = parse_openclaw_json(path)
        name = str(manifest.get("name") or path.parent.name)
        slug = str(manifest.get("slug") or name.lower().replace(" ", "-"))
        description = str(manifest.get("description") or "OpenClaw config import")
        runtime = str(manifest.get("runtime") or "node")
        entrypoint = self._entrypoint_from_manifest(manifest, path.parent)

        command = ["node", entrypoint] if runtime == "node" else []
        return BridgedSkill(
            name=name,
            slug=slug,
            source_dir=str(path.parent.resolve()),
            description=description,
            runtime=runtime,
            entrypoint=entrypoint,
            command=command,
            tags=tuple(),
            metadata={"origin": "openclaw.json", "raw": manifest},
        )

    def import_skills(self) -> list[BridgedSkill]:
        if not self.source_root.exists():
            return []

        imported: list[BridgedSkill] = []
        seen_dirs: set[Path] = set()

        for skill_md in sorted(self.source_root.rglob("SKILL.md")):
            imported.append(self._from_skill_md(skill_md))
            seen_dirs.add(skill_md.parent.resolve())

        for config in sorted(self.source_root.rglob("openclaw.json")):
            cfg_dir = config.parent.resolve()
            if cfg_dir in seen_dirs:
                continue
            imported.append(self._from_openclaw_json(config))

        return imported

    def export_registry(self, output_path: Path) -> ImportReport:
        skills = self.import_skills()
        payload = {
            "version": 1,
            "source": str(self.source_root.resolve()),
            "imported": [item.to_dict() for item in skills],
        }
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

        return ImportReport(
            imported=len(skills),
            skipped=0,
            source=str(self.source_root.resolve()),
            output=str(output_path.resolve()),
        )
