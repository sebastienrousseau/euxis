"""Public package API for euxis-template."""

from .core.service import CoreService
from .platform.factory import resolve_platform_ops

__all__ = ["CoreService", "resolve_platform_ops"]
