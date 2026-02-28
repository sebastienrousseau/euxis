#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Validate package-level excellence controls across all workspace packages."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


TRANSIENT_DIR_NAMES = {".venv", ".pytest_cache", ".ruff_cache"}


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _exists_any(root: Path, rel_paths: list[str]) -> bool:
    return any((root / rel).exists() for rel in rel_paths)


def _find_transient_dirs(pkg_root: Path) -> list[Path]:
    found: list[Path] = []
    for candidate in pkg_root.rglob("*"):
        if candidate.is_dir() and candidate.name in TRANSIENT_DIR_NAMES:
            found.append(candidate)
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=".", help="Repository root")
    parser.add_argument("--standards", required=True, help="Package standards JSON")
    parser.add_argument("--resource-policy", required=True, help="Package resource policy JSON")
    parser.add_argument("--json-output", default="", help="Optional JSON report output")
    args = parser.parse_args()

    root = Path(args.repo_root).resolve()
    standards = _load_json(root / args.standards)
    policy = _load_json(root / args.resource_policy).get("packages", {})

    errors: list[str] = []
    report: dict[str, dict[str, object]] = {}

    for pkg in standards.get("packages", []):
        name = str(pkg.get("name", ""))
        pkg_root = root / str(pkg.get("path", ""))
        required_files = [str(x) for x in pkg.get("required_files", [])]
        test_paths = [str(x) for x in pkg.get("test_paths", [])]
        docs_path = root / str(pkg.get("docs_path", ""))
        kind = str(pkg.get("kind", "unknown"))

        pkg_errors: list[str] = []
        if not pkg_root.exists():
            pkg_errors.append("package path missing")
        for req in required_files:
            if not (pkg_root / req).exists():
                pkg_errors.append(f"required file missing: {req}")

        transient_dirs = _find_transient_dirs(pkg_root) if pkg_root.exists() else []
        if transient_dirs:
            rel = [p.relative_to(root).as_posix() for p in sorted(transient_dirs)[:5]]
            extra = "..." if len(transient_dirs) > 5 else ""
            pkg_errors.append(f"transient directories present: {', '.join(rel)}{extra}")

        if test_paths and not _exists_any(root, test_paths):
            pkg_errors.append("no declared test path exists")

        if not docs_path.exists():
            pkg_errors.append(f"docs page missing: {docs_path.relative_to(root).as_posix()}")
        elif name not in docs_path.read_text(encoding="utf-8"):
            pkg_errors.append("docs page does not mention package name")

        if name not in policy:
            pkg_errors.append("missing package resource policy entry")

        # Minimal usability/security baselines.
        readme_exists = (pkg_root / "README.md").exists()
        has_manifest = any((pkg_root / filename).exists() for filename in ("pyproject.toml", "Cargo.toml", "package.json"))
        if kind not in {"runtime-data", "ops"} and not has_manifest:
            pkg_errors.append("no package manifest found (pyproject/Cargo/package.json)")
        if kind not in {"runtime-data", "ops", "rust"} and not readme_exists:
            pkg_errors.append("README.md missing")

        if pkg_errors:
            errors.extend(f"{name}: {err}" for err in pkg_errors)
            status = "failed"
        else:
            status = "ok"

        report[name] = {
            "status": status,
            "kind": kind,
            "docs_path": str(pkg.get("docs_path", "")),
            "test_paths": test_paths,
            "required_files": required_files,
        }

    summary = {
        "status": "ok" if not errors else "failed",
        "packages_checked": len(report),
        "packages_passed": sum(1 for v in report.values() if v["status"] == "ok"),
        "packages_failed": sum(1 for v in report.values() if v["status"] != "ok"),
        "errors": errors,
        "packages": report,
    }

    if args.json_output:
        out = root / args.json_output
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    if errors:
        print("Package excellence validation failed.", file=sys.stderr)
        for err in errors:
            print(f"- {err}", file=sys.stderr)
        return 1

    print("Package excellence validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
