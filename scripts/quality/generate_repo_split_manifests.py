#!/usr/bin/env python3
"""Generate per-package repo extraction manifests."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--standards", default="euxis-ops/quality/package_standards.json")
    parser.add_argument("--output-dir", default="data/release/repo-split-manifests")
    args = parser.parse_args()

    root = Path(args.repo_root).resolve()
    standards = json.loads((root / args.standards).read_text(encoding="utf-8"))
    out_dir = root / args.output_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    for pkg in standards.get("packages", []):
        name = str(pkg["name"])
        path = str(pkg["path"])
        manifest = {
            "schema_version": "2026.1",
            "package": name,
            "repo_name": name,
            "include_paths": [path],
            "shared_paths": [".github/workflows", "LICENSE", "NOTICE"],
            "docs_module_page": str(pkg.get("docs_path", "")),
            "required_files": [f"{path}/{x}" for x in pkg.get("required_files", [])],
            "tests": pkg.get("test_paths", []),
            "release_checks": [
                "template-conformance-check",
                "core-platform-boundary-check",
                "package-resource-governance-check",
            ],
        }
        (out_dir / f"{name}.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    index = {
        "status": "ok",
        "count": len(standards.get("packages", [])),
        "output_dir": str(out_dir.relative_to(root)),
    }
    (out_dir / "index.json").write_text(json.dumps(index, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(index, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
