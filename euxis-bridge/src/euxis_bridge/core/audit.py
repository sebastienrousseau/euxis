"""Append-only JSONL audit logger for bridge skill lifecycle events."""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


@dataclass(frozen=True, slots=True)
class AuditEntry:
    """Immutable audit log entry."""

    timestamp: str
    event: str
    skill_slug: str
    entrypoint: str
    success: bool
    details: dict[str, Any] = field(default_factory=dict)

    def to_json(self) -> str:
        payload = {k: v for k, v in asdict(self).items() if v or isinstance(v, bool)}
        return json.dumps(payload, separators=(",", ":"), sort_keys=True)


class AuditLogger:
    """Append-only JSONL audit logger."""

    def __init__(self, log_path: str) -> None:
        self._path = Path(log_path)
        self._path.parent.mkdir(parents=True, exist_ok=True)

    def _append(self, entry: AuditEntry) -> None:
        with self._path.open("a", encoding="utf-8") as fh:
            fh.write(entry.to_json() + "\n")

    def _now(self) -> str:
        return datetime.now(tz=timezone.utc).isoformat()

    def log_verify(self, skill_slug: str, entrypoint: str, success: bool) -> None:
        self._append(AuditEntry(
            timestamp=self._now(),
            event="verify",
            skill_slug=skill_slug,
            entrypoint=entrypoint,
            success=success,
        ))

    def log_execute(self, skill_slug: str, entrypoint: str) -> None:
        self._append(AuditEntry(
            timestamp=self._now(),
            event="execute",
            skill_slug=skill_slug,
            entrypoint=entrypoint,
            success=True,
        ))

    def log_complete(
        self, skill_slug: str, entrypoint: str, details: dict[str, Any] | None = None,
    ) -> None:
        self._append(AuditEntry(
            timestamp=self._now(),
            event="complete",
            skill_slug=skill_slug,
            entrypoint=entrypoint,
            success=True,
            details=details or {},
        ))

    def log_error(
        self, skill_slug: str, entrypoint: str, details: dict[str, Any] | None = None,
    ) -> None:
        self._append(AuditEntry(
            timestamp=self._now(),
            event="error",
            skill_slug=skill_slug,
            entrypoint=entrypoint,
            success=False,
            details=details or {},
        ))
