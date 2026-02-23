#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Agent Performance Metrics Collector.

Captures structured performance metrics from agent lifecycle events.
"""

import json
import os
import time
import uuid
from dataclasses import dataclass
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

TASK_STARTED_MAX_ARGS = 3
TASK_STARTED_PRIORITY_INDEX = 0
TASK_STARTED_COMPLEXITY_INDEX = 1
TASK_STARTED_PARENT_INDEX = 2

TASK_COMPLETED_MAX_ARGS = 5
TASK_COMPLETED_ARTIFACTS_INDEX = 0
TASK_COMPLETED_CORTEX_INDEX = 1
TASK_COMPLETED_TOOL_CALLS_INDEX = 2
TASK_COMPLETED_HANDOFF_INDEX = 3
TASK_COMPLETED_DELEGATION_INDEX = 4

TASK_FAILED_MAX_ARGS = 3
TASK_FAILED_ERROR_CATEGORY_INDEX = 0
TASK_FAILED_PARTIAL_INDEX = 1
TASK_FAILED_REFLEXION_INDEX = 2

DELEGATION_COMPLETED_MAX_ARGS = 3
DELEGATION_DURATION_INDEX = 0
DELEGATION_HANDOFF_INDEX = 1
DELEGATION_QUALITY_INDEX = 2

TOOL_EXECUTION_MAX_ARGS = 3
TOOL_EXECUTION_SUCCESS_INDEX = 0
TOOL_EXECUTION_RETRIES_INDEX = 1
TOOL_EXECUTION_CORRELATION_INDEX = 2


@dataclass(frozen=True)
class TaskStartContext:
    """Optional metadata for a task start event."""

    task_type: str = "user_request"
    priority: str = "P2"
    task_complexity: str | None = None
    parent_session: str | None = None


@dataclass(frozen=True)
class TaskCompletionContext:
    """Optional metadata for a task completion event."""

    status: str = "SUCCESS"
    artifacts_created: int = 0
    cortex_operations: int = 0
    tool_calls_count: int = 0
    handoff_required: bool = False
    delegation_count: int = 0


@dataclass(frozen=True)
class TaskFailureContext:
    """Optional metadata for a task failure event."""

    failure_reason: str
    error_category: str = "internal_error"
    partial_completion: bool = False
    reflexion_generated: bool = False


@dataclass(frozen=True)
class DelegationContext:
    """Optional metadata for a delegation completion event."""

    target_agent: str
    total_duration_ms: int
    handoff_successful: bool = True
    quality_score: float | None = None


@dataclass(frozen=True)
class ToolExecutionContext:
    """Optional metadata for a tool execution event."""

    execution_duration_ms: int
    success: bool = True
    retries: int = 0
    correlation_id: str | None = None


class PerformanceMetricsCollector:
    """Collects and emits structured performance metrics for agent operations."""

    def __init__(self, metrics_dir: str | None = None) -> None:
        base = metrics_dir or str(Path(os.environ.get("EUXIS_HOME", str(Path.home() / ".euxis"))) / "euxis-runtime" / "metrics")
        self.metrics_dir = Path(base)
        self.events_file = self.metrics_dir / "events.jsonl"
        self.sessions_file = self.metrics_dir / "sessions.jsonl"
        self.schema_file = Path(__file__).resolve().parent / "schemas" / "agent-performance.json"

        # Ensure directories exist
        self.metrics_dir.mkdir(exist_ok=True)

        # Load schema for validation
        self.schema = self._load_schema()

        # Track active sessions for duration calculation
        self.active_sessions: dict[str, dict[str, Any]] = {}

    def _load_schema(self) -> dict[str, Any]:
        """Load the metrics schema for validation."""
        try:
            with self.schema_file.open() as f:
                return json.load(f)
        except FileNotFoundError:
            return {}

    def _generate_correlation_id(self) -> str:
        """Generate unique correlation ID for event tracking."""
        return f"metrics-{uuid.uuid4().hex[:12]}"

    def _emit_event(self, event_type: str, properties: dict[str, Any]) -> None:
        """Emit structured event to metrics stream."""
        event = {
            "event_type": event_type,
            "timestamp": datetime.now(UTC).isoformat(),
            "properties": properties,
        }

        # Write to events stream
        with self.events_file.open("a") as f:
            f.write(json.dumps(event) + "\n")

    def task_started(
        self,
        agent_id: str,
        session_id: str,
        context: TaskStartContext | str | None = None,
        *args: Any,
        **kwargs: Any,
    ) -> str:
        """Record task start event and return correlation ID."""
        if context is None:
            context = TaskStartContext(**kwargs)
        elif isinstance(context, TaskStartContext):
            if args or kwargs:
                msg = "Unexpected extra arguments when TaskStartContext is provided"
                raise TypeError(msg)
        elif isinstance(context, str):
            if len(args) > TASK_STARTED_MAX_ARGS:
                msg = "Too many positional arguments for task_started"
                raise TypeError(msg)
            task_type = context
            priority = (
                args[TASK_STARTED_PRIORITY_INDEX]
                if len(args) > TASK_STARTED_PRIORITY_INDEX
                else "P2"
            )
            task_complexity = (
                args[TASK_STARTED_COMPLEXITY_INDEX]
                if len(args) > TASK_STARTED_COMPLEXITY_INDEX
                else None
            )
            parent_session = (
                args[TASK_STARTED_PARENT_INDEX]
                if len(args) > TASK_STARTED_PARENT_INDEX
                else None
            )
            context = TaskStartContext(
                task_type=task_type,
                priority=priority,
                task_complexity=task_complexity,
                parent_session=parent_session,
                **kwargs,
            )
        else:
            msg = "context must be TaskStartContext or task_type string"
            raise TypeError(msg)
        correlation_id = self._generate_correlation_id()

        start_time = time.time()
        self.active_sessions[session_id] = {
            "agent_id": agent_id,
            "start_time": start_time,
            "correlation_id": correlation_id,
            "task_type": context.task_type,
            "priority": context.priority,
        }

        properties = {
            "correlation_id": correlation_id,
            "agent_id": agent_id,
            "session_id": session_id,
            "task_type": context.task_type,
            "priority": context.priority,
        }

        if context.task_complexity:
            properties["task_complexity"] = context.task_complexity
        if context.parent_session:
            properties["parent_session"] = context.parent_session

        self._emit_event("Agent:TaskStarted", properties)
        return correlation_id

    def task_completed(
        self,
        session_id: str,
        context: TaskCompletionContext | str | None = None,
        *args: Any,
        **kwargs: Any,
    ) -> None:
        """Record task completion event."""
        if session_id not in self.active_sessions:
            return  # Session not tracked
        if context is None:
            context = TaskCompletionContext(**kwargs)
        elif isinstance(context, TaskCompletionContext):
            if args or kwargs:
                msg = "Unexpected extra arguments when TaskCompletionContext is provided"
                raise TypeError(msg)
        elif isinstance(context, str):
            if len(args) > TASK_COMPLETED_MAX_ARGS:
                msg = "Too many positional arguments for task_completed"
                raise TypeError(msg)
            status = context
            artifacts_created = (
                args[TASK_COMPLETED_ARTIFACTS_INDEX]
                if len(args) > TASK_COMPLETED_ARTIFACTS_INDEX
                else 0
            )
            cortex_operations = (
                args[TASK_COMPLETED_CORTEX_INDEX]
                if len(args) > TASK_COMPLETED_CORTEX_INDEX
                else 0
            )
            tool_calls_count = (
                args[TASK_COMPLETED_TOOL_CALLS_INDEX]
                if len(args) > TASK_COMPLETED_TOOL_CALLS_INDEX
                else 0
            )
            handoff_required = (
                args[TASK_COMPLETED_HANDOFF_INDEX]
                if len(args) > TASK_COMPLETED_HANDOFF_INDEX
                else False
            )
            delegation_count = (
                args[TASK_COMPLETED_DELEGATION_INDEX]
                if len(args) > TASK_COMPLETED_DELEGATION_INDEX
                else 0
            )
            context = TaskCompletionContext(
                status=status,
                artifacts_created=artifacts_created,
                cortex_operations=cortex_operations,
                tool_calls_count=tool_calls_count,
                handoff_required=handoff_required,
                delegation_count=delegation_count,
                **kwargs,
            )
        else:
            msg = "context must be TaskCompletionContext or status string"
            raise TypeError(msg)

        session_data = self.active_sessions.pop(session_id)
        duration_ms = int((time.time() - session_data["start_time"]) * 1000)

        properties = {
            "correlation_id": session_data["correlation_id"],
            "agent_id": session_data["agent_id"],
            "session_id": session_id,
            "duration_ms": duration_ms,
            "status": context.status,
            "artifacts_created": context.artifacts_created,
            "cortex_operations": context.cortex_operations,
            "tool_calls_count": context.tool_calls_count,
            "handoff_required": context.handoff_required,
            "delegation_count": context.delegation_count,
        }

        self._emit_event("Agent:TaskCompleted", properties)

        # Record session summary
        session_summary = {
            "session_id": session_id,
            "agent_id": session_data["agent_id"],
            "duration_ms": duration_ms,
            "status": context.status,
            "task_type": session_data["task_type"],
            "priority": session_data["priority"],
            "completed_at": datetime.now(UTC).isoformat()
        }

        with self.sessions_file.open("a") as f:
            f.write(json.dumps(session_summary) + "\n")

    def task_failed(
        self,
        session_id: str,
        context: TaskFailureContext | str | None = None,
        *args: Any,
        **kwargs: Any,
    ) -> None:
        """Record task failure event."""
        if session_id not in self.active_sessions:
            return
        if context is None:
            context = TaskFailureContext(**kwargs)
        elif isinstance(context, TaskFailureContext):
            if args or kwargs:
                msg = "Unexpected extra arguments when TaskFailureContext is provided"
                raise TypeError(msg)
        elif isinstance(context, str):
            if len(args) > TASK_FAILED_MAX_ARGS:
                msg = "Too many positional arguments for task_failed"
                raise TypeError(msg)
            failure_reason = context
            error_category = (
                args[TASK_FAILED_ERROR_CATEGORY_INDEX]
                if len(args) > TASK_FAILED_ERROR_CATEGORY_INDEX
                else "internal_error"
            )
            partial_completion = (
                args[TASK_FAILED_PARTIAL_INDEX]
                if len(args) > TASK_FAILED_PARTIAL_INDEX
                else False
            )
            reflexion_generated = (
                args[TASK_FAILED_REFLEXION_INDEX]
                if len(args) > TASK_FAILED_REFLEXION_INDEX
                else False
            )
            context = TaskFailureContext(
                failure_reason=failure_reason,
                error_category=error_category,
                partial_completion=partial_completion,
                reflexion_generated=reflexion_generated,
                **kwargs,
            )
        else:
            msg = "context must be TaskFailureContext or failure_reason string"
            raise TypeError(msg)

        session_data = self.active_sessions.pop(session_id)
        duration_ms = int((time.time() - session_data["start_time"]) * 1000)

        properties = {
            "correlation_id": session_data["correlation_id"],
            "agent_id": session_data["agent_id"],
            "session_id": session_id,
            "duration_ms": duration_ms,
            "failure_reason": context.failure_reason,
            "error_category": context.error_category,
            "partial_completion": context.partial_completion,
            "reflexion_generated": context.reflexion_generated,
        }

        self._emit_event("Agent:TaskFailed", properties)

    def delegation_started(self, delegating_agent: str, target_agent: str,
                          delegation_reason: str = "scope_boundary", priority: str = "P2") -> str:
        """Record delegation start event."""
        correlation_id = self._generate_correlation_id()

        properties = {
            "correlation_id": correlation_id,
            "delegating_agent": delegating_agent,
            "target_agent": target_agent,
            "delegation_reason": delegation_reason,
            "priority": priority
        }

        self._emit_event("Agent:DelegationStarted", properties)
        return correlation_id

    def delegation_completed(
        self,
        correlation_id: str,
        delegating_agent: str,
        context: DelegationContext | str | None = None,
        *args: Any,
        **kwargs: Any,
    ) -> None:
        """Record delegation completion event."""
        if context is None:
            context = DelegationContext(**kwargs)
        elif isinstance(context, DelegationContext):
            if args or kwargs:
                msg = "Unexpected extra arguments when DelegationContext is provided"
                raise TypeError(msg)
        elif isinstance(context, str):
            if len(args) > DELEGATION_COMPLETED_MAX_ARGS:
                msg = "Too many positional arguments for delegation_completed"
                raise TypeError(msg)
            target_agent = context
            total_duration_ms = (
                args[DELEGATION_DURATION_INDEX]
                if len(args) > DELEGATION_DURATION_INDEX
                else 0
            )
            handoff_successful = (
                args[DELEGATION_HANDOFF_INDEX]
                if len(args) > DELEGATION_HANDOFF_INDEX
                else True
            )
            quality_score = (
                args[DELEGATION_QUALITY_INDEX]
                if len(args) > DELEGATION_QUALITY_INDEX
                else None
            )
            context = DelegationContext(
                target_agent=target_agent,
                total_duration_ms=total_duration_ms,
                handoff_successful=handoff_successful,
                quality_score=quality_score,
                **kwargs,
            )
        else:
            msg = "context must be DelegationContext or target_agent string"
            raise TypeError(msg)

        properties = {
            "correlation_id": correlation_id,
            "delegating_agent": delegating_agent,
            "target_agent": context.target_agent,
            "total_duration_ms": context.total_duration_ms,
            "handoff_successful": context.handoff_successful,
        }

        if context.quality_score is not None:
            properties["quality_score"] = context.quality_score

        self._emit_event("Agent:DelegationCompleted", properties)

    def memory_operation(self, agent_id: str, operation: str, memory_type: str,
                        operation_duration_ms: int, correlation_id: str | None = None) -> None:
        """Record Cortex memory operation."""
        if not correlation_id:
            correlation_id = self._generate_correlation_id()

        properties = {
            "correlation_id": correlation_id,
            "agent_id": agent_id,
            "operation": operation,
            "memory_type": memory_type,
            "operation_duration_ms": operation_duration_ms
        }

        self._emit_event("Agent:MemoryOperation", properties)

    def tool_execution(
        self,
        agent_id: str,
        tool_name: str,
        context: ToolExecutionContext | int | None = None,
        *args: Any,
        **kwargs: Any,
    ) -> None:
        """Record tool execution metrics."""
        if context is None:
            context = ToolExecutionContext(**kwargs)
        elif isinstance(context, ToolExecutionContext):
            if args or kwargs:
                msg = "Unexpected extra arguments when ToolExecutionContext is provided"
                raise TypeError(msg)
        elif isinstance(context, int):
            if len(args) > TOOL_EXECUTION_MAX_ARGS:
                msg = "Too many positional arguments for tool_execution"
                raise TypeError(msg)
            execution_duration_ms = context
            success = (
                args[TOOL_EXECUTION_SUCCESS_INDEX]
                if len(args) > TOOL_EXECUTION_SUCCESS_INDEX
                else True
            )
            retries = (
                args[TOOL_EXECUTION_RETRIES_INDEX]
                if len(args) > TOOL_EXECUTION_RETRIES_INDEX
                else 0
            )
            correlation_id = (
                args[TOOL_EXECUTION_CORRELATION_INDEX]
                if len(args) > TOOL_EXECUTION_CORRELATION_INDEX
                else None
            )
            context = ToolExecutionContext(
                execution_duration_ms=execution_duration_ms,
                success=success,
                retries=retries,
                correlation_id=correlation_id,
                **kwargs,
            )
        else:
            msg = "context must be ToolExecutionContext or duration integer"
            raise TypeError(msg)

        correlation_id = context.correlation_id or self._generate_correlation_id()

        properties = {
            "correlation_id": correlation_id,
            "agent_id": agent_id,
            "tool_name": tool_name,
            "execution_duration_ms": context.execution_duration_ms,
            "success": context.success,
        }

        if context.retries > 0:
            properties["retries"] = context.retries

        self._emit_event("Agent:ToolExecution", properties)

    def reflexion_generated(
        self,
        agent_id: str,
        failure_category: str,
        contraindication_stored: bool = True,
        correlation_id: str | None = None,
    ) -> None:
        """Record reflexion generation event."""
        if not correlation_id:
            correlation_id = self._generate_correlation_id()

        properties = {
            "correlation_id": correlation_id,
            "agent_id": agent_id,
            "failure_category": failure_category,
            "contraindication_stored": contraindication_stored
        }

        self._emit_event("Agent:ReflexionGenerated", properties)

    def conflict_detected(
        self,
        agents: list,
        conflict_type: str,
        resolution_method: str,
        correlation_id: str | None = None,
    ) -> None:
        """Record conflict detection event."""
        if not correlation_id:
            correlation_id = self._generate_correlation_id()

        properties = {
            "correlation_id": correlation_id,
            "agents": agents,
            "conflict_type": conflict_type,
            "resolution_method": resolution_method
        }

        self._emit_event("Agent:ConflictDetected", properties)

    def cleanup_stale_sessions(self, timeout_seconds: int = 3600) -> int:
        """Clean up stale sessions that have exceeded timeout."""
        current_time = time.time()
        stale_sessions = []

        for session_id, session_data in self.active_sessions.items():
            if current_time - session_data["start_time"] > timeout_seconds:
                stale_sessions.append(session_id)

        for session_id in stale_sessions:
            session_data = self.active_sessions.pop(session_id)
            self.task_failed(session_id, "Session timeout", "timeout")

        return len(stale_sessions)

# Global collector instance
collector = PerformanceMetricsCollector()
