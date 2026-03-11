#!/usr/bin/env python3
"""Validate template conformance artifacts across all registered packages."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--standards", default="euxis-ops/quality/package_standards.json")
    parser.add_argument(
        "--json-output",
        default="data/release/template-conformance.json",
    )
    args = parser.parse_args()

    root = Path(args.repo_root).resolve()
    standards = json.loads((root / args.standards).read_text(encoding="utf-8"))

    failures: list[str] = []
    packages: list[dict[str, object]] = []

    for pkg in standards.get("packages", []):
        name = str(pkg["name"])
        pkg_path = root / str(pkg["path"])
        manifest_path = pkg_path / ".euxis-template.json"
        md_path = pkg_path / "TEMPLATE_CONFORMANCE.md"

        missing: list[str] = []
        if not manifest_path.exists():
            missing.append(str(manifest_path.relative_to(root)))
        if not md_path.exists():
            missing.append(str(md_path.relative_to(root)))

        status = "ok" if not missing else "failed"
        packages.append({"name": name, "status": status, "missing": missing})
        if missing:
            failures.append(f"{name}: missing {missing}")

    result = {
        "status": "ok" if not failures else "failed",
        "packages": packages,
        "failures": failures,
    }

    output = root / args.json_output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")

    if failures:
        print("Template conformance validation failed.")
        print(json.dumps(result, indent=2))
        return 1

    print("Template conformance validation passed.")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
