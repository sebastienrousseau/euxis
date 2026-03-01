from pathlib import Path

import pytest

from euxis_bridge.core.node_bridge import NodeSkillBridge, NodeSkillError


def test_node_bridge_missing_entrypoint(tmp_path: Path) -> None:
    bridge = NodeSkillBridge(timeout_seconds=1)
    with pytest.raises(NodeSkillError):
        bridge.run(tmp_path / "missing.js", payload={"x": 1})
