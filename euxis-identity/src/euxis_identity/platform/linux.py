"""Linux adapter implementation."""

from __future__ import annotations

from .contracts import PlatformInfo


class LinuxOps:
    def platform_info(self) -> PlatformInfo:
        return PlatformInfo(platform="linux", runtime="native")

    def path_separator(self) -> str:
        return "/"

    def join_workspace(self, relative_path: str) -> str:
        return f"~/.euxis/{relative_path.strip('/')}"
