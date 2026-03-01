"""Parsers for OpenClaw skill manifests."""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any

FRONTMATTER_BOUNDARY = "---"


def _safe_parse_value(raw: str) -> Any:
    value = raw.strip()
    if not value:
        return ""
    if value.startswith("[") and value.endswith("]"):
        inner = value[1:-1].strip()
        if not inner:
            return []
        return [item.strip().strip("\"'") for item in inner.split(",") if item.strip()]
    if value.lower() in {"true", "false"}:
        return value.lower() == "true"
    if re.fullmatch(r"-?\d+", value):
        return int(value)
    return value.strip("\"'")


def parse_frontmatter(markdown: str) -> dict[str, Any]:
    """Parse simple YAML-like frontmatter used by SKILL.md bundles."""
    lines = markdown.splitlines()
    if len(lines) < 3 or lines[0].strip() != FRONTMATTER_BOUNDARY:
        return {}

    parsed: dict[str, Any] = {}
    for line in lines[1:]:
        if line.strip() == FRONTMATTER_BOUNDARY:
            break
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        parsed[key.strip()] = _safe_parse_value(value)
    return parsed


def parse_skill_md(path: Path) -> dict[str, Any]:
    text = path.read_text(encoding="utf-8", errors="replace")
    frontmatter = parse_frontmatter(text)
    frontmatter.setdefault("name", path.parent.name)
    frontmatter.setdefault("slug", path.parent.name)
    frontmatter.setdefault("runtime", "node")
    return frontmatter


def parse_openclaw_json(path: Path) -> dict[str, Any]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError("openclaw.json must be an object")
    return data
