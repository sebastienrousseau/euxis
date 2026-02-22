# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

#!/usr/bin/env python3
"""Test script for the agent performance metrics system.

Generates sample data and validates functionality.
"""

import importlib
import secrets
import sys
import time
from collections.abc import Iterable
from pathlib import Path
from typing import TextIO

# Add local modules to path
metrics_src = Path(__file__).parent.parent / "src"
sys.path.insert(0, str(metrics_src))

performance_analyzer = importlib.import_module("metrics.aggregators.performance_analyzer")
PerformanceAnalyzer = performance_analyzer.PerformanceAnalyzer
performance_collector = importlib.import_module("metrics.collectors.performance_collector")
PerformanceMetricsCollector = performance_collector.PerformanceMetricsCollector

def _out(*values: object, sep: str = " ", end: str = "\n", stream: TextIO = sys.stdout) -> None:
    """Write output without using print()."""
    stream.write(sep.join(map(str, values)) + end)


def _choice[T](options: Iterable[T]) -> T:
    """Choose a random item using a crypto-safe source."""
    options_list = list(options)
    if not options_list:
        msg = "No options provided."
        raise ValueError(msg)
    return secrets.choice(options_list)


def _randint(low: int, high: int) -> int:
    """Return a random int in [low, high] using a crypto-safe source."""
    if low > high:
        msg = "low must be <= high."
        raise ValueError(msg)
    return low + secrets.randbelow(high - low + 1)


def _weighted_choice[T](options: list[T], weights: list[int]) -> T:
    """Choose a weighted option using a crypto-safe source."""
    if len(options) != len(weights):
        msg = "Options and weights must match."
        raise ValueError(msg)
    total = sum(weights)
    if total <= 0:
        msg = "Total weight must be positive."
        raise ValueError(msg)
    roll = secrets.randbelow(total)
    acc = 0
    for option, weight in zip(options, weights, strict=True):
        acc += weight
        if roll < acc:
            return option
    return options[-1]


def generate_sample_data() -> None:
    """Generate sample metrics data for testing."""
    collector = PerformanceMetricsCollector()

    # List of test agents
    agents = ["architect", "reviewer", "bug-fixer", "unit-tester", "edge-hunter"]
    task_types = ["user_request", "delegation", "orchestrated"]
    priorities = ["P0", "P1", "P2", "P3"]
    tools = ["Read", "Write", "Edit", "Bash", "Grep"]

    _out("🧪 Generating sample metrics data...")

    # Generate 50 sample sessions
    for i in range(50):
        agent = _choice(agents)
        session_id = f"test-session-{i:03d}"
        task_type = _choice(task_types)
        priority = _choice(priorities)

        # Start task
        correlation_id = collector.task_started(
            agent_id=agent,
            session_id=session_id,
            task_type=task_type,
            priority=priority
        )

        # Simulate some tool executions
        for _ in range(_randint(1, 8)):
            tool = _choice(tools)
            duration = _randint(50, 2000)
            success = _weighted_choice([True, False], [3, 1])  # 75% success rate

            collector.tool_execution(
                agent_id=agent,
                tool_name=tool,
                execution_duration_ms=duration,
                success=success,
                correlation_id=correlation_id
            )

        # Simulate some memory operations
        if _choice([True, False]):
            operation = _choice(["remember", "recall"])
            memory_type = _choice(["episodic", "semantic", "procedural"])
            duration = _randint(10, 200)

            collector.memory_operation(
                agent_id=agent,
                operation=operation,
                memory_type=memory_type,
                operation_duration_ms=duration,
                correlation_id=correlation_id
            )

        # Complete task (with some failures)
        status = _weighted_choice(["SUCCESS", "WARNING", "FAILURE"], [80, 15, 5])

        if status == "FAILURE":
            collector.task_failed(
                session_id=session_id,
                failure_reason="Test failure",
                error_category="timeout",
                reflexion_generated=True
            )
        else:
            collector.task_completed(
                session_id=session_id,
                status=status,
                artifacts_created=_randint(0, 5),
                cortex_operations=_randint(0, 3),
                tool_calls_count=_randint(1, 8),
                handoff_required=_choice([True, False])
            )

        # Brief delay to simulate realistic timing
        time.sleep(0.01)

    _out(f"✅ Generated sample data for {len(agents)} agents with 50 sessions")

