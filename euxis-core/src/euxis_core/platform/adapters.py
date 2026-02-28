# SPDX-License-Identifier: AGPL-3.0-or-later

"""Default local adapters implementing core ports."""

from __future__ import annotations

import os
import platform as py_platform
from pathlib import Path


class RuntimePlatformAdapter:
    """OS detection isolated from pure core logic."""

    def platform_name(self) -> str:
        if self.is_wsl():
            return "wsl"

        system = py_platform.system().lower()
        if system == "darwin":
            return "macos"
        if system == "linux":
            return "linux"
        if system == "windows":
            return "windows"
        return "unknown"

    def is_wsl(self) -> bool:
        if os.environ.get("WSL_DISTRO_NAME"):
            return True
        try:
            version = Path("/proc/version")
            if version.exists():
                return "microsoft" in version.read_text(encoding="utf-8").lower()
        except OSError:
            return False
        return False


class LocalFilesystemAdapter:
    """Filesystem adapter for local runtime usage."""

    def ensure_dir(self, path: Path) -> Path:
        path.mkdir(parents=True, exist_ok=True)
        return path.resolve()

    def read_text(self, path: Path) -> str:
        return path.read_text(encoding="utf-8")

    def write_text(self, path: Path, content: str) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
