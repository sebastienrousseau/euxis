"""Tests for NodeSkillBridge policy integration."""

import json
from pathlib import Path
from unittest.mock import patch

import pytest

from euxis_bridge.core.node_bridge import NodeSkillBridge, NodeSkillError
from euxis_bridge.core.policy import ResourceLimits, SkillExecutionPolicy
from euxis_bridge.core.verification import SkillVerificationError


class _Result:
    def __init__(self, returncode: int, stdout: str = "", stderr: str = "") -> None:
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr


def test_backward_compat_no_policy(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("console.log('ok')", encoding="utf-8")

    bridge = NodeSkillBridge(timeout_seconds=2)
    with patch("subprocess.run", return_value=_Result(0, "ok")) as run:
        result = bridge.run(entry, payload={"k": "v"})

    assert result["status"] == "ok"
    assert "NODE_OPTIONS" not in run.call_args.kwargs["env"]


def test_policy_injects_memory_limit(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("x", encoding="utf-8")

    policy = SkillExecutionPolicy(resources=ResourceLimits(memory_mb=128))
    bridge = NodeSkillBridge(policy=policy)
    with patch("subprocess.run", return_value=_Result(0, "")) as run:
        bridge.run(entry, payload={})

    assert run.call_args.kwargs["env"]["NODE_OPTIONS"] == "--max-old-space-size=128"


def test_policy_uses_resource_timeout(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("x", encoding="utf-8")

    policy = SkillExecutionPolicy(resources=ResourceLimits(timeout_seconds=5))
    bridge = NodeSkillBridge(timeout_seconds=99, policy=policy)
    with patch("subprocess.run", return_value=_Result(0, "")) as run:
        bridge.run(entry, payload={})

    assert run.call_args.kwargs["timeout"] == 5


def test_policy_require_signature_no_key_raises(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("x", encoding="utf-8")

    policy = SkillExecutionPolicy(require_signature=True)
    bridge = NodeSkillBridge(policy=policy)
    with pytest.raises(NodeSkillError, match="no public_key_path"):
        bridge.run(entry, payload={})


def test_policy_signature_check_fails(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("x", encoding="utf-8")

    policy = SkillExecutionPolicy(
        require_signature=True,
        public_key_path="/nonexistent/pub.key",
    )
    bridge = NodeSkillBridge(policy=policy)
    with pytest.raises(SkillVerificationError):
        bridge.run(entry, payload={})


def test_policy_audit_logging(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("x", encoding="utf-8")
    log_path = tmp_path / "audit.jsonl"

    policy = SkillExecutionPolicy(audit_log_path=str(log_path))
    bridge = NodeSkillBridge(policy=policy)
    with patch("subprocess.run", return_value=_Result(0, "")):
        bridge.run(entry, payload={}, skill_slug="test-skill")

    lines = log_path.read_text(encoding="utf-8").strip().splitlines()
    assert len(lines) == 2
    assert json.loads(lines[0])["event"] == "execute"
    assert json.loads(lines[1])["event"] == "complete"


def test_policy_audit_logging_on_error(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("x", encoding="utf-8")
    log_path = tmp_path / "audit.jsonl"

    policy = SkillExecutionPolicy(audit_log_path=str(log_path))
    bridge = NodeSkillBridge(policy=policy)
    with patch("subprocess.run", return_value=_Result(1, stderr="boom")):
        with pytest.raises(NodeSkillError):
            bridge.run(entry, payload={}, skill_slug="fail-skill")

    lines = log_path.read_text(encoding="utf-8").strip().splitlines()
    assert any(json.loads(l)["event"] == "error" for l in lines)


def test_policy_audit_logging_on_exception(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("x", encoding="utf-8")
    log_path = tmp_path / "audit.jsonl"

    policy = SkillExecutionPolicy(audit_log_path=str(log_path))
    bridge = NodeSkillBridge(policy=policy)
    with patch("subprocess.run", side_effect=OSError("no node")):
        with pytest.raises(OSError):
            bridge.run(entry, payload={}, skill_slug="crash-skill")

    lines = log_path.read_text(encoding="utf-8").strip().splitlines()
    assert any(json.loads(l)["event"] == "error" for l in lines)


def test_output_schema_validation(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("x", encoding="utf-8")

    policy = SkillExecutionPolicy(output_schema={"required": ["result", "status"]})
    bridge = NodeSkillBridge(policy=policy)
    with patch("subprocess.run", return_value=_Result(0, '{"result":"ok"}')):
        with pytest.raises(NodeSkillError, match="output missing required field: status"):
            bridge.run(entry, payload={})


def test_output_schema_passes(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("x", encoding="utf-8")

    policy = SkillExecutionPolicy(output_schema={"required": ["result"]})
    bridge = NodeSkillBridge(policy=policy)
    with patch("subprocess.run", return_value=_Result(0, '{"result":"ok"}')):
        result = bridge.run(entry, payload={})
    assert result["status"] == "ok"


def test_output_schema_non_json_stdout_ignored(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("x", encoding="utf-8")

    policy = SkillExecutionPolicy(output_schema={"required": ["result"]})
    bridge = NodeSkillBridge(policy=policy)
    with patch("subprocess.run", return_value=_Result(0, "not json")):
        result = bridge.run(entry, payload={})
    assert result["status"] == "ok"


def test_signature_verify_with_audit_logging(tmp_path: Path) -> None:
    """Lines 60-61: signature verification succeeds with audit logging active."""
    entry = tmp_path / "index.js"
    entry.write_text("x", encoding="utf-8")
    log_path = tmp_path / "audit.jsonl"

    policy = SkillExecutionPolicy(
        require_signature=True,
        public_key_path="/fake/pub.key",
        audit_log_path=str(log_path),
    )
    bridge = NodeSkillBridge(policy=policy)
    with (
        patch(
            "euxis_bridge.core.node_bridge.require_verified", return_value=None
        ),
        patch("subprocess.run", return_value=_Result(0, "")),
    ):
        result = bridge.run(entry, payload={}, skill_slug="sig-audit")

    assert result["status"] == "ok"
    lines = log_path.read_text(encoding="utf-8").strip().splitlines()
    import json as _json

    events = [_json.loads(l)["event"] for l in lines]
    assert "verify" in events
    assert "execute" in events
    assert "complete" in events


def test_signature_verify_without_audit_logging(tmp_path: Path) -> None:
    """Branch 60->63: signature verification succeeds but no audit logger."""
    entry = tmp_path / "index.js"
    entry.write_text("x", encoding="utf-8")

    policy = SkillExecutionPolicy(
        require_signature=True,
        public_key_path="/fake/pub.key",
    )
    bridge = NodeSkillBridge(policy=policy)
    with (
        patch(
            "euxis_bridge.core.node_bridge.require_verified", return_value=None
        ),
        patch("subprocess.run", return_value=_Result(0, "")),
    ):
        result = bridge.run(entry, payload={}, skill_slug="no-audit")

    assert result["status"] == "ok"


def test_exception_without_logger(tmp_path: Path) -> None:
    """Branch 76->78: subprocess raises exception without audit logger."""
    entry = tmp_path / "index.js"
    entry.write_text("x", encoding="utf-8")

    bridge = NodeSkillBridge()
    with patch("subprocess.run", side_effect=OSError("no node")):
        with pytest.raises(OSError, match="no node"):
            bridge.run(entry, payload={})
