import json
from pathlib import Path
from unittest.mock import patch

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


def test_cli_verify_skill_clean(tmp_path: Path, capsys) -> None:
    skill_dir = tmp_path / "clean-skill"
    skill_dir.mkdir()
    (skill_dir / "index.js").write_text('module.exports = () => "ok";', encoding="utf-8")

    code = main(["verify-skill", "--skill-dir", str(skill_dir)])
    captured = capsys.readouterr()
    result = json.loads(captured.out)

    assert code == 0
    assert result["admitted"] is True


def test_cli_verify_skill_malicious(tmp_path: Path, capsys) -> None:
    skill_dir = tmp_path / "evil-skill"
    skill_dir.mkdir()
    (skill_dir / "evil.js").write_text('eval("rm -rf /")', encoding="utf-8")

    code = main(["verify-skill", "--skill-dir", str(skill_dir)])
    captured = capsys.readouterr()
    result = json.loads(captured.out)

    assert code == 1
    assert result["admitted"] is False


def test_cli_analyze_skill_json(tmp_path: Path, capsys) -> None:
    skill_dir = tmp_path / "skill"
    skill_dir.mkdir()
    (skill_dir / "safe.js").write_text('console.log("ok");', encoding="utf-8")

    code = main(["analyze-skill", "--skill-dir", str(skill_dir), "--format", "json"])
    captured = capsys.readouterr()
    result = json.loads(captured.out)

    assert code == 0
    assert result["passed"] is True


def test_cli_analyze_skill_text(tmp_path: Path, capsys) -> None:
    skill_dir = tmp_path / "skill"
    skill_dir.mkdir()
    (skill_dir / "safe.js").write_text('console.log("ok");', encoding="utf-8")

    code = main(["analyze-skill", "--skill-dir", str(skill_dir), "--format", "text"])
    captured = capsys.readouterr()

    assert code == 0
    assert "Passed: True" in captured.out


def test_cli_analyze_skill_text_with_findings(tmp_path: Path, capsys) -> None:
    skill_dir = tmp_path / "skill"
    skill_dir.mkdir()
    (skill_dir / "bad.js").write_text('eval("danger")', encoding="utf-8")

    code = main(["analyze-skill", "--skill-dir", str(skill_dir), "--format", "text"])
    captured = capsys.readouterr()

    assert code == 1
    assert "Passed: False" in captured.out
    assert "[critical]" in captured.out


def test_cli_analyze_skill_json_with_findings(tmp_path: Path, capsys) -> None:
    skill_dir = tmp_path / "skill"
    skill_dir.mkdir()
    (skill_dir / "bad.py").write_text('exec(open("x").read())', encoding="utf-8")

    code = main(["analyze-skill", "--skill-dir", str(skill_dir), "--format", "json"])
    captured = capsys.readouterr()
    result = json.loads(captured.out)

    assert code == 1
    assert result["passed"] is False
    assert len(result["findings"]) > 0


def test_cli_unknown_command_returns_1() -> None:
    """Defensive fallback: unknown command returns exit code 1."""
    import argparse

    fake_args = argparse.Namespace(command="nonexistent")
    with patch("euxis_bridge.cli._parser") as mock_parser:
        mock_parser.return_value.parse_args.return_value = fake_args
        assert main([]) == 1
