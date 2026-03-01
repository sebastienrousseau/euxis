"""WSL platform adapter for euxis-bridge."""

from __future__ import annotations

import os
from pathlib import Path

from .contracts import PlatformInfo


class WSLOps:
    def platform_info(self) -> PlatformInfo:
        return PlatformInfo(platform="linux", runtime="wsl")

    def path_separator(self) -> str:
        return os.sep

    def join_workspace(self, relative_path: str) -> str:
        root = Path.home() / ".euxis"
        return str(root / relative_path.strip("/"))
