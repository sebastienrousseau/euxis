"""A2A protocol message and artifact types."""

from __future__ import annotations

import time
from dataclasses import dataclass, field
from typing import Any


@dataclass(frozen=True, slots=True)
class ContentPart:
    type: str  # "text", "image", "data"
    content: str
    mime_type: str = "text/plain"


@dataclass(frozen=True, slots=True)
class A2AMessage:
    role: str  # "user" or "agent"
    parts: tuple[ContentPart, ...]
    metadata: dict[str, Any] = field(default_factory=dict)
    timestamp: float = field(default_factory=time.time)


@dataclass(frozen=True, slots=True)
class Artifact:
    name: str
    type: str
    data: str
    metadata: dict[str, Any] = field(default_factory=dict)
