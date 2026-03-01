"""Bridge models for imported OpenClaw/ClawHub skills."""

from __future__ import annotations

from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any


@dataclass(slots=True, frozen=True)
class BridgedSkill:
    """Normalized skill descriptor imported into Euxis runtime."""

    name: str
    slug: str
    source_dir: str
    description: str
    runtime: str
    entrypoint: str
    command: list[str] = field(default_factory=list)
    tags: tuple[str, ...] = field(default_factory=tuple)
    metadata: dict[str, Any] = field(default_factory=dict)
    signature_path: str | None = field(default=None)
    output_schema: dict[str, Any] | None = field(default=None)

    def to_dict(self) -> dict[str, Any]:
        payload = asdict(self)
        payload["tags"] = list(self.tags)
        return payload

    @property
    def source_path(self) -> Path:
        return Path(self.source_dir)
