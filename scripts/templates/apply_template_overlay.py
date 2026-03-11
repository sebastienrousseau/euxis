#!/usr/bin/env python3
"""Apply template conformance manifests across all registered euxis packages."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


KIND_PLATFORM_SUPPORT = {
    "python": ["macos", "linux", "wsl"],
    "python-rust": ["macos", "linux", "wsl"],
    "node": ["macos", "linux", "wsl"],
    "rust": ["macos", "linux", "wsl"],
    "runtime-data": ["macos", "linux", "wsl"],
    "ops": ["macos", "linux", "wsl"],
    "docs": ["macos", "linux", "wsl"],
}


def _module_from_pkg_name(name: str) -> str:
    return name.replace("-", "_")


def _manifest_for_package(pkg: dict) -> dict:
    name = str(pkg["name"])
    kind = str(pkg.get("kind", "unknown"))
    module = _module_from_pkg_name(name)
    is_code_pkg = kind in {"python", "python-rust", "node", "rust"}

    return {
        "schema_version": "2026.1",
        "package": name,
        "kind": kind,
        "module": module,
        "required_files": pkg.get("required_files", []),
        "test_paths": pkg.get("test_paths", []),
        "docs_path": pkg.get("docs_path", ""),
        "platform_support": KIND_PLATFORM_SUPPORT.get(kind, ["macos", "linux", "wsl"]),
        "core_platform_separation": {
            "required": kind in {"python", "python-rust"},
            "core_paths": [f"src/{module}/core"],
            "platform_paths": [f"src/{module}/platform"],
            "platform_targets": ["macos", "linux", "wsl"],
            "notes": "Core logic should depend only on platform contracts/interfaces.",
        },
        "consistency_contract": {
            "docs_required": True,
            "tests_required": bool(pkg.get("test_paths", [])) or is_code_pkg,
            "strict_naming": True,
        },
    }


def _conformance_markdown(pkg: dict, manifest: dict) -> str:
    name = str(pkg["name"])
    kind = str(pkg.get("kind", "unknown"))
    req_files = ", ".join(manifest["required_files"]) or "(none)"
    tests = ", ".join(manifest["test_paths"]) or "(none)"
    docs = manifest["docs_path"] or "(none)"
    core_paths = ", ".join(manifest["core_platform_separation"]["core_paths"])
    platform_paths = ", ".join(manifest["core_platform_separation"]["platform_paths"])

    return (
        f"# Template Conformance: {name}\n\n"
        f"- Kind: `{kind}`\n"
        f"- Strict naming: `enabled`\n"
        f"- Platform targets: `macOS`, `Linux`, `WSL`\n\n"
        "## Required Files\n\n"
        f"- {req_files}\n\n"
        "## Documentation\n\n"
        f"- Module docs page: `{docs}`\n\n"
        "## Tests\n\n"
        f"- Test paths: `{tests}`\n\n"
        "## Core vs Platform Separation\n\n"
        f"- Core paths: `{core_paths}`\n"
        f"- Platform paths: `{platform_paths}`\n"
        "- Rule: Core logic must depend on contracts, not concrete OS adapters.\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--standards", default="euxis-ops/quality/package_standards.json")
    args = parser.parse_args()

    root = Path(args.repo_root).resolve()
    standards = json.loads((root / args.standards).read_text(encoding="utf-8"))

    for pkg in standards.get("packages", []):
        pkg_path = root / str(pkg["path"])
        pkg_path.mkdir(parents=True, exist_ok=True)

        manifest = _manifest_for_package(pkg)
        (pkg_path / ".euxis-template.json").write_text(
            json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
        )
        (pkg_path / "TEMPLATE_CONFORMANCE.md").write_text(
            _conformance_markdown(pkg, manifest), encoding="utf-8"
        )

    print("Template overlay applied to all registered packages.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
