import json
from pathlib import Path

from euxis_bridge.core.importer import ClawHubImporter


def test_import_skills_prefers_skill_md(tmp_path: Path) -> None:
    skill_dir = tmp_path / "skills" / "browser-use"
    skill_dir.mkdir(parents=True)
    (skill_dir / "SKILL.md").write_text(
        """---
name: Browser Use
slug: browser-use
entrypoint: run.js
runtime: node
---
""",
        encoding="utf-8",
    )
    (skill_dir / "openclaw.json").write_text(
        '{"name":"Ignored Name","entrypoint":"ignored.js"}',
        encoding="utf-8",
    )

    importer = ClawHubImporter(tmp_path / "skills")
    skills = importer.import_skills()

    assert len(skills) == 1
    assert skills[0].name == "Browser Use"
    assert skills[0].slug == "browser-use"
    assert skills[0].entrypoint.endswith("run.js")


def test_export_registry(tmp_path: Path) -> None:
    skill_dir = tmp_path / "skills" / "telegram"
    skill_dir.mkdir(parents=True)
    (skill_dir / "openclaw.json").write_text(
        '{"name":"Telegram Bridge","runtime":"node","entrypoint":"index.js"}',
        encoding="utf-8",
    )

    importer = ClawHubImporter(tmp_path / "skills")
    output = tmp_path / "registry.json"
    report = importer.export_registry(output)

    data = json.loads(output.read_text(encoding="utf-8"))
    assert report.imported == 1
    assert data["imported"][0]["name"] == "Telegram Bridge"
