"""Platform adapter layer for euxis-bridge."""

from .contracts import PlatformInfo, PlatformOps
from .factory import resolve_platform_ops

__all__ = ["PlatformInfo", "PlatformOps", "resolve_platform_ops"]
