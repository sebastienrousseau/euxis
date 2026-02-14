#!/usr/bin/env python3
"""
Test script for the agent performance metrics system
Generates sample data and validates functionality
"""

import sys
import time
import random
from pathlib import Path

# Add local modules to path
sys.path.insert(0, str(Path(__file__).parent))

from collectors.performance_collector import PerformanceMetricsCollector
from aggregators.performance_analyzer import PerformanceAnalyzer

def generate_sample_data():
    """Generate sample metrics data for testing"""
    collector = PerformanceMetricsCollector()

    # List of test agents
    agents = ["architect", "reviewer", "bug-fixer", "unit-tester", "edge-hunter"]
    task_types = ["user_request", "delegation", "orchestrated"]
    priorities = ["P0", "P1", "P2", "P3"]
    tools = ["Read", "Write", "Edit", "Bash", "Grep"]

    print("🧪 Generating sample metrics data...")

    # Generate 50 sample sessions
    for i in range(50):
        agent = random.choice(agents)
        session_id = f"test-session-{i:03d}"
        task_type = random.choice(task_types)
        priority = random.choice(priorities)

        # Start task
        correlation_id = collector.task_started(
            agent_id=agent,
            session_id=session_id,
            task_type=task_type,
            priority=priority
        )

        # Simulate some tool executions
        for _ in range(random.randint(1, 8)):
            tool = random.choice(tools)
            duration = random.randint(50, 2000)
            success = random.choice([True, True, True, False])  # 75% success rate

            collector.tool_execution(
                agent_id=agent,
                tool_name=tool,
                execution_duration_ms=duration,
                success=success,
                correlation_id=correlation_id
            )

        # Simulate some memory operations
        if random.choice([True, False]):
            operation = random.choice(["remember", "recall"])
            memory_type = random.choice(["episodic", "semantic", "procedural"])
            duration = random.randint(10, 200)

            collector.memory_operation(
                agent_id=agent,
                operation=operation,
                memory_type=memory_type,
                operation_duration_ms=duration,
                correlation_id=correlation_id
            )

        # Complete task (with some failures)
        status = random.choices(["SUCCESS", "WARNING", "FAILURE"], weights=[80, 15, 5])[0]

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
                artifacts_created=random.randint(0, 5),
                cortex_operations=random.randint(0, 3),
                tool_calls_count=random.randint(1, 8),
                handoff_required=random.choice([True, False])
            )

        # Brief delay to simulate realistic timing
        time.sleep(0.01)

    print(f"✅ Generated sample data for {len(agents)} agents with 50 sessions")

def test_delegation_tracking():
    """Test delegation tracking"""
    collector = PerformanceMetricsCollector()

    print("\n🔄 Testing delegation tracking...")

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

    print("✅ Delegation tracking tested")

def test_conflict_detection():
    """Test conflict detection"""
    collector = PerformanceMetricsCollector()

    print("\n⚠️  Testing conflict detection...")

    collector.conflict_detected(
        agents=["architect", "reviewer"],
        conflict_type="recommendation",
        resolution_method="evidence"
    )

    print("✅ Conflict detection tested")

def test_analysis():
    """Test metrics analysis"""
    analyzer = PerformanceAnalyzer()

    print("\n📊 Testing metrics analysis...")

    # Test agent performance analysis
    agent_perf = analyzer.analyze_agent_performance(hours_back=1)
    print(f"✅ Analyzed performance for {len(agent_perf)} agents")

    # Test delegation analysis
    delegation_patterns = analyzer.analyze_delegation_patterns(hours_back=1)
    print(f"✅ Analyzed {len(delegation_patterns)} delegation patterns")

    # Test tool usage analysis
    tool_usage = analyzer.analyze_tool_usage_patterns(hours_back=1)
    print(f"✅ Analyzed usage for {len(tool_usage)} tools")

    # Generate report
    report = analyzer.generate_performance_report(hours_back=1)
    print(f"✅ Generated comprehensive report")

    # Show sample metrics
    fleet_metrics = report.get("fleet_metrics", {})
    if fleet_metrics:
        print(f"\n📈 Sample Fleet Metrics:")
        print(f"   Total Tasks: {fleet_metrics.get('total_tasks', 0)}")
        print(f"   Success Rate: {fleet_metrics.get('fleet_success_rate', 0):.1%}")
        print(f"   Active Agents: {fleet_metrics.get('active_agents', 0)}")

def test_rankings():
    """Test agent rankings"""
    analyzer = PerformanceAnalyzer()

    print("\n🏆 Testing agent rankings...")

    rankings = analyzer.get_agent_rankings(hours_back=1, metric="success_rate")
    print(f"✅ Generated rankings for {len(rankings)} agents")

    if rankings:
        print(f"   Top performer: {rankings[0][0]} ({rankings[0][1]:.1%})")

def main():
    """Run all tests"""
    print("🚀 Starting Agent Performance Metrics Test Suite")
    print("=" * 60)

    try:
        # Generate test data
        generate_sample_data()

        # Test specific features
        test_delegation_tracking()
        test_conflict_detection()
        test_analysis()
        test_rankings()

        print("\n" + "=" * 60)
        print("✅ All tests completed successfully!")
        print("\nMetrics files created:")
        print(f"   📄 Events: /home/seb/.euxis/metrics/events.jsonl")
        print(f"   📄 Sessions: /home/seb/.euxis/metrics/sessions.jsonl")
        print("\nTry running:")
        print("   python3 /home/seb/.euxis/metrics/metrics_cli.py report")

    except Exception as e:
        print(f"\n❌ Test failed: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()