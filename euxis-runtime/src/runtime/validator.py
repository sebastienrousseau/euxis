# SPDX-License-Identifier: AGPL-3.0-or-later

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


class RuntimeValidationError(ValueError):
    """Raised when runtime assets fail structural validation."""


@dataclass(frozen=True)
class RuntimeValidator:
    """Validate runtime package data/config artifacts."""

    runtime_root: Path

    def ensure_required_files(self) -> None:
        required = [
            self.runtime_root / "README.md",
            self.runtime_root / "config" / "etx-settings.json",
            self.runtime_root / "data" / "perf" / "metrics.jsonl",
            self.runtime_root / "data" / "lifecycle" / "transitions.jsonl",
            self.runtime_root / "data" / "manifests" / "performance-optimization.json",
        ]
        missing = [str(p) for p in required if not p.exists()]
        if missing:
            raise RuntimeValidationError(f"Missing runtime assets: {', '.join(missing)}")

    def _read_jsonl(self, path: Path) -> list[dict]:
        records: list[dict] = []
        for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            if not raw.strip():
                continue
            try:
                obj = json.loads(raw)
            except json.JSONDecodeError as exc:
                raise RuntimeValidationError(f"{path}:{lineno}: invalid jsonl record") from exc
            if not isinstance(obj, dict):
                raise RuntimeValidationError(f"{path}:{lineno}: record is not an object")
            records.append(obj)
        if not records:
            raise RuntimeValidationError(f"{path}: no records")
        return records

    def _assert_keys(self, records: Iterable[dict], required: set[str], context: str) -> None:
        for idx, record in enumerate(records):
            missing = required.difference(record.keys())
            if missing:
                ordered = ", ".join(sorted(missing))
                raise RuntimeValidationError(f"{context}[{idx}] missing keys: {ordered}")

    def validate_perf_metrics(self) -> None:
        path = self.runtime_root / "data" / "perf" / "metrics.jsonl"
        records = self._read_jsonl(path)
        self._assert_keys(records, {"ts", "op", "agent", "ms"}, str(path))
        for idx, record in enumerate(records):
            ms = record.get("ms")
            if not isinstance(ms, int | float) or ms < 0:
                raise RuntimeValidationError(f"{path}[{idx}] invalid ms value")

    def validate_lifecycle_transitions(self) -> None:
        path = self.runtime_root / "data" / "lifecycle" / "transitions.jsonl"
        records = self._read_jsonl(path)
        self._assert_keys(records, {"ts", "agent", "state", "session"}, str(path))

    def validate_lifecycle_state_files(self) -> None:
        lifecycle_dir = self.runtime_root / "data" / "lifecycle"
        state_files = sorted(lifecycle_dir.glob("*.state"))
        if not state_files:
            raise RuntimeValidationError(f"{lifecycle_dir}: no .state files found")
        for path in state_files:
            raw = path.read_text(encoding="utf-8").strip()
            if not raw:
                raise RuntimeValidationError(f"{path}: empty state file")

    def validate_manifest(self) -> None:
        path = self.runtime_root / "data" / "manifests" / "performance-optimization.json"
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            raise RuntimeValidationError(f"{path}: invalid json") from exc
        if not isinstance(data, dict):
            raise RuntimeValidationError(f"{path}: manifest must be an object")

    def validate(self) -> None:
        self.ensure_required_files()
        self.validate_perf_metrics()
        self.validate_lifecycle_transitions()
        self.validate_lifecycle_state_files()
        self.validate_manifest()


def validate_runtime_layout(runtime_root: str | Path) -> None:
    RuntimeValidator(Path(runtime_root)).validate()

