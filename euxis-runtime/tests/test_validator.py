# SPDX-License-Identifier: AGPL-3.0-or-later

from __future__ import annotations

import json
from pathlib import Path

import pytest

from runtime.validator import RuntimeValidationError, RuntimeValidator, validate_runtime_layout


def _make_runtime(tmp_path: Path) -> Path:
    root = tmp_path / "runtime"
    (root / "config").mkdir(parents=True)
    (root / "data" / "perf").mkdir(parents=True)
    (root / "data" / "lifecycle").mkdir(parents=True)
    (root / "data" / "manifests").mkdir(parents=True)

    (root / "README.md").write_text("runtime", encoding="utf-8")
    (root / "config" / "etx-settings.json").write_text("{}", encoding="utf-8")
    (root / "data" / "perf" / "metrics.jsonl").write_text(
        json.dumps({"ts": "2026-01-01T00:00:00Z", "op": "x", "agent": "a", "ms": 1}) + "\n",
        encoding="utf-8",
    )
    (root / "data" / "lifecycle" / "transitions.jsonl").write_text(
        json.dumps({"ts": "2026-01-01T00:00:00Z", "agent": "a", "state": "active", "session": "s1"}) + "\n",
        encoding="utf-8",
    )
    (root / "data" / "lifecycle" / "architect.state").write_text("active", encoding="utf-8")
    (root / "data" / "manifests" / "performance-optimization.json").write_text("{}", encoding="utf-8")
    return root


def test_validate_runtime_layout_success(tmp_path: Path) -> None:
    root = _make_runtime(tmp_path)
    validate_runtime_layout(root)


def test_missing_required_file_raises(tmp_path: Path) -> None:
    root = _make_runtime(tmp_path)
    (root / "README.md").unlink()
    with pytest.raises(RuntimeValidationError, match="Missing runtime assets"):
        RuntimeValidator(root).ensure_required_files()


def test_invalid_jsonl_record_raises(tmp_path: Path) -> None:
    root = _make_runtime(tmp_path)
    (root / "data" / "perf" / "metrics.jsonl").write_text("{\n", encoding="utf-8")
    with pytest.raises(RuntimeValidationError, match="invalid jsonl record"):
        RuntimeValidator(root).validate_perf_metrics()


def test_non_object_jsonl_record_raises(tmp_path: Path) -> None:
    root = _make_runtime(tmp_path)
    (root / "data" / "perf" / "metrics.jsonl").write_text('"x"\n', encoding="utf-8")
    with pytest.raises(RuntimeValidationError, match="record is not an object"):
        RuntimeValidator(root).validate_perf_metrics()


def test_empty_jsonl_raises(tmp_path: Path) -> None:
    root = _make_runtime(tmp_path)
    (root / "data" / "perf" / "metrics.jsonl").write_text("\n", encoding="utf-8")
    with pytest.raises(RuntimeValidationError, match="no records"):
        RuntimeValidator(root).validate_perf_metrics()


def test_missing_metric_keys_and_bad_ms_raise(tmp_path: Path) -> None:
    root = _make_runtime(tmp_path)
    (root / "data" / "perf" / "metrics.jsonl").write_text(
        json.dumps({"ts": "x", "op": "y", "agent": "z"}) + "\n",
        encoding="utf-8",
    )
    with pytest.raises(RuntimeValidationError, match="missing keys: ms"):
        RuntimeValidator(root).validate_perf_metrics()

    (root / "data" / "perf" / "metrics.jsonl").write_text(
        json.dumps({"ts": "x", "op": "y", "agent": "z", "ms": -1}) + "\n",
        encoding="utf-8",
    )
    with pytest.raises(RuntimeValidationError, match="invalid ms value"):
        RuntimeValidator(root).validate_perf_metrics()


def test_transition_keys_missing_raise(tmp_path: Path) -> None:
    root = _make_runtime(tmp_path)
    (root / "data" / "lifecycle" / "transitions.jsonl").write_text(
        json.dumps({"ts": "x", "agent": "a"}) + "\n",
        encoding="utf-8",
    )
    with pytest.raises(RuntimeValidationError, match="missing keys"):
        RuntimeValidator(root).validate_lifecycle_transitions()


def test_lifecycle_state_files_required_and_non_empty(tmp_path: Path) -> None:
    root = _make_runtime(tmp_path)
    state = root / "data" / "lifecycle" / "architect.state"
    state.unlink()
    with pytest.raises(RuntimeValidationError, match="no \\.state files found"):
        RuntimeValidator(root).validate_lifecycle_state_files()

    state.write_text("", encoding="utf-8")
    with pytest.raises(RuntimeValidationError, match="empty state file"):
        RuntimeValidator(root).validate_lifecycle_state_files()


def test_manifest_validation(tmp_path: Path) -> None:
    root = _make_runtime(tmp_path)
    manifest = root / "data" / "manifests" / "performance-optimization.json"
    manifest.write_text("{", encoding="utf-8")
    with pytest.raises(RuntimeValidationError, match="invalid json"):
        RuntimeValidator(root).validate_manifest()
    manifest.write_text("[]", encoding="utf-8")
    with pytest.raises(RuntimeValidationError, match="manifest must be an object"):
        RuntimeValidator(root).validate_manifest()

