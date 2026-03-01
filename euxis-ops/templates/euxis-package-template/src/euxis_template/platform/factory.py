"""Factory for resolving current platform adapter."""

from __future__ import annotations

import os
import platform

from .contracts import PlatformOps
from .linux import LinuxOps
from .macos import MacOSOps
from .wsl import WSLOps


def _is_wsl() -> bool:
    if os.environ.get("WSL_DISTRO_NAME"):
        return True
    release = platform.release().lower()
    version = platform.version().lower()
    return "microsoft" in release or "microsoft" in version


def resolve_platform_ops() -> PlatformOps:
    system = platform.system().lower()
    if system == "darwin":
        return MacOSOps()
    if system == "linux" and _is_wsl():
        return WSLOps()
    if system == "linux":
        return LinuxOps()
    raise RuntimeError(f"unsupported platform: {system}")
