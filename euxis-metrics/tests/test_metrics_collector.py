# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Extended unit tests for euxis-metrics modules with low coverage.

Covers:
- metrics/__init__.py: get_performance_collector, get_fast_collector
- metrics/collectors/performance_collector.py: PerformanceMetricsCollector
- metrics/collectors/fast_collector.py: FastMetricsCollector, FastMetricsBuffer
- metrics/aggregators/performance_analyzer.py: PerformanceAnalyzer
- metrics/verification/validation_pipeline.py: ValidationPipeline
- metrics/verification/evidence_framework.py: EvidenceFramework
"""

from __future__ import annotations

import asyncio
import json
import os
import time
from datetime import UTC, datetime, timedelta
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

# ============================================================================
# metrics/__init__.py
# ============================================================================


class TestMetricsInit:
    """Tests for lazy imports in metrics/__init__.py."""

    def test_get_performance_collector(self):
        """get_performance_collector should return PerformanceMetricsCollector."""
        import metrics
        metrics._perf_collector = None  # reset
        collector = metrics.get_performance_collector()
        assert collector is not None
        # Second call should return cached
        assert metrics.get_performance_collector() is collector

    def test_get_fast_collector(self):
        """get_fast_collector should return FastMetricsCollector."""
        import metrics
        metrics._fast_collector = None  # reset
        collector = metrics.get_fast_collector()
        assert collector is not None
        # Second call should return cached
        assert metrics.get_fast_collector() is collector

    def test_version(self):
        """Module version should be set."""
        import metrics
        assert metrics.__version__ == "v0.0.2"


# ============================================================================
# PerformanceMetricsCollector
# ============================================================================

class TestPerformanceMetricsCollector:
    """Tests for PerformanceMetricsCollector."""

    def test_init_creates_metrics_dir(self, tmp_path):
        """__init__ should create the metrics directory."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        metrics_dir = tmp_path / "metrics"
        collector = PerformanceMetricsCollector(str(metrics_dir))
        assert metrics_dir.exists()
        assert collector.events_file == metrics_dir / "events.jsonl"
        assert collector.sessions_file == metrics_dir / "sessions.jsonl"

    def test_load_schema_valid_file(self, tmp_path, monkeypatch):
        """_load_schema should load JSON when schema file exists."""
        from metrics.collectors import performance_collector
        schema_dir = tmp_path / "schemas"
        schema_dir.mkdir(parents=True, exist_ok=True)
        schema_file = schema_dir / "agent-performance.json"
        with schema_file.open("w") as f:
            f.write('{"type": "object"}')

        monkeypatch.setattr(performance_collector, "__file__", str(tmp_path / "dummy.py"))
        c = performance_collector.PerformanceMetricsCollector(metrics_dir=str(tmp_path / "metrics"))
        assert c.schema == {"type": "object"}

    def test_generate_correlation_id(self, tmp_path):
        """_generate_correlation_id should return a unique string."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        id1 = collector._generate_correlation_id()
        id2 = collector._generate_correlation_id()
        assert id1.startswith("metrics-")
        assert id1 != id2

    def test_emit_event_writes_jsonl(self, tmp_path):
        """_emit_event should write a JSON line to events file."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector._emit_event("TestEvent", {"key": "value"})

        with collector.events_file.open() as f:
            line = f.readline()
            event = json.loads(line)

        assert event["event_type"] == "TestEvent"
        assert event["properties"]["key"] == "value"
        assert "timestamp" in event

    def test_task_started_with_none_context(self, tmp_path):
        """task_started with None context should create default TaskStartContext."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        cid = collector.task_started("architect", "s1")
        assert cid.startswith("metrics-")
        assert "s1" in collector.active_sessions

    def test_task_started_with_context_object(self, tmp_path):
        """task_started with TaskStartContext object should use it directly."""
        from metrics.collectors.performance_collector import (
            PerformanceMetricsCollector,
            TaskStartContext,
        )
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        ctx = TaskStartContext(task_type="delegation", priority="P0",
                              task_complexity="high", parent_session="parent-1")
        cid = collector.task_started("architect", "s1", context=ctx)
        assert cid.startswith("metrics-")

    def test_task_started_with_context_object_extra_args_raises(self, tmp_path):
        """task_started with TaskStartContext + extra args should raise TypeError."""
        from metrics.collectors.performance_collector import (
            PerformanceMetricsCollector,
            TaskStartContext,
        )
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        ctx = TaskStartContext()
        with pytest.raises(TypeError, match="Unexpected"):
            collector.task_started("architect", "s1", context=ctx, extra="bad")

    def test_task_started_with_string_context(self, tmp_path):
        """task_started with string context should treat as task_type."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        cid = collector.task_started("architect", "s1", "delegation", "P0", "high", "parent-1")
        assert cid.startswith("metrics-")

    def test_task_started_string_too_many_args_raises(self, tmp_path):
        """task_started with string + too many positional args should raise TypeError."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        with pytest.raises(TypeError, match="Too many"):
            collector.task_started("architect", "s1", "type", "P0", "high", "parent", "extra")

    def test_task_started_invalid_context_type_raises(self, tmp_path):
        """task_started with invalid context type should raise TypeError."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        with pytest.raises(TypeError, match="context must be"):
            collector.task_started("architect", "s1", context=12345)

    def test_task_completed_untracked_session(self, tmp_path):
        """task_completed for untracked session should be a no-op."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        # Should not raise
        collector.task_completed("nonexistent")

    def test_task_completed_with_none_context(self, tmp_path):
        """task_completed with None context should create default TaskCompletionContext."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.task_started("architect", "s1")
        collector.task_completed("s1")
        assert "s1" not in collector.active_sessions

    def test_task_completed_with_context_object(self, tmp_path):
        """task_completed with TaskCompletionContext should use it directly."""
        from metrics.collectors.performance_collector import (
            PerformanceMetricsCollector,
            TaskCompletionContext,
        )
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.task_started("architect", "s1")
        ctx = TaskCompletionContext(status="SUCCESS", artifacts_created=3)
        collector.task_completed("s1", context=ctx)
        assert "s1" not in collector.active_sessions

    def test_task_completed_context_with_extra_args_raises(self, tmp_path):
        """task_completed with context object + extra args should raise."""
        from metrics.collectors.performance_collector import (
            PerformanceMetricsCollector,
            TaskCompletionContext,
        )
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.task_started("architect", "s1")
        ctx = TaskCompletionContext()
        with pytest.raises(TypeError, match="Unexpected"):
            collector.task_completed("s1", context=ctx, extra="bad")

    def test_task_completed_with_string_context(self, tmp_path):
        """task_completed with string context should treat as status."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.task_started("architect", "s1")
        collector.task_completed("s1", "SUCCESS", 2, 1, 5, False, 0)

    def test_task_completed_string_too_many_args_raises(self, tmp_path):
        """task_completed string + too many positional args should raise."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.task_started("architect", "s1")
        with pytest.raises(TypeError, match="Too many"):
            collector.task_completed("s1", "SUCCESS", 0, 0, 0, False, 0, "extra")

    def test_task_completed_invalid_context_type_raises(self, tmp_path):
        """task_completed with invalid context type should raise."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.task_started("architect", "s1")
        with pytest.raises(TypeError, match="context must be"):
            collector.task_completed("s1", context=12345)

    def test_task_completed_writes_session(self, tmp_path):
        """task_completed should write session summary to sessions file."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.task_started("architect", "s1")
        collector.task_completed("s1")

        with collector.sessions_file.open() as f:
            session = json.loads(f.readline())
        assert session["session_id"] == "s1"
        assert session["agent_id"] == "architect"

    def test_task_failed_untracked_session(self, tmp_path):
        """task_failed for untracked session should be a no-op."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.task_failed("nonexistent")

    def test_task_failed_with_none_context(self, tmp_path):
        """task_failed with kwargs should create TaskFailureContext."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.task_started("architect", "s1")
        collector.task_failed("s1", failure_reason="timeout")
        assert "s1" not in collector.active_sessions

    def test_task_failed_with_context_object(self, tmp_path):
        """task_failed with TaskFailureContext object should use it directly."""
        from metrics.collectors.performance_collector import (
            PerformanceMetricsCollector,
            TaskFailureContext,
        )
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.task_started("architect", "s1")
        ctx = TaskFailureContext(failure_reason="error", error_category="timeout",
                                partial_completion=True, reflexion_generated=True)
        collector.task_failed("s1", context=ctx)

    def test_task_failed_context_extra_args_raises(self, tmp_path):
        """task_failed with context object + extra args should raise."""
        from metrics.collectors.performance_collector import (
            PerformanceMetricsCollector,
            TaskFailureContext,
        )
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.task_started("architect", "s1")
        ctx = TaskFailureContext(failure_reason="e")
        with pytest.raises(TypeError, match="Unexpected"):
            collector.task_failed("s1", context=ctx, extra="bad")

    def test_task_failed_with_string_context(self, tmp_path):
        """task_failed with string context should treat as failure_reason."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.task_started("architect", "s1")
        collector.task_failed("s1", "timeout error", "timeout", True, True)

    def test_task_failed_string_too_many_args_raises(self, tmp_path):
        """task_failed string + too many positional args should raise."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.task_started("architect", "s1")
        with pytest.raises(TypeError, match="Too many"):
            collector.task_failed("s1", "reason", "cat", True, True, "extra")

    def test_task_failed_invalid_context_type_raises(self, tmp_path):
        """task_failed with invalid context type should raise."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.task_started("architect", "s1")
        with pytest.raises(TypeError, match="context must be"):
            collector.task_failed("s1", context=12345)

    def test_delegation_started(self, tmp_path):
        """delegation_started should emit event and return correlation_id."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        cid = collector.delegation_started("architect", "reviewer")
        assert cid.startswith("metrics-")

    def test_delegation_completed_with_none_context(self, tmp_path):
        """delegation_completed with kwargs should create DelegationContext."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.delegation_completed(
            "corr-1", "architect",
            target_agent="reviewer", total_duration_ms=1000,
            handoff_successful=True, quality_score=0.9,
        )

    def test_delegation_completed_with_context_object(self, tmp_path):
        """delegation_completed with DelegationContext should use it directly."""
        from metrics.collectors.performance_collector import (
            DelegationContext,
            PerformanceMetricsCollector,
        )
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        ctx = DelegationContext(target_agent="reviewer", total_duration_ms=1000,
                               quality_score=0.95)
        collector.delegation_completed("corr-1", "architect", context=ctx)

    def test_delegation_completed_context_extra_args_raises(self, tmp_path):
        """delegation_completed with context + extra should raise."""
        from metrics.collectors.performance_collector import (
            DelegationContext,
            PerformanceMetricsCollector,
        )
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        ctx = DelegationContext(target_agent="r", total_duration_ms=100)
        with pytest.raises(TypeError, match="Unexpected"):
            collector.delegation_completed("c", "a", context=ctx, extra="bad")

    def test_delegation_completed_with_string_context(self, tmp_path):
        """delegation_completed with string should treat as target_agent."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.delegation_completed("corr-1", "architect", "reviewer", 1000, True, 0.9)

    def test_delegation_completed_string_too_many_args_raises(self, tmp_path):
        """delegation_completed with too many positional args should raise."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        with pytest.raises(TypeError, match="Too many"):
            collector.delegation_completed("c", "a", "r", 1000, True, 0.9, "extra")

    def test_delegation_completed_invalid_context_type_raises(self, tmp_path):
        """delegation_completed with invalid context should raise."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        with pytest.raises(TypeError, match="context must be"):
            collector.delegation_completed("c", "a", context=12345)

    def test_delegation_completed_no_quality_score(self, tmp_path):
        """delegation_completed without quality_score should omit it from properties."""
        from metrics.collectors.performance_collector import (
            DelegationContext,
            PerformanceMetricsCollector,
        )
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        ctx = DelegationContext(target_agent="reviewer", total_duration_ms=500)
        collector.delegation_completed("c", "architect", context=ctx)

        with collector.events_file.open() as f:
            event = json.loads(f.readline())
        assert "quality_score" not in event["properties"]

    def test_memory_operation(self, tmp_path):
        """memory_operation should emit event."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.memory_operation("architect", "remember", "episodic", 50)

    def test_memory_operation_with_correlation_id(self, tmp_path):
        """memory_operation with provided correlation_id should use it."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.memory_operation("architect", "recall", "semantic", 30, "custom-id")

        with collector.events_file.open() as f:
            event = json.loads(f.readline())
        assert event["properties"]["correlation_id"] == "custom-id"

    def test_tool_execution_with_none_context(self, tmp_path):
        """tool_execution with kwargs should create ToolExecutionContext."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.tool_execution("architect", "Read", execution_duration_ms=100)

    def test_tool_execution_with_context_object(self, tmp_path):
        """tool_execution with ToolExecutionContext should use it."""
        from metrics.collectors.performance_collector import (
            PerformanceMetricsCollector,
            ToolExecutionContext,
        )
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        ctx = ToolExecutionContext(execution_duration_ms=200, success=True, retries=2,
                                  correlation_id="c1")
        collector.tool_execution("architect", "Write", context=ctx)

        with collector.events_file.open() as f:
            event = json.loads(f.readline())
        assert event["properties"]["retries"] == 2

    def test_tool_execution_context_extra_args_raises(self, tmp_path):
        """tool_execution with context + extra should raise."""
        from metrics.collectors.performance_collector import (
            PerformanceMetricsCollector,
            ToolExecutionContext,
        )
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        ctx = ToolExecutionContext(execution_duration_ms=100)
        with pytest.raises(TypeError, match="Unexpected"):
            collector.tool_execution("a", "t", context=ctx, extra="bad")

    def test_tool_execution_with_int_context(self, tmp_path):
        """tool_execution with integer context should treat as duration."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.tool_execution("architect", "Read", 150, True, 1, "corr-1")

    def test_tool_execution_invalid_args(self, tmp_path):
        from metrics.collectors.performance_collector import PerformanceMetricsCollector, ToolExecutionContext
        c = PerformanceMetricsCollector(metrics_dir=str(tmp_path / "metrics"))
        # Extra args when passing context
        with pytest.raises(TypeError, match="Unexpected extra arguments"):
            c.tool_execution("agent", "tool", ToolExecutionContext(10), "extra")
        # Too many positional args for int
        with pytest.raises(TypeError, match="Too many positional arguments"):
            c.tool_execution("agent", "tool", 10, True, 0, "corr", "extra")
        # Invalid type
        with pytest.raises(TypeError, match="context must be ToolExecutionContext"):
            c.tool_execution("agent", "tool", "not_int")

    def test_task_started_completed_string_args(self, tmp_path):
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        from pathlib import Path
        c = PerformanceMetricsCollector(metrics_dir=str(tmp_path / "metrics"))
        
        # Test string arg path
        corr_id = c.task_started("agent1", "sess1", "my_task", "P1", "high", "parent")
        assert "sess1" in c.active_sessions
        assert Path(c.events_file).exists()
        
        c.task_completed("sess1", "SUCCESS", 2, 1, 3, True, 1)
        assert "sess1" not in c.active_sessions
        assert Path(c.sessions_file).exists()

    def test_task_failed_string_args(self, tmp_path):
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        c = PerformanceMetricsCollector(metrics_dir=str(tmp_path / "metrics"))
        
        c.task_started("agent1", "sess2")
        c.task_failed("sess2", "boom", "fatal", True, True)
        assert "sess2" not in c.active_sessions

    def test_delegation_string_args(self, tmp_path):
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        c = PerformanceMetricsCollector(metrics_dir=str(tmp_path / "metrics"))
        
        corr_id = c.delegation_started("agent1", "agent2")
        c.delegation_completed(corr_id, "agent1", "agent2", 1500, False, 0.5)

    def test_tool_execution_int_args(self, tmp_path):
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        c = PerformanceMetricsCollector(metrics_dir=str(tmp_path / "metrics"))
        
        c.tool_execution("agent1", "tool_a", 500, False, 2, "corr1")

    def test_other_events(self, tmp_path):
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        import time
        from pathlib import Path
        c = PerformanceMetricsCollector(metrics_dir=str(tmp_path / "metrics"))
        
        c.memory_operation("agent1", "read", "short", 10)
        c.reflexion_generated("agent1", "logic")
        c.conflict_detected(["agent1", "agent2"], "resource", "arbitration")
        
        # Cleanup
        c.task_started("agent99", "sess99")
        c.active_sessions["sess99"]["start_time"] = time.time() - 4000
        count = c.cleanup_stale_sessions(3600)
        assert count == 1

    def test_tool_execution_no_retries_omits_field(self, tmp_path):
        """tool_execution with 0 retries should not include retries in properties."""
        from metrics.collectors.performance_collector import (
            PerformanceMetricsCollector,
            ToolExecutionContext,
        )
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        ctx = ToolExecutionContext(execution_duration_ms=100, retries=0)
        collector.tool_execution("architect", "Read", context=ctx)

        with collector.events_file.open() as f:
            event = json.loads(f.readline())
        assert "retries" not in event["properties"]

    def test_reflexion_generated(self, tmp_path):
        """reflexion_generated should emit event."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.reflexion_generated("architect", "timeout")

    def test_reflexion_generated_with_correlation_id(self, tmp_path):
        """reflexion_generated with correlation_id should use it."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.reflexion_generated("architect", "timeout", True, "corr-1")

        with collector.events_file.open() as f:
            event = json.loads(f.readline())
        assert event["properties"]["correlation_id"] == "corr-1"

    def test_conflict_detected(self, tmp_path):
        """conflict_detected should emit event."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.conflict_detected(["architect", "reviewer"], "recommendation", "evidence")

    def test_conflict_detected_with_correlation_id(self, tmp_path):
        """conflict_detected with correlation_id should use it."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.conflict_detected(
            ["a", "b"], "c", "d", correlation_id="corr-1"
        )

    def test_cleanup_stale_sessions(self, tmp_path):
        """cleanup_stale_sessions should remove old sessions and return count."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))

        # Start a session that will be stale
        collector.task_started("architect", "s-old")
        # Manually backdate the session
        collector.active_sessions["s-old"]["start_time"] = time.time() - 7200

        # Start a session that will be fresh
        collector.task_started("reviewer", "s-fresh")

        cleaned = collector.cleanup_stale_sessions(timeout_seconds=3600)
        assert cleaned == 1
        assert "s-old" not in collector.active_sessions
        assert "s-fresh" in collector.active_sessions

    def test_cleanup_stale_sessions_none(self, tmp_path):
        """cleanup_stale_sessions with no stale sessions should return 0."""
        from metrics.collectors.performance_collector import PerformanceMetricsCollector
        collector = PerformanceMetricsCollector(str(tmp_path / "metrics"))
        collector.task_started("architect", "s1")
        assert collector.cleanup_stale_sessions() == 0


# ============================================================================
# FastMetricsCollector & FastMetricsBuffer
# ============================================================================

class TestFastMetricsBuffer:
    """Tests for FastMetricsBuffer."""

    def test_append_and_drain(self):
        """append should add events, drain should return and clear."""
        from metrics.collectors.fast_collector import FastMetricsBuffer, MetricEvent
        buf = FastMetricsBuffer(max_size=10)
        event = MetricEvent(event_type="test", timestamp=time.time(), properties={})
        buf.append(event)
        assert len(buf) == 1

        events = buf.drain()
        assert len(events) == 1
        assert len(buf) == 0

    def test_buffer_max_size(self):
        """Buffer should respect max_size (deque with maxlen)."""
        from metrics.collectors.fast_collector import FastMetricsBuffer, MetricEvent
        buf = FastMetricsBuffer(max_size=3)
        for i in range(5):
            buf.append(MetricEvent(f"event-{i}", time.time(), {}))

        assert len(buf) == 3
        events = buf.drain()
        assert events[0].event_type == "event-2"

    def test_drain_empty(self):
        """Draining empty buffer should return empty list."""
        from metrics.collectors.fast_collector import FastMetricsBuffer
        buf = FastMetricsBuffer()
        assert buf.drain() == []


class TestFastMetricsCollector:
    """Tests for FastMetricsCollector."""

    def test_init(self, tmp_path):
        """__init__ should create metrics directory."""
        from metrics.collectors.fast_collector import FastMetricsCollector
        metrics_dir = tmp_path / "fast-metrics"
        collector = FastMetricsCollector(str(metrics_dir))
        assert metrics_dir.exists()

    def test_record(self, tmp_path):
        """record should add event to buffer."""
        from metrics.collectors.fast_collector import FastMetricsCollector
        collector = FastMetricsCollector(str(tmp_path / "metrics"))
        collector.record("test_event", {"key": "val"})
        assert len(collector._buffer) == 1

    def test_record_no_properties(self, tmp_path):
        """record with no properties should use empty dict."""
        from metrics.collectors.fast_collector import FastMetricsCollector
        collector = FastMetricsCollector(str(tmp_path / "metrics"))
        collector.record("test_event")
        events = collector._buffer.drain()
        assert events[0].properties == {}

    def test_record_metric(self, tmp_path):
        """record_metric should record a metric event."""
        from metrics.collectors.fast_collector import FastMetricsCollector
        collector = FastMetricsCollector(str(tmp_path / "metrics"))
        collector.record_metric("cpu_usage", 75.5, {"host": "a"})
        events = collector._buffer.drain()
        assert events[0].event_type == "metric"
        assert events[0].properties["name"] == "cpu_usage"
        assert events[0].properties["value"] == 75.5

    def test_record_timing(self, tmp_path):
        """record_timing should record a timing event."""
        from metrics.collectors.fast_collector import FastMetricsCollector
        collector = FastMetricsCollector(str(tmp_path / "metrics"))
        collector.record_timing("api_call", 5.2)
        events = collector._buffer.drain()
        assert events[0].event_type == "timing"
        assert events[0].properties["duration_ms"] == 5.2

    def test_record_counter(self, tmp_path):
        """record_counter should record a counter event."""
        from metrics.collectors.fast_collector import FastMetricsCollector
        collector = FastMetricsCollector(str(tmp_path / "metrics"))
        collector.record_counter("requests", 5)
        events = collector._buffer.drain()
        assert events[0].event_type == "counter"
        assert events[0].properties["delta"] == 5

    @pytest.mark.asyncio
    async def test_flush_empty(self, tmp_path):
        """flush with no events should return 0."""
        from metrics.collectors.fast_collector import FastMetricsCollector
        collector = FastMetricsCollector(str(tmp_path / "metrics"))
        count = await collector.flush()
        assert count == 0

    @pytest.mark.asyncio
    async def test_flush_writes_events(self, tmp_path):
        """flush should write buffered events to disk."""
        from metrics.collectors.fast_collector import FastMetricsCollector
        collector = FastMetricsCollector(str(tmp_path / "metrics"))
        collector.record("e1", {"k": "v1"})
        collector.record("e2", {"k": "v2"})
        count = await collector.flush()
        assert count == 2
        assert collector._events_file.exists()

    def test_sync_flush_empty(self, tmp_path):
        """sync_flush with no events should return 0."""
        from metrics.collectors.fast_collector import FastMetricsCollector
        collector = FastMetricsCollector(str(tmp_path / "metrics"))
        assert collector.sync_flush() == 0

    def test_sync_flush_writes(self, tmp_path):
        """sync_flush should write buffered events."""
        from metrics.collectors.fast_collector import FastMetricsCollector
        collector = FastMetricsCollector(str(tmp_path / "metrics"))
        collector.record("e1", {"k": "v1"})
        count = collector.sync_flush()
        assert count == 1
        assert collector._events_file.exists()

    @pytest.mark.asyncio
    async def test_shutdown(self, tmp_path):
        """shutdown should cancel flush task and do final flush."""
        from metrics.collectors.fast_collector import FastMetricsCollector
        collector = FastMetricsCollector(str(tmp_path / "metrics"))
        collector.record("e1", {"k": "v1"})
        await collector.start_background_flush()
        assert collector._flush_task is not None
        await collector.shutdown()
        assert collector._shutdown is True
        # Final flush should have written the event
        assert collector._events_file.exists()

    @pytest.mark.asyncio
    async def test_shutdown_without_flush_task(self, tmp_path):
        """shutdown without started background flush should still work."""
        from metrics.collectors.fast_collector import FastMetricsCollector
        collector = FastMetricsCollector(str(tmp_path / "metrics"))
        collector.record("e1")
        await collector.shutdown()
        assert collector._shutdown is True

    @pytest.mark.asyncio
    async def test_msgpack_async_flush(self, tmp_path):
        import importlib
        import sys
        from unittest.mock import MagicMock, patch
        with patch.dict(sys.modules, {"msgpack": MagicMock()}):
            import msgpack
            msgpack.packb.return_value = b"mockdata"
            import metrics.collectors.fast_collector as fc
            importlib.reload(fc)
            fc.HAS_MSGPACK = True
            collector = fc.FastMetricsCollector(metrics_dir=str(tmp_path / "metrics"))
            collector._buffer.append(MagicMock(event_type="t", timestamp="t", properties={}))
            with patch.object(fc.FastMetricsCollector, "_write_file") as mock_write:
                await collector.flush()
                mock_write.assert_called_with(b"mockdata", "ab")
                
    @pytest.mark.asyncio
    async def test_shutdown_loop_exit(self, tmp_path):
        import metrics.collectors.fast_collector as fc
        import importlib
        importlib.reload(fc)
        import asyncio
        from unittest.mock import patch
        
        collector = fc.FastMetricsCollector(metrics_dir=str(tmp_path / "metrics"))
        
        # Patch sleep to not wait, so it hits the next loop iteration immediately
        with patch("asyncio.sleep", return_value=None):
            await collector.start_background_flush()
            # Set shutdown gracefully
            collector._shutdown = True
            # Let the loop exit fully
            await collector._flush_task
            
        assert collector._flush_task.done()

    @pytest.mark.asyncio
    async def test_start_background_flush_idempotent(self, tmp_path):
        """Calling start_background_flush twice should not create second task."""
        from metrics.collectors.fast_collector import FastMetricsCollector
        collector = FastMetricsCollector(str(tmp_path / "metrics"))
        await collector.start_background_flush()
        task1 = collector._flush_task
        await collector.start_background_flush()
        assert collector._flush_task is task1
        await collector.shutdown()

    def test_write_file_text(self, tmp_path):
        """_write_file should handle text data."""
        from metrics.collectors.fast_collector import FastMetricsCollector
        collector = FastMetricsCollector(str(tmp_path / "metrics"))
        collector._write_file("hello\n", "a")
        assert collector._events_file.read_text() == "hello\n"

    def test_write_file_binary(self, tmp_path):
        """_write_file should handle binary data."""
        from metrics.collectors.fast_collector import FastMetricsCollector
        collector = FastMetricsCollector(str(tmp_path / "metrics"))
        collector._write_file(b"\x00\x01", "a")
        assert collector._events_file.read_bytes() == b"\x00\x01"


class TestFastCollectorConvenienceFunctions:
    """Tests for module-level convenience functions."""

    @pytest.mark.asyncio
    async def test_flush_msgpack(self, tmp_path, monkeypatch):
        from metrics.collectors.fast_collector import FastMetricsCollector, HAS_MSGPACK
        import metrics.collectors.fast_collector as fc
        
        # Test the msgpack branch if available or mock it
        original_has_msgpack = fc.HAS_MSGPACK
        try:
            import msgpack
            # ensure it's True
            monkeypatch.setattr(fc, "HAS_MSGPACK", True)
            c = FastMetricsCollector(metrics_dir=str(tmp_path / "metrics"))
            c.record_metric("cpu", 80.0)
            await c.flush()
            assert Path(c._events_file).exists()
        except ImportError:
            # If msgpack isn't actually installed, we can't fully mock the packb cleanly without a lot of work.
            pass
        finally:
            monkeypatch.setattr(fc, "HAS_MSGPACK", original_has_msgpack)

    @pytest.mark.asyncio
    async def test_background_flush_loop(self, tmp_path, monkeypatch):
        from metrics.collectors.fast_collector import FastMetricsCollector
        c = FastMetricsCollector(metrics_dir=str(tmp_path / "metrics"))
        
        # Speed up flush interval for testing
        monkeypatch.setattr("metrics.collectors.fast_collector.FLUSH_INTERVAL", 0.01)
        
        await c.start_background_flush()
        c.record("test_event", {"a": 1})
        await asyncio.sleep(0.05)
        await c.shutdown()
        
        # Buffer should be drained
        assert len(c._buffer) == 0

    @pytest.mark.asyncio
    async def test_background_flush_loop_exception(self, tmp_path, monkeypatch):
        from metrics.collectors.fast_collector import FastMetricsCollector
        c = FastMetricsCollector(metrics_dir=str(tmp_path / "metrics"))
        monkeypatch.setattr("metrics.collectors.fast_collector.FLUSH_INTERVAL", 0.01)
        
        async def boom():
            raise RuntimeError("boom")
        
        monkeypatch.setattr(c, "flush", boom)
        await c.start_background_flush()
        c.record("test_event", {"a": 1})
        await asyncio.sleep(0.05)
        
        # During shutdown it will try a final flush, which we also mocked to boom.
        # We catch it here to prove the loop itself didn't crash the program.
        with pytest.raises(RuntimeError, match="boom"):
            await c.shutdown()

    def test_sync_flush_msgpack(self, tmp_path, monkeypatch):
        from metrics.collectors.fast_collector import FastMetricsCollector
        import metrics.collectors.fast_collector as fc
        
        original_has = fc.HAS_MSGPACK
        try:
            import msgpack
            monkeypatch.setattr(fc, "HAS_MSGPACK", True)
            c = FastMetricsCollector(metrics_dir=str(tmp_path / "metrics"))
            c.record_timing("load", 10.5)
            c.sync_flush()
            assert Path(c._events_file).exists()
        except ImportError:
            pass
        finally:
            monkeypatch.setattr(fc, "HAS_MSGPACK", original_has)

class TestFastCollectorEdgeCases:
    def test_msgpack_serialization(self, tmp_path, monkeypatch):
        import sys
        from unittest.mock import MagicMock, patch
        import importlib
        import asyncio
        from pathlib import Path

        # Mock msgpack module and ensure HAS_MSGPACK is True for this test
        with patch.dict(sys.modules, {"msgpack": MagicMock()}):
            import msgpack
            msgpack.packb.return_value = b"mockdata"
            
            import metrics.collectors.fast_collector as fc
            # Reload the module to ensure HAS_MSGPACK is re-evaluated with the mocked msgpack
            importlib.reload(fc)
            
            # Ensure HAS_MSGPACK is True after reload
            monkeypatch.setattr(fc, "HAS_MSGPACK", True)

            collector = fc.FastMetricsCollector(metrics_dir=str(tmp_path / "metrics"))
            collector._buffer.append(MagicMock(event_type="t", timestamp="t", properties={}))
            
            # This writes to buffer, now flush it synchronously
            with patch("builtins.open") as mock_open:
                collector.sync_flush()
                # Verify that open was called with binary write mode
                mock_open.assert_called_with(collector._events_file, 'ab')

    def test_record_function(self, tmp_path):
        """Module-level record should delegate to global collector."""
        from metrics.collectors import fast_collector as fc
        # Use the global fast_collector
        fc.record("test_event", {"k": "v"})
        # Just verify no exception

    def test_record_metric_function(self):
        """Module-level record_metric should work."""
        from metrics.collectors.fast_collector import record_metric
        record_metric("cpu", 50.0)

    def test_record_timing_function(self):
        """Module-level record_timing should work."""
        from metrics.collectors.fast_collector import record_timing
        record_timing("api", 1.5)

    def test_record_counter_function(self):
        """Module-level record_counter should work."""
        from metrics.collectors.fast_collector import record_counter
        record_counter("hits", 1)


# ============================================================================
# PerformanceAnalyzer
# ============================================================================

