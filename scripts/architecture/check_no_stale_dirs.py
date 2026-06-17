#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Refuse PRs that re-introduce the legacy directories enumerated in
docs/architecture/legacy-cleanup-2026.md.

This script is the CI half of the P0.11 cleanup: even after the
.gitignore update, a contributor could still `git add -f` one of the
legacy paths. The gate catches that before the PR can merge.

Exit codes:
    0  no stale directories tracked
    1  at least one stale path appears in `git ls-files`
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

# The 1:1 list from docs/architecture/legacy-cleanup-2026.md. Keep
# in sync — both the doc and the gate are the source of truth.
STALE_PREFIXES = (
    "tui/",
    ".venv/",
    ".venv-voice/",
    "bus/",
    "metrics/",
    "packages/",
    "prompts/",
    "codex/",
    ".pytest_cache/",
)

# Match any of these inside any directory.
STALE_GLOBS = (
    "__pycache__/",
    "*.pyc",
)


def run_git_ls_files(repo_root: Path) -> list[str]:
    """Tracked files in the repository, normalised to POSIX paths."""
    out = subprocess.run(
        ["git", "ls-files"],
        cwd=repo_root,
        check=True,
        capture_output=True,
        text=True,
    )
    return [line for line in out.stdout.splitlines() if line]


def find_violations(tracked: list[str]) -> list[str]:
    bad: list[str] = []
    for path in tracked:
        if any(path.startswith(prefix) for prefix in STALE_PREFIXES):
            bad.append(path)
            continue
        if "/__pycache__/" in path or path.startswith("__pycache__/"):
            bad.append(path)
            continue
        if path.endswith(".pyc"):
            bad.append(path)
    return bad


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="Repository root (default: derived from script location)",
    )
    args = parser.parse_args()

    tracked = run_git_ls_files(args.repo_root)
    bad = find_violations(tracked)
    if not bad:
        print(f"OK: no stale directories tracked ({len(tracked)} files scanned)")
        return 0

    print("STALE DIRECTORY REGRESSION DETECTED:", file=sys.stderr)
    for path in bad:
        print(f"  tracked but should not be: {path}", file=sys.stderr)
    print(
        "\nSee docs/architecture/legacy-cleanup-2026.md for the canonical list "
        "and the replacement for each removed path.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
