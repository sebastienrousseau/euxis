#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Enforce Euxis architectural boundaries.

Phase 1 gate: core logic must not depend on platform/application layers.
"""

from __future__ import annotations

import ast
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
CORE_ROOT = REPO_ROOT / "euxis-core" / "src" / "euxis_core"

# Core may depend on stdlib + its own package only.
BANNED_TOP_LEVEL_IMPORTS = {
    "adapters",
    "gateway",
    "cli",
    "tui",
    "metrics",
    "security",
}

# OS-specific modules must stay isolated in dedicated platform layers.
BANNED_OS_IMPORTS = {
    "winreg",
    "winsound",
    "msvcrt",
    "AppKit",
    "Foundation",
}

# Files in this subtree are allowed to use platform-specific modules.
PLATFORM_ALLOWLIST = (
    "platform/",
    "runtime/",
)

# Transitional adapter zones where IO/networking is currently isolated.
ADAPTER_IO_ALLOWLIST = (
    "platform/",
    "runtime/",
)

# IO/process modules forbidden in pure core logic.
BANNED_IO_IMPORTS = {
    "subprocess",
    "socket",
    "requests",
    "httpx",
    "websockets",
    "aiohttp",
    "urllib",
}

# Async runtime primitives must stay in runtime adapters.
BANNED_ASYNC_IMPORTS = {
    "asyncio",
}


def _is_python_file(path: Path) -> bool:
    return path.suffix == ".py" and "__pycache__" not in path.parts


def _platform_allowed(relative_path: str) -> bool:
    return any(relative_path.startswith(prefix) for prefix in PLATFORM_ALLOWLIST)


def _io_allowed(relative_path: str) -> bool:
    return any(relative_path.startswith(prefix) for prefix in ADAPTER_IO_ALLOWLIST)


def _async_allowed(relative_path: str) -> bool:
    return relative_path.startswith("runtime/")


def _scan_file(path: Path) -> list[str]:
    rel = path.relative_to(CORE_ROOT).as_posix()
    errors: list[str] = []
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))

    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                top = alias.name.split(".", 1)[0]
                if top in BANNED_TOP_LEVEL_IMPORTS:
                    errors.append(
                        f"{rel}:{node.lineno}: forbidden core dependency import '{alias.name}'"
                    )
                if top in BANNED_OS_IMPORTS and not _platform_allowed(rel):
                    errors.append(
                        f"{rel}:{node.lineno}: OS-specific import '{alias.name}' is not allowed in core"
                    )
                if top in BANNED_IO_IMPORTS and not _io_allowed(rel):
                    errors.append(
                        f"{rel}:{node.lineno}: IO/network import '{alias.name}' must live in adapter dirs"
                    )
                if top in BANNED_ASYNC_IMPORTS and not _async_allowed(rel):
                    errors.append(
                        f"{rel}:{node.lineno}: async runtime import '{alias.name}' must live in runtime/"
                    )
        elif isinstance(node, ast.ImportFrom):
            # Relative imports are internal to the same package boundary.
            if node.level and node.level > 0:
                continue
            if node.module is None:
                continue
            top = node.module.split(".", 1)[0]
            if top in BANNED_TOP_LEVEL_IMPORTS:
                errors.append(
                    f"{rel}:{node.lineno}: forbidden core dependency import 'from {node.module} import ...'"
                )
            if top in BANNED_OS_IMPORTS and not _platform_allowed(rel):
                errors.append(
                    f"{rel}:{node.lineno}: OS-specific import '{node.module}' is not allowed in core"
                )
            if top in BANNED_IO_IMPORTS and not _io_allowed(rel):
                errors.append(
                    f"{rel}:{node.lineno}: IO/network import '{node.module}' must live in adapter dirs"
                )
            if top in BANNED_ASYNC_IMPORTS and not _async_allowed(rel):
                errors.append(
                    f"{rel}:{node.lineno}: async runtime import '{node.module}' must live in runtime/"
                )

    return errors


def main() -> int:
    if not CORE_ROOT.exists():
        print(f"Core path not found: {CORE_ROOT} (skipping Python-specific check)", file=sys.stderr)
        return 0

    errors: list[str] = []
    for file_path in CORE_ROOT.rglob("*.py"):
        if _is_python_file(file_path):
            errors.extend(_scan_file(file_path))

    if errors:
        print("Architectural boundary violations detected:\n", file=sys.stderr)
        for err in sorted(errors):
            print(f" - {err}", file=sys.stderr)
        return 1

    print("Architecture boundary check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
