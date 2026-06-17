#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Verify the branding footer (Gate 5f) is present on the tip commit.

Mirrors the `branding-check.yml` job logic but as a standalone CLI so
the architecture-quality workflow can call it without duplicating the
regex. The expected footer is whatever is in
data/config/branding/signature.txt.

Exit codes:
    0  branding footer present on the tip commit
    1  branding footer missing or signature file missing
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

DEFAULT_SIGNATURE_PATH = "data/config/branding/signature.txt"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
    )
    parser.add_argument(
        "--signature",
        default=DEFAULT_SIGNATURE_PATH,
        help="Path to the branding signature file, relative to repo root.",
    )
    parser.add_argument(
        "--ref",
        default="HEAD",
        help="Commit ref to inspect (default: HEAD).",
    )
    args = parser.parse_args()

    sig_path = args.repo_root / args.signature
    if not sig_path.is_file():
        print(f"ERROR: branding signature file not found at {sig_path}",
              file=sys.stderr)
        return 1
    sig_lines = [
        line.strip()
        for line in sig_path.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.startswith("---")
    ]
    if not sig_lines:
        print("ERROR: branding signature file is empty after stripping markers",
              file=sys.stderr)
        return 1

    body = subprocess.run(
        ["git", "log", "-1", "--format=%B", args.ref],
        cwd=args.repo_root,
        check=True,
        capture_output=True,
        text=True,
    ).stdout

    missing = [line for line in sig_lines if line not in body]
    if missing:
        print(
            f"ERROR: commit {args.ref} is missing branding lines:",
            file=sys.stderr,
        )
        for line in missing:
            print(f"  {line}", file=sys.stderr)
        print(
            "\nEvery commit must include data/config/branding/signature.txt "
            "as a trailer. See the branding-check workflow + Gate 5f for "
            "the broader context.",
            file=sys.stderr,
        )
        return 1
    print(f"OK: branding footer present on {args.ref}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
