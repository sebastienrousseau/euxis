#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Enforce the canonical layout of every libs/<name>/ directory.

Every C++23 library under libs/ must ship:
    libs/<name>/CMakeLists.txt
    libs/<name>/include/euxis/<name>/   (at least one header)
    libs/<name>/src/                    (at least one source)
    libs/<name>/tests/                  (recommended; warning only)

Names are mapped through LIB_HEADER_OVERRIDES for the handful of
libraries whose include directory differs from the directory name
(e.g. libs/a2a-types ships includes under euxis/a2a/).

Exit codes:
    0  every library is layout-conformant
    1  at least one library is missing a required component
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

# Map libs/<dirname> → include subdirectory under include/euxis/.
LIB_HEADER_OVERRIDES = {
    "a2a-types": "a2a",
}


def lib_header_dir(name: str) -> str:
    return LIB_HEADER_OVERRIDES.get(name, name)


def check_lib(lib_root: Path) -> list[str]:
    """Return a list of failure messages for the library at lib_root."""
    name = lib_root.name
    problems: list[str] = []

    if not (lib_root / "CMakeLists.txt").is_file():
        problems.append(f"missing CMakeLists.txt")

    header_dir = lib_root / "include" / "euxis" / lib_header_dir(name)
    if not header_dir.is_dir():
        problems.append(f"missing include/euxis/{lib_header_dir(name)}/")
    elif not any(header_dir.glob("*.hpp")):
        problems.append(f"include/euxis/{lib_header_dir(name)}/ has no .hpp files")

    src_dir = lib_root / "src"
    if not src_dir.is_dir() or not any(src_dir.glob("*.cpp")):
        problems.append("src/ is missing or has no .cpp files")

    # Tests are warned, not failed — some lightweight libraries
    # (libs/a2a-types is largely header-only types) intentionally
    # share tests with their primary dependent.
    test_dir = lib_root / "tests"
    if test_dir.is_dir() and any(test_dir.glob("*.cpp")):
        pass  # has tests, all good
    elif test_dir.is_dir():
        problems.append("tests/ exists but has no .cpp files")
    else:
        problems.append("tests/ directory missing (warning)")

    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Treat missing tests/ as a failure rather than a warning.",
    )
    args = parser.parse_args()

    libs_root = args.repo_root / "libs"
    if not libs_root.is_dir():
        print(f"ERROR: {libs_root} not found", file=sys.stderr)
        return 1

    libraries = sorted(p for p in libs_root.iterdir() if p.is_dir())
    failures: list[tuple[str, list[str]]] = []
    warnings: list[tuple[str, list[str]]] = []

    for lib in libraries:
        problems = check_lib(lib)
        if not problems:
            continue
        warn_only = [p for p in problems if "warning" in p]
        hard = [p for p in problems if "warning" not in p]
        if hard:
            failures.append((lib.name, hard))
        if warn_only and args.strict:
            failures.append((lib.name, warn_only))
        elif warn_only:
            warnings.append((lib.name, warn_only))

    print(f"Scanned {len(libraries)} libraries under libs/")
    for name, msgs in warnings:
        for m in msgs:
            print(f"  WARN {name}: {m}")
    for name, msgs in failures:
        for m in msgs:
            print(f"  FAIL {name}: {m}", file=sys.stderr)

    if failures:
        print(f"\n{len(failures)} libraries failed layout audit", file=sys.stderr)
        return 1
    print("OK: every libs/ entry is layout-conformant")
    return 0


if __name__ == "__main__":
    sys.exit(main())
