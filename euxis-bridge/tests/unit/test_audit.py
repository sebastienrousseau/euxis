"""Tests for the JSONL audit logger."""

import json
from pathlib import Path

from euxis_bridge.core.audit import AuditEntry, AuditLogger


def test_audit_entry_to_json_excludes_empty_details() -> None:
    entry = AuditEntry(
        timestamp="2026-03-01T00:00:00+00:00",
        event="verify",
        skill_slug="browser-use",
        entrypoint="/tmp/index.js",
        success=True,
    )
    parsed = json.loads(entry.to_json())
    assert "details" not in parsed
    assert parsed["success"] is True
    assert parsed["event"] == "verify"


def test_audit_entry_to_json_includes_details() -> None:
    entry = AuditEntry(
        timestamp="2026-03-01T00:00:00+00:00",
        event="error",
        skill_slug="memory",
        entrypoint="/tmp/run.js",
        success=False,
        details={"error": "timeout"},
    )
    parsed = json.loads(entry.to_json())
    assert parsed["details"] == {"error": "timeout"}
    assert parsed["success"] is False


def test_audit_logger_creates_file(tmp_path: Path) -> None:
    log_path = tmp_path / "audit.jsonl"
    logger = AuditLogger(str(log_path))
    logger.log_verify("test-skill", "/tmp/entry.js", success=True)
    assert log_path.exists()

    lines = log_path.read_text(encoding="utf-8").strip().splitlines()
    assert len(lines) == 1
    parsed = json.loads(lines[0])
    assert parsed["event"] == "verify"


def test_audit_logger_appends(tmp_path: Path) -> None:
    log_path = tmp_path / "audit.jsonl"
    logger = AuditLogger(str(log_path))
    logger.log_execute("s1", "/a.js")
    logger.log_complete("s1", "/a.js")
    logger.log_error("s1", "/a.js", {"err": "fail"})

    lines = log_path.read_text(encoding="utf-8").strip().splitlines()
    assert len(lines) == 3
    assert json.loads(lines[0])["event"] == "execute"
    assert json.loads(lines[1])["event"] == "complete"
    assert json.loads(lines[2])["event"] == "error"


def test_audit_logger_creates_parent_dirs(tmp_path: Path) -> None:
    log_path = tmp_path / "deep" / "nested" / "audit.jsonl"
    logger = AuditLogger(str(log_path))
    logger.log_verify("x", "/y.js", success=False)
    assert log_path.exists()


def test_audit_entry_frozen() -> None:
    entry = AuditEntry(
        timestamp="t", event="e", skill_slug="s", entrypoint="p", success=True,
    )
    try:
        entry.event = "modified"  # type: ignore[misc]
        raise AssertionError("Expected FrozenInstanceError")
    except AttributeError:
        pass
