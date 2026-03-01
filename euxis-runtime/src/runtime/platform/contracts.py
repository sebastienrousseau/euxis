"""Platform contracts for adapter implementations.

Core modules should depend on these interfaces instead of concrete OS adapters.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol

@dataclass(frozen=True, slots=True)
class PlatformInfo:
    platform: str
    runtime: str

class PlatformOps(Protocol):
    def platform_info(self) -> PlatformInfo: ...
    def path_separator(self) -> str: ...
