#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Refuse PRs that introduce a CI workflow step pointing at a script
that does not exist on the filesystem.

This gate exists because the previous architecture-quality.yml was
silently non-functional for months — every `python3 euxis-ops/...`
call resolved to a missing path, but the failure surfaced only at
runtime. Catching the broken reference at audit time prevents the
same drift class.

We scan every YAML file under .github/workflows/ for shell tokens
that look like script invocations (`python3 path/to/script.py`,
`bash path/to/script.sh`, `./path/to/script.sh`, etc.) and assert
the path exists. Any token referencing an explicit on-disk path
that does not resolve is reported.

Tokens that look like flags, registry tags, or absolute URLs are
ignored.

Exit codes:
    0  every referenced script exists
    1  at least one workflow step points at a missing path
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# Match a relative path that ends in .py, .sh, .ts, .mjs, .cjs, .rb,
# or .pl — the script-ish suffixes worth checking. Avoid greedy
# matches across whitespace.
SCRIPT_TOKEN = re.compile(
    r"(?<![\w./-])"
    r"(?:scripts|euxis-ops|tools|tests|fixtures|cmake-build|build|libs|apps)"
    r"/[A-Za-z0-9_./-]+"
    r"\.(?:py|sh|ts|mjs|cjs|rb|pl)"
    r"(?![\w])"
)


def referenced_paths(workflow_file: Path) -> list[str]:
    """Lines in a workflow file that look like local script paths."""
    text = workflow_file.read_text(encoding="utf-8", errors="replace")
    found: list[str] = []
    for match in SCRIPT_TOKEN.finditer(text):
        token = match.group(0)
        if token not in found:
            found.append(token)
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
    )
    args = parser.parse_args()

    workflows_dir = args.repo_root / ".github" / "workflows"
    if not workflows_dir.is_dir():
        print(f"No workflows directory at {workflows_dir}; nothing to check.")
        return 0

    workflows = sorted(workflows_dir.glob("*.yml")) + sorted(workflows_dir.glob("*.yaml"))
    missing: list[tuple[Path, str]] = []
    total = 0
    for wf in workflows:
        for token in referenced_paths(wf):
            total += 1
            path = args.repo_root / token
            if not path.exists():
                missing.append((wf, token))

    print(
        f"Scanned {len(workflows)} workflow file(s); "
        f"checked {total} script reference(s)."
    )
    if missing:
        print("MISSING SCRIPT REFERENCES:", file=sys.stderr)
        for wf, token in missing:
            print(f"  {wf.relative_to(args.repo_root)}  -> {token}", file=sys.stderr)
        return 1
    print("OK: every referenced script exists")
    return 0


if __name__ == "__main__":
    sys.exit(main())
