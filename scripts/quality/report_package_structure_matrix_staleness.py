#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Report package structure matrix staleness with summary and diff artifacts."""

from __future__ import annotations

import argparse
import difflib
import json
from pathlib import Path


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _fmt_bool(value: bool) -> str:
    return "yes" if value else "no"


def _render(repo_root: Path, standards: dict) -> str:
    lines = [
        "# Package Structure Matrix",
        "",
        "Generated from `euxis-ops/quality/package_standards.json`.",
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
    parser.add_argument("--matrix", required=True)
    parser.add_argument("--summary-output", required=True)
    parser.add_argument("--diff-output", required=True)
    parser.add_argument("--warn-only", action="store_true")
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    standards = _load_json(repo_root / args.standards)
    matrix_path = repo_root / args.matrix

    expected = _render(repo_root, standards)
    actual = matrix_path.read_text(encoding="utf-8") if matrix_path.exists() else ""

    stale = expected != actual

    diff_lines = list(
        difflib.unified_diff(
            actual.splitlines(),
            expected.splitlines(),
            fromfile=args.matrix,
            tofile=f"{args.matrix} (expected)",
            lineterm="",
        )
    )

    summary_lines = [
        "## Package Structure Matrix Staleness",
        "",
        f"- Matrix path: `{args.matrix}`",
        f"- Status: `{'stale' if stale else 'up-to-date'}`",
        f"- Blocking mode: `{'no (warn-only)' if args.warn_only else 'yes'}`",
        "",
    ]

    if stale:
        summary_lines.append("Matrix is stale. Regenerate with `make package-structure-matrix`.")
    else:
        summary_lines.append("Matrix is up to date.")

    summary_path = repo_root / args.summary_output
    diff_path = repo_root / args.diff_output
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    diff_path.parent.mkdir(parents=True, exist_ok=True)
    summary_path.write_text("\n".join(summary_lines) + "\n", encoding="utf-8")
    diff_path.write_text("\n".join(diff_lines) + ("\n" if diff_lines else ""), encoding="utf-8")

    print("\n".join(summary_lines))

    if stale and not args.warn_only:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