def test_delegation_tracking() -> None:
    """Test delegation tracking."""
    collector = PerformanceMetricsCollector()

    _out("\n🔄 Testing delegation tracking...")

    # Simulate delegation chain: architect -> reviewer -> edge-hunter
    delegation_id = collector.delegation_started(
        delegating_agent="architect",
        target_agent="reviewer",
        delegation_reason="scope_boundary",
        priority="P1"
    )

    # Simulate delegation work
    time.sleep(0.1)

    collector.delegation_completed(
        correlation_id=delegation_id,
        delegating_agent="architect",
        target_agent="reviewer",
        total_duration_ms=2500,
        handoff_successful=True,
        quality_score=0.92
    )

    _out("✅ Delegation tracking tested")

def test_conflict_detection() -> None:
    """Test conflict detection."""
    collector = PerformanceMetricsCollector()

    _out("\n⚠️  Testing conflict detection...")

    collector.conflict_detected(
        agents=["architect", "reviewer"],
        conflict_type="recommendation",
        resolution_method="evidence"
    )

    _out("✅ Conflict detection tested")

def test_analysis() -> None:
    """Test metrics analysis."""
    analyzer = PerformanceAnalyzer()

    _out("\n📊 Testing metrics analysis...")

    # Test agent performance analysis
    agent_perf = analyzer.analyze_agent_performance(hours_back=1)
    _out(f"✅ Analyzed performance for {len(agent_perf)} agents")

    # Test delegation analysis
    delegation_patterns = analyzer.analyze_delegation_patterns(hours_back=1)
    _out(f"✅ Analyzed {len(delegation_patterns)} delegation patterns")

    # Test tool usage analysis
    tool_usage = analyzer.analyze_tool_usage_patterns(hours_back=1)
    _out(f"✅ Analyzed usage for {len(tool_usage)} tools")

    # Generate report
    report = analyzer.generate_performance_report(hours_back=1)
    _out("✅ Generated comprehensive report")

    # Show sample metrics
    fleet_metrics = report.get("fleet_metrics", {})
    if fleet_metrics:
        _out("\n📈 Sample Fleet Metrics:")
        _out(f"   Total Tasks: {fleet_metrics.get('total_tasks', 0)}")
        _out(f"   Success Rate: {fleet_metrics.get('fleet_success_rate', 0):.1%}")
        _out(f"   Active Agents: {fleet_metrics.get('active_agents', 0)}")

def test_rankings() -> None:
    """Test agent rankings."""
    analyzer = PerformanceAnalyzer()

    _out("\n🏆 Testing agent rankings...")

    rankings = analyzer.get_agent_rankings(hours_back=1, metric="success_rate")
    _out(f"✅ Generated rankings for {len(rankings)} agents")

    if rankings:
        _out(f"   Top performer: {rankings[0][0]} ({rankings[0][1]:.1%})")

def main() -> None:
    """Run all tests."""
    _out("🚀 Starting Agent Performance Metrics Test Suite")
    _out("=" * 60)

    # Generate test data
    generate_sample_data()

    # Test specific features
    test_delegation_tracking()
    test_conflict_detection()
    test_analysis()
    test_rankings()

    _out("\n" + "=" * 60)
    _out("✅ All tests completed successfully!")
    _out("\nMetrics files created:")
    _out("   📄 Events: $EUXIS_HOME/metrics/events.jsonl")
    _out("   📄 Sessions: $EUXIS_HOME/metrics/sessions.jsonl")
    _out("\nTry running:")
    _out("   python3 $EUXIS_HOME/metrics/metrics_cli.py report")

if __name__ == "__main__":
    main()
