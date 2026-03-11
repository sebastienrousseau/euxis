#!/usr/bin/env python3
"""Validate that core modules do not import concrete platform adapters."""

from __future__ import annotations

import argparse
import ast
import json
from pathlib import Path


def _module_candidates(pkg_root: Path) -> list[Path]:
    src = pkg_root / "src"
    if not src.exists():
        return []
    return [p for p in src.iterdir() if p.is_dir() and not p.name.startswith("__")]


def _violations_in_file(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    tree = ast.parse(text, filename=str(path))
    violations: list[str] = []

    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                name = alias.name
                if ".platform." in name and not name.endswith(".platform.contracts"):
                    violations.append(f"{path}: import {name}")
        elif isinstance(node, ast.ImportFrom):
            module = node.module or ""
            if module.startswith("platform") and module != "platform.contracts":
                violations.append(f"{path}: from {module} import ...")
            if ".platform." in module and not module.endswith(".platform.contracts"):
                violations.append(f"{path}: from {module} import ...")
            # relative imports like from ..platform import factory
            if node.level > 0 and module.startswith("platform") and module != "platform.contracts":
                violations.append(f"{path}: relative from {'.'*node.level}{module} import ...")

    return violations


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--standards", default="euxis-ops/quality/package_standards.json")
    parser.add_argument("--json-output", default="data/release/core-platform-boundaries.json")
    args = parser.parse_args()

    root = Path(args.repo_root).resolve()
    standards = json.loads((root / args.standards).read_text(encoding="utf-8"))

    failures: list[str] = []

    for pkg in standards.get("packages", []):
        if str(pkg.get("kind", "")) not in {"python", "python-rust", "runtime-data"}:
            continue
        pkg_root = root / str(pkg["path"])
        for module_dir in _module_candidates(pkg_root):
            core_dir = module_dir / "core"
            if not core_dir.exists():
                continue
            for py_file in core_dir.rglob("*.py"):
                failures.extend(_violations_in_file(py_file))

    result = {
        "status": "ok" if not failures else "failed",
        "violations": failures,
    }

    out = root / args.json_output
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")

    if failures:
        print("Core/platform boundary validation failed.")
        print(json.dumps(result, indent=2))
        return 1

    print("Core/platform boundary validation passed.")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
