#!/usr/bin/env python3
"""Generate multi-repo split-readiness checklist per package."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def _exists(root: Path, rel: str) -> bool:
    return (root / rel).exists()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--standards", default="euxis-ops/quality/package_standards.json")
    parser.add_argument("--json-output", default="data/release/repo-split-readiness.json")
    parser.add_argument("--md-output", default="data/release/repo-split-readiness.md")
    args = parser.parse_args()

    root = Path(args.repo_root).resolve()
    standards = json.loads((root / args.standards).read_text(encoding="utf-8"))

    packages = []
    for pkg in standards.get("packages", []):
        name = str(pkg["name"])
        pkg_path = str(pkg["path"])
        required_files = [str(x) for x in pkg.get("required_files", [])]
        test_paths = [str(x) for x in pkg.get("test_paths", [])]
        docs_path = str(pkg.get("docs_path", ""))

        req_ok = all(_exists(root / pkg_path, f) for f in required_files)
        tests_ok = True if not test_paths else any(_exists(root, t) for t in test_paths)
        docs_ok = _exists(root, docs_path) if docs_path else False
        ci_ok = _exists(root, ".github/workflows/strict-verify.yml")
        template_ok = _exists(root, f"{pkg_path}/.euxis-template.json") and _exists(root, f"{pkg_path}/TEMPLATE_CONFORMANCE.md")

        score = sum([req_ok, tests_ok, docs_ok, ci_ok, template_ok])
        status = "ready" if score == 5 else "partial"
        packages.append(
            {
                "name": name,
                "path": pkg_path,
                "required_files_ok": req_ok,
                "tests_ok": tests_ok,
                "docs_ok": docs_ok,
                "ci_ok": ci_ok,
                "template_ok": template_ok,
                "score": score,
                "status": status,
            }
        )

    result = {
        "status": "ok",
        "ready_count": sum(1 for p in packages if p["status"] == "ready"),
        "total_count": len(packages),
        "packages": packages,
    }

    json_path = root / args.json_output
    md_path = root / args.md_output
    json_path.parent.mkdir(parents=True, exist_ok=True)
    md_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# Repo Split Readiness",
        "",
        f"- Ready packages: **{result['ready_count']} / {result['total_count']}**",
        "",
        "| Package | Required Files | Tests | Docs | CI | Template | Score | Status |",
        "| --- | --- | --- | --- | --- | --- | --- | --- |",
    ]
    for p in packages:
        lines.append(
            f"| `{p['name']}` | {'yes' if p['required_files_ok'] else 'no'} | {'yes' if p['tests_ok'] else 'no'} | {'yes' if p['docs_ok'] else 'no'} | {'yes' if p['ci_ok'] else 'no'} | {'yes' if p['template_ok'] else 'no'} | {p['score']}/5 | `{p['status']}` |"
        )

    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
