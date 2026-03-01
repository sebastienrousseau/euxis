#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Import OpenClaw local identity/memory data into Euxis canonical stores."""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from bridge_common import append_jsonl, digest_file, expand_user_path, load_config, utc_ts

SUPPORTED_EXTENSIONS = {".json", ".jsonl", ".md", ".markdown"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", default="bridge_config.yaml")
    parser.add_argument("--source", default="~/.openclaw")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--output-manifest", default="")
    return parser.parse_args()


def classify_target(path: Path) -> str:
    lowered = str(path).lower()
    if "identit" in lowered or "credential" in lowered:
        return "identities"
    if "transcript" in lowered:
        return "transcripts"
    return "sessions"


def collect_source_files(root: Path) -> list[Path]:
    files: list[Path] = []
    if not root.exists():
        return files
    for candidate in root.rglob("*"):
        if candidate.is_file() and candidate.suffix.lower() in SUPPORTED_EXTENSIONS:
            files.append(candidate)
    return sorted(files)


def _safe_json(raw: str) -> Any:
    try:
        return json.loads(raw)
    except Exception:
        return None


def _normalized_records(src: Path, rel: Path, target_type: str) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    suffix = src.suffix.lower()
    text = src.read_text(encoding="utf-8", errors="replace")

    if suffix == ".json":
        data = _safe_json(text)
        if isinstance(data, dict):
            if target_type == "identities":
                identity_id = (
                    data.get("agent_id")
                    or data.get("id")
                    or data.get("user_id")
                    or rel.stem
                )
                identity_name = data.get("name") or data.get("display_name") or str(identity_id)
                records.append(
                    {
                        "kind": "identity",
                        "id": str(identity_id),
                        "name": str(identity_name),
                        "source_file": str(rel),
                        "raw": data,
                    }
                )
            else:
                messages = data.get("messages") or data.get("history") or data.get("entries")
                if isinstance(messages, list):
                    for idx, item in enumerate(messages):
                        records.append(
                            {
                                "kind": "message",
                                "index": idx,
                                "source_file": str(rel),
                                "content": item,
                            }
                        )
    elif suffix == ".jsonl":
        for idx, line in enumerate(text.splitlines()):
            parsed = _safe_json(line)
            if parsed is None:
                continue
            records.append({"kind": "jsonl", "index": idx, "source_file": str(rel), "content": parsed})
    elif suffix in {".md", ".markdown"}:
        records.append(
            {
                "kind": "markdown",
                "source_file": str(rel),
                "content": text,
            }
        )

    return records


def main() -> int:
    args = parse_args()
    config = load_config(Path(args.config))

    source_root = expand_user_path(args.source)
    paths = config.get("migration", {}).get("output_roots", {})

    out_identities = expand_user_path(str(paths.get("identities", "~/.euxis/euxis-data/gateway/identities")))
    out_sessions = expand_user_path(str(paths.get("sessions", "~/.euxis/euxis-data/gateway/sessions")))
    out_transcripts = expand_user_path(str(paths.get("transcripts", "~/.euxis/euxis-data/gateway/transcripts")))

    all_files = collect_source_files(source_root)
    manifest_entries: list[dict[str, Any]] = []

    normalized_paths = {
        "identities": out_identities / "identities.normalized.jsonl",
        "sessions": out_sessions / "sessions.normalized.jsonl",
        "transcripts": out_transcripts / "transcripts.normalized.jsonl",
    }

    for src in all_files:
        target_type = classify_target(src)
        rel = src.relative_to(source_root)
        dest_base = {
            "identities": out_identities,
            "sessions": out_sessions,
            "transcripts": out_transcripts,
        }[target_type]
        dst = dest_base / rel

        entry = {
            "source": str(src),
            "destination": str(dst),
            "type": target_type,
            "sha256": digest_file(src),
            "size_bytes": src.stat().st_size,
        }
        manifest_entries.append(entry)

        records = _normalized_records(src, rel, target_type)
        if not args.dry_run:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
            for record in records:
                append_jsonl(
                    normalized_paths[target_type],
                    {
                        "imported_at": utc_ts(),
                        "source": "openclaw",
                        **record,
                    },
                )

    default_manifest = expand_user_path("~/.euxis/euxis-data/bridge/import-manifest.json")
    manifest_path = expand_user_path(args.output_manifest) if args.output_manifest else default_manifest
    manifest_path.parent.mkdir(parents=True, exist_ok=True)

    manifest = {
        "generated_at": utc_ts(),
        "source_root": str(source_root),
        "dry_run": bool(args.dry_run),
        "files": manifest_entries,
        "counts": {
            "total": len(manifest_entries),
            "identities": sum(1 for x in manifest_entries if x["type"] == "identities"),
            "sessions": sum(1 for x in manifest_entries if x["type"] == "sessions"),
            "transcripts": sum(1 for x in manifest_entries if x["type"] == "transcripts"),
        },
    }
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    audit_path = expand_user_path("~/.euxis/euxis-data/security/bridge-audit.jsonl")
    append_jsonl(
        audit_path,
        {
            "ts": utc_ts(),
            "event": "openclaw_import",
            "source_root": str(source_root),
            "manifest": str(manifest_path),
            "dry_run": bool(args.dry_run),
            "count": len(manifest_entries),
        },
    )

    print(json.dumps({"status": "ok", "manifest": str(manifest_path), "files": len(manifest_entries)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
