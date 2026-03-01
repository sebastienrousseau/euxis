"""OS-specific contracts consumed by bridge core logic."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol


@dataclass(frozen=True, slots=True)
class PlatformInfo:
    platform: str
    runtime: str


class PlatformOps(Protocol):  # pragma: no cover - structural typing contract only
    def platform_info(self) -> PlatformInfo: ...
    def path_separator(self) -> str: ...
    def join_workspace(self, relative_path: str) -> str: ...
