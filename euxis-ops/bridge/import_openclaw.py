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

        if not args.dry_run:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)

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
