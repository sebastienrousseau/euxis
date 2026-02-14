#!/usr/bin/env python3
"""
Agent Performance Metrics Collector
Captures structured performance metrics from agent lifecycle events
"""

import json
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, Any, Optional
import uuid

class PerformanceMetricsCollector:
    """Collects and emits structured performance metrics for agent operations"""

    def __init__(self, metrics_dir: str = "/home/seb/.euxis/metrics"):
        self.metrics_dir = Path(metrics_dir)
        self.events_file = self.metrics_dir / "events.jsonl"
        self.sessions_file = self.metrics_dir / "sessions.jsonl"
        self.schema_file = self.metrics_dir / "schemas" / "agent-performance.json"

        # Ensure directories exist
        self.metrics_dir.mkdir(exist_ok=True)

        # Load schema for validation
        self.schema = self._load_schema()

        # Track active sessions for duration calculation
        self.active_sessions: Dict[str, Dict[str, Any]] = {}

    def _load_schema(self) -> Dict[str, Any]:
        """Load the metrics schema for validation"""
        try:
            with open(self.schema_file) as f:
                return json.load(f)
        except FileNotFoundError:
            return {}

    def _generate_correlation_id(self) -> str:
        """Generate unique correlation ID for event tracking"""
        return f"metrics-{uuid.uuid4().hex[:12]}"

    def _emit_event(self, event_type: str, properties: Dict[str, Any]) -> None:
        """Emit structured event to metrics stream"""
        event = {
            "event_type": event_type,
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "properties": properties
        }

        # Write to events stream
        with open(self.events_file, "a") as f:
            f.write(json.dumps(event) + "\n")

    def task_started(self, agent_id: str, session_id: str, task_type: str = "user_request",
                    priority: str = "P2", task_complexity: Optional[str] = None,
                    parent_session: Optional[str] = None) -> str:
        """Record task start event and return correlation ID"""
        correlation_id = self._generate_correlation_id()

        start_time = time.time()
        self.active_sessions[session_id] = {
            "agent_id": agent_id,
            "start_time": start_time,
            "correlation_id": correlation_id,
            "task_type": task_type,
            "priority": priority
        }

        properties = {
            "correlation_id": correlation_id,
            "agent_id": agent_id,
            "session_id": session_id,
            "task_type": task_type,
            "priority": priority
        }

        if task_complexity:
            properties["task_complexity"] = task_complexity
        if parent_session:
            properties["parent_session"] = parent_session

        self._emit_event("Agent:TaskStarted", properties)
        return correlation_id

    def task_completed(self, session_id: str, status: str = "SUCCESS",
                      artifacts_created: int = 0, cortex_operations: int = 0,
                      tool_calls_count: int = 0, handoff_required: bool = False,
                      delegation_count: int = 0) -> None:
        """Record task completion event"""
        if session_id not in self.active_sessions:
            return  # Session not tracked

        session_data = self.active_sessions.pop(session_id)
        duration_ms = int((time.time() - session_data["start_time"]) * 1000)

        properties = {
            "correlation_id": session_data["correlation_id"],
            "agent_id": session_data["agent_id"],
            "session_id": session_id,
            "duration_ms": duration_ms,
            "status": status,
            "artifacts_created": artifacts_created,
            "cortex_operations": cortex_operations,
            "tool_calls_count": tool_calls_count,
            "handoff_required": handoff_required,
            "delegation_count": delegation_count
        }

        self._emit_event("Agent:TaskCompleted", properties)

        # Record session summary
        session_summary = {
            "session_id": session_id,
            "agent_id": session_data["agent_id"],
            "duration_ms": duration_ms,
            "status": status,
            "task_type": session_data["task_type"],
            "priority": session_data["priority"],
            "completed_at": datetime.now(timezone.utc).isoformat()
        }

        with open(self.sessions_file, "a") as f:
            f.write(json.dumps(session_summary) + "\n")

    def task_failed(self, session_id: str, failure_reason: str,
                   error_category: str = "internal_error", partial_completion: bool = False,
                   reflexion_generated: bool = False) -> None:
        """Record task failure event"""
        if session_id not in self.active_sessions:
            return

        session_data = self.active_sessions.pop(session_id)
        duration_ms = int((time.time() - session_data["start_time"]) * 1000)

        properties = {
            "correlation_id": session_data["correlation_id"],
            "agent_id": session_data["agent_id"],
            "session_id": session_id,
            "duration_ms": duration_ms,
            "failure_reason": failure_reason,
            "error_category": error_category,
            "partial_completion": partial_completion,
            "reflexion_generated": reflexion_generated
        }

        self._emit_event("Agent:TaskFailed", properties)

    def delegation_started(self, delegating_agent: str, target_agent: str,
                          delegation_reason: str = "scope_boundary", priority: str = "P2") -> str:
        """Record delegation start event"""
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

    def delegation_completed(self, correlation_id: str, delegating_agent: str,
                           target_agent: str, total_duration_ms: int,
                           handoff_successful: bool = True, quality_score: Optional[float] = None) -> None:
        """Record delegation completion event"""
        properties = {
            "correlation_id": correlation_id,
            "delegating_agent": delegating_agent,
            "target_agent": target_agent,
            "total_duration_ms": total_duration_ms,
            "handoff_successful": handoff_successful
        }

        if quality_score is not None:
            properties["quality_score"] = quality_score

        self._emit_event("Agent:DelegationCompleted", properties)

    def memory_operation(self, agent_id: str, operation: str, memory_type: str,
                        operation_duration_ms: int, correlation_id: Optional[str] = None) -> None:
        """Record Cortex memory operation"""
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

    def tool_execution(self, agent_id: str, tool_name: str, execution_duration_ms: int,
                      success: bool = True, retries: int = 0, correlation_id: Optional[str] = None) -> None:
        """Record tool execution metrics"""
        if not correlation_id:
            correlation_id = self._generate_correlation_id()

        properties = {
            "correlation_id": correlation_id,
            "agent_id": agent_id,
            "tool_name": tool_name,
            "execution_duration_ms": execution_duration_ms,
            "success": success
        }

        if retries > 0:
            properties["retries"] = retries

        self._emit_event("Agent:ToolExecution", properties)

    def reflexion_generated(self, agent_id: str, failure_category: str,
                          contraindication_stored: bool = True, correlation_id: Optional[str] = None) -> None:
        """Record reflexion generation event"""
        if not correlation_id:
            correlation_id = self._generate_correlation_id()

        properties = {
            "correlation_id": correlation_id,
            "agent_id": agent_id,
            "failure_category": failure_category,
            "contraindication_stored": contraindication_stored
        }

        self._emit_event("Agent:ReflexionGenerated", properties)

    def conflict_detected(self, agents: list, conflict_type: str, resolution_method: str,
                         correlation_id: Optional[str] = None) -> None:
        """Record conflict detection event"""
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
        """Clean up stale sessions that have exceeded timeout"""
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