from pathlib import Path

from euxis_bridge.core.importer import ClawHubImporter


def test_importer_empty_source(tmp_path: Path) -> None:
    importer = ClawHubImporter(tmp_path / "missing")
    assert importer.import_skills() == []


def test_importer_openclaw_json_only(tmp_path: Path) -> None:
    skill_dir = tmp_path / "skills" / "telegram"
    skill_dir.mkdir(parents=True)
    (skill_dir / "openclaw.json").write_text(
        '{"name":"Telegram","slug":"telegram","runtime":"python","entrypoint":"run.py"}',
        encoding="utf-8",
    )

    importer = ClawHubImporter(tmp_path / "skills")
    items = importer.import_skills()
    assert len(items) == 1
    assert items[0].runtime == "python"
    assert items[0].command == []


def test_entrypoint_fallback_main(tmp_path: Path) -> None:
    skill_dir = tmp_path / "skills" / "task"
    skill_dir.mkdir(parents=True)
    (skill_dir / "openclaw.json").write_text('{"name":"Task","main":"app.js"}', encoding="utf-8")

    importer = ClawHubImporter(tmp_path / "skills")
    skill = importer.import_skills()[0]
    assert skill.entrypoint.endswith("app.js")
