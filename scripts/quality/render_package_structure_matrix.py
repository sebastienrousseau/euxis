#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Render deterministic package structure matrix markdown from standards."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _fmt_bool(value: bool) -> str:
    return "yes" if value else "no"


def _render(repo_root: Path, standards: dict) -> str:
    lines = [
        "# Package Structure Matrix",
        "",
        "Generated from `scripts/quality/package_standards.json`.",
        "",
        "| Package | Kind | Path | Required Files | Tests | Docs | Status |",
        "| --- | --- | --- | --- | --- | --- | --- |",
    ]

    for pkg in standards.get("packages", []):
        name = str(pkg.get("name", ""))
        kind = str(pkg.get("kind", "unknown"))
        pkg_path = str(pkg.get("path", ""))
        required_files = [str(x) for x in pkg.get("required_files", [])]
        test_paths = [str(x) for x in pkg.get("test_paths", [])]
        docs_path = str(pkg.get("docs_path", ""))

        pkg_root = repo_root / pkg_path
        required_ok = all((pkg_root / req).exists() for req in required_files)
        tests_ok = True if not test_paths else any((repo_root / t).exists() for t in test_paths)
        docs_ok = (repo_root / docs_path).exists() if docs_path else False
        status = "ok" if required_ok and tests_ok and docs_ok else "failed"

        req_cell = ", ".join(required_files) if required_files else "-"
        tests_cell = ", ".join(test_paths) if test_paths else "-"

        lines.append(
            "| "
            f"`{name}` | `{kind}` | `{pkg_path}` | `{req_cell}` ({_fmt_bool(required_ok)}) | "
            f"`{tests_cell}` ({_fmt_bool(tests_ok)}) | `{docs_path}` ({_fmt_bool(docs_ok)}) | `{status}` |"
        )

    lines.extend(
        [
            "",
            "## Enforcement",
            "",
            "- Local generate: `make package-structure-matrix`",
            "- Local check: `make package-structure-matrix-check`",
            "- CI: enforced via `make gate-all`.",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--standards", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    standards = _load_json(repo_root / args.standards)
    output = repo_root / args.output
    rendered = _render(repo_root, standards)

    if args.check:
        current = output.read_text(encoding="utf-8") if output.exists() else ""
        if current != rendered:
            print(
                "Package structure matrix check failed: output is stale. "
                "Run make package-structure-matrix.",
                file=sys.stderr,
            )
            return 1
        print("Package structure matrix check passed.")
        return 0

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(rendered, encoding="utf-8")
    print(f"Wrote {output.relative_to(repo_root).as_posix()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
