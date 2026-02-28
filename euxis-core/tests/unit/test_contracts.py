# SPDX-License-Identifier: AGPL-3.0-or-later

from __future__ import annotations

from euxis_core.contracts import AgentExecutionRequest
from euxis_core.platform import LocalFilesystemAdapter, RuntimePlatformAdapter


def test_contract_types_construct() -> None:
    request = AgentExecutionRequest(agent_id="agent", function_name="run", payload={"x": 1})
    assert request.timeout_seconds == 30.0


def test_platform_and_filesystem_adapters(tmp_path) -> None:
    platform_name = RuntimePlatformAdapter().platform_name()
    assert platform_name in {"linux", "macos", "windows", "wsl", "unknown"}

    fs = LocalFilesystemAdapter()
    target = tmp_path / "data" / "sample.txt"
    fs.write_text(target, "ok")
    assert fs.read_text(target) == "ok"
