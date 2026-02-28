# SPDX-License-Identifier: AGPL-3.0-or-later

"""Ports for dependency inversion between core and platform-specific adapters."""

from __future__ import annotations

from pathlib import Path
from typing import Protocol

from .types import AgentExecutionRequest, AgentExecutionResult


class PlatformPort(Protocol):
    """Platform information and runtime capabilities."""

    def platform_name(self) -> str:
        """Return normalized platform id (linux|macos|windows|wsl|unknown)."""

    def is_wsl(self) -> bool:
        """Return True when running on WSL."""


class FilesystemPort(Protocol):
    """Filesystem operations used by core orchestration."""

    def ensure_dir(self, path: Path) -> Path:
        """Create directory when needed and return the canonical path."""

    def read_text(self, path: Path) -> str:
        """Read UTF-8 text."""

    def write_text(self, path: Path, content: str) -> None:
        """Write UTF-8 text atomically when possible."""


class ExecutionPort(Protocol):
    """Execution port for wasm/sandbox adapters."""

    def execute(self, request: AgentExecutionRequest) -> AgentExecutionResult:
        """Run the request in the configured runtime."""
