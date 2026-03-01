import json
from pathlib import Path

from euxis_bridge.cli import main


def test_cli_import_clawhub(tmp_path: Path, capsys) -> None:
    skill_dir = tmp_path / "skills" / "memory"
    skill_dir.mkdir(parents=True)
    (skill_dir / "SKILL.md").write_text(
        """---
name: Memory
slug: memory
runtime: node
entrypoint: index.js
---
""",
        encoding="utf-8",
    )
    output = tmp_path / "registry.json"

    code = main(["import-clawhub", "--source", str(tmp_path / "skills"), "--output", str(output)])
    captured = capsys.readouterr()

    assert code == 0
    report = json.loads(captured.out)
    assert report["imported"] == 1
    assert output.exists()
