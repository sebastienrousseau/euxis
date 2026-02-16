"""Shared Gateway utilities (session storage, timestamps)."""

from __future__ import annotations

import json
import os
import time
from pathlib import Path
from typing import Any, Dict, List


def timestamp() -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%S%z")


def gateway_data_dir() -> Path:
    home = os.environ.get("EUXIS_HOME", str(Path.home() / ".euxis"))
    base = Path(home) / "data" / "gateway"
    base.mkdir(parents=True, exist_ok=True)
    return base


def sessions_dir() -> Path:
    base = gateway_data_dir() / "sessions"
    base.mkdir(parents=True, exist_ok=True)
    return base


def runs_dir() -> Path:
    base = gateway_data_dir() / "runs"
    base.mkdir(parents=True, exist_ok=True)
    return base


def load_session_from_disk(session_id: str) -> List[Dict[str, Any]]:
    path = sessions_dir() / f"{session_id}.jsonl"
    if not path.exists():
        return []
    entries: List[Dict[str, Any]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        try:
            entries.append(json.loads(line))
        except Exception:
            continue
    return entries


def persist_message(session_id: str, entry: Dict[str, Any]) -> None:
    path = sessions_dir() / f"{session_id}.jsonl"
    with path.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(entry) + "\n")


def session_meta_path(session_id: str) -> Path:
    return sessions_dir() / f"{session_id}.meta.json"


def load_session_meta(session_id: str) -> Dict[str, Any]:
    path = session_meta_path(session_id)
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return {}


def persist_session_meta(session_id: str, meta: Dict[str, Any]) -> None:
    path = session_meta_path(session_id)
    path.write_text(json.dumps(meta, indent=2), encoding="utf-8")


def persist_run_event(run_id: str, entry: Dict[str, Any]) -> None:
    path = runs_dir() / f"{run_id}.jsonl"
    with path.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(entry) + "\n")


def load_run_events(run_id: str) -> List[Dict[str, Any]]:
    path = runs_dir() / f"{run_id}.jsonl"
    if not path.exists():
        return []
    entries: List[Dict[str, Any]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        try:
            entries.append(json.loads(line))
        except Exception:
            continue
    return entries
