"""Shared Gateway utilities (session storage, timestamps)."""

from __future__ import annotations

import json
import os
import time
from pathlib import Path
from typing import Any, Dict, List


def timestamp() -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%S%z")


def make_session_id(
    channel_id: str,
    chat_id: str,
    thread_id: str | None = None,
    dm_scope: str = "main",
    account_id: str | None = None,
    sender_id: str | None = None,
) -> str:
    if dm_scope == "per-peer":
        suffix = sender_id or "unknown"
        base = f"{channel_id}:{chat_id}:{suffix}"
    elif dm_scope == "per-channel-peer":
        suffix = sender_id or "unknown"
        base = f"{channel_id}:{chat_id}:{suffix}"
    elif dm_scope == "per-account-channel-peer":
        suffix = sender_id or "unknown"
        account = account_id or "default"
        base = f"{account}:{channel_id}:{chat_id}:{suffix}"
    else:
        base = f"{channel_id}:{chat_id}"
    if thread_id:
        return f"{base}:{thread_id}"
    return base


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


def approvals_dir() -> Path:
    base = gateway_data_dir() / "approvals"
    base.mkdir(parents=True, exist_ok=True)
    return base


def audit_dir() -> Path:
    base = gateway_data_dir() / "audit"
    base.mkdir(parents=True, exist_ok=True)
    return base


def transcripts_dir() -> Path:
    base = gateway_data_dir() / "transcripts"
    base.mkdir(parents=True, exist_ok=True)
    return base


def cron_dir() -> Path:
    base = gateway_data_dir() / "cron"
    base.mkdir(parents=True, exist_ok=True)
    return base


def cron_path() -> Path:
    return cron_dir() / "jobs.json"


def load_cron_jobs() -> List[Dict[str, Any]]:
    path = cron_path()
    if not path.exists():
        return []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return []
    if isinstance(data, list):
        return data
    return []


def persist_cron_jobs(jobs: List[Dict[str, Any]]) -> None:
    path = cron_path()
    path.write_text(json.dumps(jobs, indent=2), encoding="utf-8")


def webhooks_path() -> Path:
    return gateway_data_dir() / "webhooks.json"


def load_webhooks() -> List[Dict[str, Any]]:
    path = webhooks_path()
    if not path.exists():
        return []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return []
    if isinstance(data, list):
        return data
    return []


def persist_webhooks(hooks: List[Dict[str, Any]]) -> None:
    path = webhooks_path()
    path.write_text(json.dumps(hooks, indent=2), encoding="utf-8")


def persist_voice_blob(session_id: str, blob: bytes, suffix: str) -> Path:
    stamp = time.strftime("%Y%m%d-%H%M%S")
    path = voice_dir() / f"{session_id}_{stamp}.{suffix}"
    path.write_bytes(blob)
    return path


def canvas_dir() -> Path:
    base = gateway_data_dir() / "canvas"
    base.mkdir(parents=True, exist_ok=True)
    return base


def canvas_state_path(session_id: str) -> Path:
    return canvas_dir() / f"{session_id}.json"


def load_canvas_state(session_id: str) -> Dict[str, Any]:
    path = canvas_state_path(session_id)
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return {}


def persist_canvas_state(session_id: str, state: Dict[str, Any]) -> None:
    path = canvas_state_path(session_id)
    path.write_text(json.dumps(state, indent=2), encoding="utf-8")


def voice_dir() -> Path:
    base = gateway_data_dir() / "voice"
    base.mkdir(parents=True, exist_ok=True)
    return base


def voice_transcript_path(session_id: str) -> Path:
    return voice_dir() / f"{session_id}.jsonl"


def persist_voice_text(session_id: str, entry: Dict[str, Any]) -> None:
    path = voice_transcript_path(session_id)
    with path.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(entry) + "\n")


def voice_chunk_path(session_id: str, suffix: str = "raw") -> Path:
    return voice_dir() / f"{session_id}.{suffix}"


def append_voice_chunk(session_id: str, chunk: bytes, suffix: str = "raw") -> Path:
    path = voice_chunk_path(session_id, suffix)
    with path.open("ab") as handle:
        handle.write(chunk)
    return path


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


def persist_approval(run_id: str, entry: Dict[str, Any]) -> None:
    path = approvals_dir() / f"{run_id}.json"
    path.write_text(json.dumps(entry, indent=2), encoding="utf-8")


def load_approvals() -> Dict[str, Dict[str, Any]]:
    approvals: Dict[str, Dict[str, Any]] = {}
    for path in approvals_dir().glob("*.json"):
        try:
            approvals[path.stem] = json.loads(path.read_text(encoding="utf-8"))
        except Exception:
            continue
    return approvals


def delete_approval(run_id: str) -> None:
    path = approvals_dir() / f"{run_id}.json"
    if path.exists():
        path.unlink()


def audit_log(event: Dict[str, Any]) -> None:
    path = audit_dir() / "gateway_audit.jsonl"
    with path.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(event) + "\n")


def persist_transcript(run_id: str, entry: Dict[str, Any]) -> None:
    path = transcripts_dir() / f"{run_id}.jsonl"
    with path.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(entry) + "\n")
