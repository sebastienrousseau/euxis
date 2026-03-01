"""Execution policy for bridged skill sandboxing and verification."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any


@dataclass(frozen=True, slots=True)
class FilesystemPolicy:
    """Controls filesystem access for skill execution."""

    read_only: bool = True
    write_paths: tuple[str, ...] = ()


@dataclass(frozen=True, slots=True)
class NetworkPolicy:
    """Controls network access for skill execution."""

    deny_all: bool = True
    allowed_hosts: tuple[str, ...] = ()


@dataclass(frozen=True, slots=True)
class ResourceLimits:
    """Resource bounds for skill execution."""

    memory_mb: int = 256
    timeout_seconds: int = 20


@dataclass(frozen=True, slots=True)
class SkillExecutionPolicy:
    """Policy governing bridged skill execution."""

    require_signature: bool = False
    public_key_path: str | None = None
    filesystem: FilesystemPolicy = field(default_factory=FilesystemPolicy)
    network: NetworkPolicy = field(default_factory=NetworkPolicy)
    resources: ResourceLimits = field(default_factory=ResourceLimits)
    output_schema: dict[str, Any] | None = None
    audit_log_path: str | None = None

    @classmethod
    def permissive(cls) -> SkillExecutionPolicy:
        """Create a permissive policy suitable for trusted local skills."""
        return cls(
            require_signature=False,
            filesystem=FilesystemPolicy(read_only=False),
            network=NetworkPolicy(deny_all=False),
            resources=ResourceLimits(memory_mb=512, timeout_seconds=60),
        )
