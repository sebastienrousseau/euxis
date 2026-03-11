#!/usr/bin/env python3
"""Enforce baseline core/platform structure across registered code packages."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def _module_dir(root: Path, pkg: dict) -> Path | None:
    kind = str(pkg.get("kind", ""))
    if kind not in {"python", "python-rust", "runtime-data"}:
        return None

    pkg_root = root / str(pkg["path"])
    src_root = pkg_root / "src"
    if not src_root.exists():
        return None

    candidates = [p for p in src_root.iterdir() if p.is_dir() and not p.name.startswith("__")]
    if not candidates:
        return None

    pkg_name = str(pkg["name"])
    explicit_module_map = {
        "euxis-cli": "cli",
        "euxis-gateway": "gateway",
        "euxis-runtime": "runtime",
        "euxis-docs": "euxis_docs",
    }
    mapped = explicit_module_map.get(pkg_name)
    if mapped:
        mapped_path = src_root / mapped
        if mapped_path.exists():
            return mapped_path

    name = pkg_name.replace("-", "_")
    preferred = [p for p in candidates if p.name == name]
    if preferred:
        return preferred[0]

    if len(candidates) == 1:
        return candidates[0]
    return None


def _write_if_missing(path: Path, content: str) -> bool:
    if path.exists():
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--standards", default="euxis-ops/quality/package_standards.json")
    parser.add_argument("--json-output", default="euxis-data/release/structure-enforcement.json")
    args = parser.parse_args()

    root = Path(args.repo_root).resolve()
    standards = json.loads((root / args.standards).read_text(encoding="utf-8"))

    created: dict[str, list[str]] = {}
    skipped: dict[str, str] = {}

    for pkg in standards.get("packages", []):
        pkg_name = str(pkg["name"])
        module_dir = _module_dir(root, pkg)
        if module_dir is None:
            skipped[pkg_name] = "non-python package or ambiguous src module directory"
            continue

        module_name = module_dir.name
        files_created: list[str] = []

        # Avoid namespace collisions when a module already uses `core.py`.
        if not (module_dir / "core.py").exists():
            core_init = module_dir / "core" / "__init__.py"
            if _write_if_missing(core_init, '"""Core domain logic for this package."""\n'):
                files_created.append(str(core_init.relative_to(root)))

        platform_init = module_dir / "platform" / "__init__.py"
        if _write_if_missing(platform_init, '"""Platform adapter layer for this package."""\n'):
            files_created.append(str(platform_init.relative_to(root)))

        contracts = module_dir / "platform" / "contracts.py"
        contracts_content = (
            '"""Platform contracts for adapter implementations.\n\n'
            "Core modules should depend on these interfaces instead of concrete OS adapters.\n"
            '"""\n\n'
            "from __future__ import annotations\n\n"
            "from dataclasses import dataclass\n"
            "from typing import Protocol\n\n"
            "@dataclass(frozen=True, slots=True)\n"
            "class PlatformInfo:\n"
            "    platform: str\n"
            "    runtime: str\n\n"
            "class PlatformOps(Protocol):\n"
            "    def platform_info(self) -> PlatformInfo: ...\n"
            "    def path_separator(self) -> str: ...\n"
        )
        if _write_if_missing(contracts, contracts_content):
            files_created.append(str(contracts.relative_to(root)))

        factory = module_dir / "platform" / "factory.py"
        factory_content = (
            '"""Adapter factory placeholder.\n\n'
            "Replace with package-specific platform resolution logic.\n"
            '"""\n\n'
            "from __future__ import annotations\n\n"
            "from .contracts import PlatformInfo, PlatformOps\n\n"
            "class _DefaultOps:\n"
            "    def platform_info(self) -> PlatformInfo:\n"
            "        return PlatformInfo(platform=\"unknown\", runtime=\"generic\")\n\n"
            "    def path_separator(self) -> str:\n"
            "        return \"/\"\n\n"
            "def resolve_platform_ops() -> PlatformOps:\n"
            "    return _DefaultOps()\n"
        )
        if _write_if_missing(factory, factory_content):
            files_created.append(str(factory.relative_to(root)))

        readme = module_dir / "platform" / "README.md"
        readme_content = (
            f"# {module_name}.platform\n\n"
            "This directory isolates platform-specific behavior.\n\n"
            "Targets:\n"
            "- macOS\n"
            "- Linux\n"
            "- WSL\n\n"
            "Guidelines:\n"
            "- expose platform behavior through contracts in `contracts.py`\n"
            "- keep core logic under `../core` platform-agnostic\n"
            "- avoid importing concrete adapters directly from core modules\n"
        )
        if _write_if_missing(readme, readme_content):
            files_created.append(str(readme.relative_to(root)))

        if files_created:
            created[pkg_name] = files_created

    result = {
        "status": "ok",
        "created": created,
        "skipped": skipped,
    }

    out = root / args.json_output
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")

    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
