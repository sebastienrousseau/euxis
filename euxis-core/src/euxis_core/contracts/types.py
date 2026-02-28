# SPDX-License-Identifier: AGPL-3.0-or-later

"""Core value objects shared between orchestration and adapters."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Mapping


@dataclass(slots=True, frozen=True)
class AgentExecutionRequest:
    agent_id: str
    function_name: str
    payload: Mapping[str, Any]
    timeout_seconds: float = 30.0


@dataclass(slots=True, frozen=True)
class AgentExecutionResult:
    ok: bool
    output: Mapping[str, Any] = field(default_factory=dict)
    error: str | None = None
    latency_ms: int = 0
