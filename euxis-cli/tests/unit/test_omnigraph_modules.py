# SPDX-License-Identifier: AGPL-3.0-or-later

from __future__ import annotations

import asyncio
from pathlib import Path

from cli.omnigraph.adapter import GraphAdapter
from cli.omnigraph.budgeter import TokenBudgeter
from cli.omnigraph.mapper import WorkspaceMapper


def _sample_graph() -> dict:
    return {
        "nodes": {
            "a.py": {"type": "python", "classes": ["A"], "functions": ["f"]},
            "b.ts": {"type": "typescript", "classes": [], "functions": []},
        },
        "edges": [{"source": "a.py", "target": "os", "relation": "imports"}],
    }


def test_graph_adapter_provider_routing_and_formats() -> None:
    graph = _sample_graph()

    claude_output = GraphAdapter.format_for_provider(graph, "claude")
    assert "EUXIS OMNIGRAPH" in claude_output
    assert "## a.py (python)" in claude_output
    assert "Classes: A" in claude_output
    assert "Functions: f" in claude_output
    assert "a.py --[imports]--> os" in claude_output

    json_output = GraphAdapter.format_for_provider(graph, "openai")
    assert '"nodes"' in json_output
    assert '"edges"' in json_output

    minimal_output = GraphAdapter.format_for_provider(graph, "ollama")
    assert minimal_output.startswith("# Workspace Map")
    assert "- a.py" in minimal_output
    assert "- b.ts" in minimal_output

    fallback_output = GraphAdapter.format_for_provider(graph, "unknown-provider")
    assert "EUXIS OMNIGRAPH" in fallback_output


def test_graph_adapter_aliases() -> None:
    graph = _sample_graph()
    assert GraphAdapter.format_for_provider(graph, "gemini").startswith("# EUXIS OMNIGRAPH")
    assert GraphAdapter.format_for_provider(graph, "gpt-4").lstrip().startswith("{")
    assert GraphAdapter.format_for_provider(graph, "llama").startswith("# Workspace Map")


def test_token_budgeter_estimate_and_prune() -> None:
    budgeter = TokenBudgeter(max_tokens=1)
    graph = _sample_graph()
    optimized = budgeter.optimize_graph(graph, "json")
    assert optimized["edges"] == []

    roomy = TokenBudgeter(max_tokens=1_000_000)
    graph2 = _sample_graph()
    optimized2 = roomy.optimize_graph(graph2, "json")
    assert optimized2["edges"] != []
    assert budgeter.estimate_tokens("abcd") == 1


def test_workspace_mapper_scan_workspace(tmp_path: Path) -> None:
    (tmp_path / "pkg").mkdir()
    (tmp_path / "pkg" / "mod.py").write_text(
        "import os\nfrom typing import Any\n\nclass A:\n    pass\n\ndef f():\n    return 1\n",
        encoding="utf-8",
    )
    (tmp_path / "script.sh").write_text("source ./lib.sh\n. ./env.sh\n", encoding="utf-8")
    (tmp_path / "ui.ts").write_text('import {x} from "./x"\n', encoding="utf-8")

    # Ignored dir content should not be scanned.
    (tmp_path / ".git").mkdir()
    (tmp_path / ".git" / "ignored.py").write_text("import never", encoding="utf-8")

    mapper = WorkspaceMapper(str(tmp_path))
    graph = asyncio.run(mapper.scan_workspace())

    assert "pkg/mod.py" in graph["nodes"]
    assert "script.sh" in graph["nodes"]
    assert "ui.ts" in graph["nodes"]
    assert ".git/ignored.py" not in graph["nodes"]

    py_node = graph["nodes"]["pkg/mod.py"]
    assert py_node["type"] == "python"
    assert "A" in py_node["classes"]
    assert "f" in py_node["functions"]
    assert "os" in py_node["imports"]
    assert "typing" in py_node["imports"]

    shell_node = graph["nodes"]["script.sh"]
    assert shell_node["type"] == "shell"
    assert "./lib.sh" in shell_node["imports"]
    assert "./env.sh" in shell_node["imports"]

    ts_node = graph["nodes"]["ui.ts"]
    assert ts_node["type"] == "typescript"
    assert "./x" in ts_node["imports"]


def test_workspace_mapper_parse_failure_paths(tmp_path: Path) -> None:
    mapper = WorkspaceMapper(str(tmp_path))

    bad_py = tmp_path / "bad.py"
    bad_py.write_text("def broken(:\n", encoding="utf-8")
    asyncio.run(mapper._parse_python(bad_py))
    assert "bad.py" not in mapper.graph["nodes"]

    missing = tmp_path / "missing.sh"
    asyncio.run(mapper._parse_regex(missing))
    assert "missing.sh" not in mapper.graph["nodes"]

