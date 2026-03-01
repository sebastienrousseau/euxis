"""Adapter factory placeholder.

Replace with package-specific platform resolution logic.
"""

from __future__ import annotations

from .contracts import PlatformInfo, PlatformOps

class _DefaultOps:
    def platform_info(self) -> PlatformInfo:
        return PlatformInfo(platform="unknown", runtime="generic")

    def path_separator(self) -> str:
        return "/"

def resolve_platform_ops() -> PlatformOps:
    return _DefaultOps()
