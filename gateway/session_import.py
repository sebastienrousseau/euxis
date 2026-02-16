#!/usr/bin/env python3
"""Import Gateway sessions from JSON.

Usage:
  python3 gateway/session_import.py --in sessions.json
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from gateway.utils import persist_message


def main() -> int:
    parser = argparse.ArgumentParser(description="Import gateway sessions")
    parser.add_argument("--in", dest="input_path", required=True, help="Input JSON file")
    args = parser.parse_args()

    data = json.loads(open(args.input_path, "r", encoding="utf-8").read())
    if not isinstance(data, dict):
        raise SystemExit("Invalid session export format")

    count = 0
    for session_id, entries in data.items():
        if not isinstance(entries, list):
            continue
        for entry in entries:
            if not isinstance(entry, dict):
                continue
            persist_message(session_id, entry)
            count += 1

    print(f"Imported {count} messages")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
