from pathlib import Path

from euxis_bridge.core import parser


def test_parse_frontmatter_missing_block() -> None:
    assert parser.parse_frontmatter("# no frontmatter") == {}


def test_safe_parse_value_coverage() -> None:
    assert parser._safe_parse_value("") == ""
    assert parser._safe_parse_value("true") is True
    assert parser._safe_parse_value("false") is False
    assert parser._safe_parse_value("42") == 42
    assert parser._safe_parse_value("[a,b]") == ["a", "b"]
    assert parser._safe_parse_value("[]") == []
    assert parser._safe_parse_value("'x'") == "x"


def test_parse_skill_md_defaults(tmp_path: Path) -> None:
    skill_dir = tmp_path / "skill-a"
    skill_dir.mkdir(parents=True)
    skill = skill_dir / "SKILL.md"
    skill.write_text("---\n---\n", encoding="utf-8")

    parsed = parser.parse_skill_md(skill)
    assert parsed["name"] == "skill-a"
    assert parsed["slug"] == "skill-a"
    assert parsed["runtime"] == "node"


def test_parse_frontmatter_skips_invalid_line_and_unclosed_block() -> None:
    parsed = parser.parse_frontmatter("---\ninvalid\nname: ok\n")
    assert parsed["name"] == "ok"


def test_parse_openclaw_json_error(tmp_path: Path) -> None:
    path = tmp_path / "openclaw.json"
    path.write_text("[]", encoding="utf-8")
    try:
        parser.parse_openclaw_json(path)
    except ValueError as exc:
        assert "must be an object" in str(exc)
    else:
        raise AssertionError("Expected ValueError")
