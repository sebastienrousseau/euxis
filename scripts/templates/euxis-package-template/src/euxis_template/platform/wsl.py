"""WSL adapter implementation."""

from __future__ import annotations

from .contracts import PlatformInfo


class WSLOps:
    def platform_info(self) -> PlatformInfo:
        return PlatformInfo(platform="linux", runtime="wsl")

    def path_separator(self) -> str:
        return "/"

    def join_workspace(self, relative_path: str) -> str:
        return f"/mnt/c/Users/<user>/.euxis/{relative_path.strip('/')}"
