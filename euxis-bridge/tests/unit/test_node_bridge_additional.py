from pathlib import Path
from unittest.mock import patch

import pytest

from euxis_bridge.core.node_bridge import NodeSkillBridge, NodeSkillError


class _Result:
    def __init__(self, returncode: int, stdout: str = "", stderr: str = "") -> None:
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr


def test_node_bridge_success(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("console.log('ok')", encoding="utf-8")

    bridge = NodeSkillBridge(timeout_seconds=2)
    with patch("subprocess.run", return_value=_Result(returncode=0, stdout="ok", stderr="")) as run:
        result = bridge.run(entry, payload={"k": "v"})

    assert result["status"] == "ok"
    assert result["returncode"] == 0
    assert "PATH" in run.call_args.kwargs["env"]


def test_node_bridge_nonzero_raises(tmp_path: Path) -> None:
    entry = tmp_path / "index.js"
    entry.write_text("console.error('bad')", encoding="utf-8")

    bridge = NodeSkillBridge(timeout_seconds=2)
    with patch("subprocess.run", return_value=_Result(returncode=3, stderr="boom")):
        with pytest.raises(NodeSkillError):
            bridge.run(entry, payload={})
