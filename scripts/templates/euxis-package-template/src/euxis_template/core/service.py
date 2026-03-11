"""Platform-agnostic service logic.

This layer depends only on protocol contracts, never concrete platform adapters.
"""

from __future__ import annotations

from dataclasses import dataclass

from euxis_template.platform.contracts import PlatformOps


@dataclass(slots=True)
class CoreService:
    """Core orchestration service independent of operating system details."""

    ops: PlatformOps

    def health_snapshot(self) -> dict[str, str]:
        info = self.ops.platform_info()
        return {
            "platform": info.platform,
            "runtime": info.runtime,
            "path_separator": self.ops.path_separator(),
        }

    def normalize_workspace_path(self, relative_path: str) -> str:
        """Normalize workspace path using platform adapter semantics."""
        return self.ops.join_workspace(relative_path)
