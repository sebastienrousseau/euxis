#!/usr/bin/env python3
"""Export Gateway sessions to JSON.

Usage:
  python3 gateway/session_export.py --out sessions.json
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from gateway.utils import load_session_from_disk, sessions_dir


def main() -> int:
    parser = argparse.ArgumentParser(description="Export gateway sessions")
    parser.add_argument("--out", required=True, help="Output JSON file")
    args = parser.parse_args()

    data = {}
    for path in sessions_dir().glob("*.jsonl"):
        session_id = path.stem
        data[session_id] = load_session_from_disk(session_id)

    out_path = Path(args.out)
    out_path.write_text(json.dumps(data, indent=2), encoding="utf-8")
    print(f"Exported {len(data)} sessions to {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
