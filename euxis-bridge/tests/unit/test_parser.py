from pathlib import Path

from euxis_bridge.core.parser import parse_openclaw_json, parse_skill_md


def test_parse_skill_md_frontmatter(tmp_path: Path) -> None:
    skill_dir = tmp_path / "browser-use"
    skill_dir.mkdir(parents=True)
    skill_file = skill_dir / "SKILL.md"
    skill_file.write_text(
        """---
name: Browser Use
slug: browser-use
description: Browser automation skill
runtime: node
entrypoint: run.js
tags: [web, automation]
---
# Browser Use
""",
        encoding="utf-8",
    )

    parsed = parse_skill_md(skill_file)
    assert parsed["name"] == "Browser Use"
    assert parsed["slug"] == "browser-use"
    assert parsed["entrypoint"] == "run.js"
    assert parsed["tags"] == ["web", "automation"]


def test_parse_openclaw_json(tmp_path: Path) -> None:
    config = tmp_path / "openclaw.json"
    config.write_text(
        '{"name":"Telegram Bridge","runtime":"node","entrypoint":"bridge.js"}',
        encoding="utf-8",
    )

    parsed = parse_openclaw_json(config)
    assert parsed["name"] == "Telegram Bridge"
    assert parsed["entrypoint"] == "bridge.js"
