#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Shared helpers for Euxis OpenClaw bridge tooling."""

from __future__ import annotations

import json
import os
import time
from hashlib import sha256
from pathlib import Path
from typing import Any


def utc_ts() -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


def expand_user_path(raw: str) -> Path:
    expanded = os.path.expandvars(raw)
    return Path(expanded).expanduser().resolve()


def load_config(config_path: Path) -> dict[str, Any]:
    text = config_path.read_text(encoding="utf-8")

    # Prefer YAML when available; fallback to JSON for constrained environments.
    try:
        import yaml  # type: ignore

        parsed = yaml.safe_load(text)
        if not isinstance(parsed, dict):
            raise ValueError("bridge config must be a mapping")
        return parsed
    except ModuleNotFoundError:
        parsed = json.loads(text)
        if not isinstance(parsed, dict):
            raise ValueError("bridge config must be a JSON object")
        return parsed


def digest_file(path: Path) -> str:
    hasher = sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(65536), b""):
            hasher.update(block)
    return hasher.hexdigest()


def ensure_parent(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def append_jsonl(path: Path, payload: dict[str, Any]) -> None:
    ensure_parent(path)
    with path.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(payload, separators=(",", ":"), sort_keys=True) + "\n")


def resolve_configured_path(config: dict[str, Any], section: str, key: str, default: str) -> Path:
    raw = (
        config.get(section, {}).get(key)
        if isinstance(config.get(section), dict)
        else None
    )
    if not raw:
        raw = default
    return expand_user_path(str(raw))
