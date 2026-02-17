#!/usr/bin/env python3
"""Agent Performance Metrics Analyzer.

Aggregates and analyzes collected performance metrics.
"""

import json
import math
import statistics
from collections import Counter, defaultdict
from datetime import UTC, datetime, timedelta
from pathlib import Path
from typing import Any


class PerformanceAnalyzer:
    """Analyzes agent performance metrics and generates insights."""

    def __init__(self, metrics_dir: str = "/home/seb/.euxis/metrics") -> None:
        self.metrics_dir = Path(metrics_dir)
        self.events_file = self.metrics_dir / "events.jsonl"
        self.sessions_file = self.metrics_dir / "sessions.jsonl"
        self.reports_dir = self.metrics_dir / "reports"

        self.reports_dir.mkdir(exist_ok=True)

    def _load_events(self, hours_back: int = 24) -> list[dict[str, Any]]:
        """Load events from the last N hours."""
        if not self.events_file.exists():
            return []

        cutoff_time = datetime.now(UTC) - timedelta(hours=hours_back)
        events = []

        with self.events_file.open() as f:
            for line in f:
                try:
                    event = json.loads(line.strip())
                    event_time = datetime.fromisoformat(
                        event["timestamp"].replace("Z", "+00:00")
                    )
                    if event_time >= cutoff_time:
                        events.append(event)
                except (json.JSONDecodeError, KeyError, ValueError):
                    continue

        return events

    def _load_sessions(self, hours_back: int = 24) -> list[dict[str, Any]]:
        """Load completed sessions from the last N hours."""
        if not self.sessions_file.exists():
            return []

        cutoff_time = datetime.now(UTC) - timedelta(hours=hours_back)
        sessions = []

        with self.sessions_file.open() as f:
            for line in f:
                try:
                    session = json.loads(line.strip())
                    session_time = datetime.fromisoformat(
                        session["completed_at"].replace("Z", "+00:00")
                    )
                    if session_time >= cutoff_time:
                        sessions.append(session)
                except (json.JSONDecodeError, KeyError, ValueError):
                    continue

        return sessions

    def calculate_percentile(self, values: list[float], percentile: int) -> float:
        """Calculate percentile value from a list."""
        if not values:
            return 0.0
        sorted_values = sorted(values)
        index = (percentile / 100) * (len(sorted_values) - 1)
        lower = math.floor(index)
        upper = math.ceil(index)

        if lower == upper:
            return sorted_values[lower]

        weight = index - lower
        return sorted_values[lower] * (1 - weight) + sorted_values[upper] * weight

    def analyze_agent_performance(self, hours_back: int = 24) -> dict[str, dict[str, Any]]:
        """Analyze performance metrics by agent."""
        sessions = self._load_sessions(hours_back)
        if not sessions:
            return {}

        agent_metrics = defaultdict(lambda: {
            "total_tasks": 0,
            "successful_tasks": 0,
            "failed_tasks": 0,
            "warning_tasks": 0,
            "durations": [],
            "task_types": Counter(),
            "priorities": Counter()
        })

        # Aggregate raw data
        for session in sessions:
            agent_id = session["agent_id"]
            metrics = agent_metrics[agent_id]

            metrics["total_tasks"] += 1
            metrics["durations"].append(session["duration_ms"])
            metrics["task_types"][session["task_type"]] += 1
            metrics["priorities"][session["priority"]] += 1

            if session["status"] == "SUCCESS":
                metrics["successful_tasks"] += 1
            elif session["status"] == "FAILURE":
                metrics["failed_tasks"] += 1
            elif session["status"] == "WARNING":
                metrics["warning_tasks"] += 1

        # Calculate derived metrics
        result = {}
        for agent_id, metrics in agent_metrics.items():
            if metrics["total_tasks"] == 0:
                continue

            durations = metrics["durations"]
            result[agent_id] = {
                "total_tasks": metrics["total_tasks"],
                "success_rate": metrics["successful_tasks"] / metrics["total_tasks"],
                "failure_rate": metrics["failed_tasks"] / metrics["total_tasks"],
                "warning_rate": metrics["warning_tasks"] / metrics["total_tasks"],
                "avg_duration_ms": statistics.mean(durations),
                "median_duration_ms": statistics.median(durations),
                "p95_duration_ms": self.calculate_percentile(durations, 95),
                "p99_duration_ms": self.calculate_percentile(durations, 99),
                "min_duration_ms": min(durations),
                "max_duration_ms": max(durations),
                "task_type_distribution": dict(metrics["task_types"]),
                "priority_distribution": dict(metrics["priorities"])
            }

        return result

    def analyze_delegation_patterns(self, hours_back: int = 24) -> dict[str, Any]:
        """Analyze delegation patterns between agents."""
        events = self._load_events(hours_back)

        delegation_starts = defaultdict(list)
        delegation_completions = {}
        delegation_pairs = defaultdict(lambda: {
            "count": 0,
            "durations": [],
            "success_count": 0,
            "total_count": 0
        })

        for event in events:
            if event["event_type"] == "Agent:DelegationStarted":
                props = event["properties"]
                correlation_id = props["correlation_id"]
                delegation_starts[correlation_id].append({
                    "delegating_agent": props["delegating_agent"],
                    "target_agent": props["target_agent"],
                    "timestamp": event["timestamp"],
                    "priority": props.get("priority", "P2")
                })

            elif event["event_type"] == "Agent:DelegationCompleted":
                props = event["properties"]
                correlation_id = props["correlation_id"]
                delegation_completions[correlation_id] = {
                    "delegating_agent": props["delegating_agent"],
                    "target_agent": props["target_agent"],
                    "duration_ms": props["total_duration_ms"],
                    "handoff_successful": props["handoff_successful"],
                    "quality_score": props.get("quality_score")
                }

        # Analyze delegation pairs
        for correlation_id, completion in delegation_completions.items():
            if correlation_id in delegation_starts:
                delegating = completion["delegating_agent"]
                target = completion["target_agent"]
                pair_key = f"{delegating}->{target}"

                pair_data = delegation_pairs[pair_key]
                pair_data["count"] += 1
                pair_data["durations"].append(completion["duration_ms"])
                pair_data["total_count"] += 1

                if completion["handoff_successful"]:
                    pair_data["success_count"] += 1

        # Calculate metrics for each delegation pair
        result = {}
        for pair_key, data in delegation_pairs.items():
            if data["total_count"] == 0:
                continue

            durations = data["durations"]
            result[pair_key] = {
                "delegation_frequency": data["count"],
                "handoff_success_rate": data["success_count"] / data["total_count"],
                "avg_delegation_duration_ms": (
                    statistics.mean(durations) if durations else 0
                ),
                "median_delegation_duration_ms": (
                    statistics.median(durations) if durations else 0
                ),
                "p95_delegation_duration_ms": (
                    self.calculate_percentile(durations, 95) if durations else 0
                ),
            }

        return result

    def analyze_tool_usage_patterns(self, hours_back: int = 24) -> dict[str, Any]:
        """Analyze tool usage patterns across agents."""
        events = self._load_events(hours_back)

        tool_metrics = defaultdict(lambda: {
            "total_executions": 0,
            "successful_executions": 0,
            "durations": [],
            "agents": Counter(),
            "retry_counts": []
        })

        for event in events:
            if event["event_type"] == "Agent:ToolExecution":
                props = event["properties"]
                tool_name = props["tool_name"]
                agent_id = props["agent_id"]

                metrics = tool_metrics[tool_name]
                metrics["total_executions"] += 1
                metrics["durations"].append(props["execution_duration_ms"])
                metrics["agents"][agent_id] += 1

                if props["success"]:
                    metrics["successful_executions"] += 1

                retries = props.get("retries", 0)
                if retries > 0:
                    metrics["retry_counts"].append(retries)

        # Calculate derived metrics
        result = {}
        for tool_name, metrics in tool_metrics.items():
            if metrics["total_executions"] == 0:
                continue

            durations = metrics["durations"]
            result[tool_name] = {
                "total_executions": metrics["total_executions"],
                "success_rate": metrics["successful_executions"] / metrics["total_executions"],
                "avg_duration_ms": statistics.mean(durations),
                "median_duration_ms": statistics.median(durations),
                "p95_duration_ms": self.calculate_percentile(durations, 95),
                "agents_using": dict(metrics["agents"]),
                "avg_retries": (
                    statistics.mean(metrics["retry_counts"])
                    if metrics["retry_counts"]
                    else 0
                ),
                "retry_frequency": (
                    len(metrics["retry_counts"]) / metrics["total_executions"]
                ),
            }

        return result

    def generate_performance_report(self, hours_back: int = 24) -> dict[str, Any]:
        """Generate comprehensive performance report."""
        report = {
            "report_timestamp": datetime.now(UTC).isoformat(),
            "analysis_period_hours": hours_back,
            "agent_performance": self.analyze_agent_performance(hours_back),
            "delegation_patterns": self.analyze_delegation_patterns(hours_back),
            "tool_usage_patterns": self.analyze_tool_usage_patterns(hours_back)
        }

        # Calculate fleet-wide metrics
        agent_perf = report["agent_performance"]
        if agent_perf:
            all_durations = []
            total_tasks = 0
            total_successful = 0

            for agent_metrics in agent_perf.values():
                total_tasks += agent_metrics["total_tasks"]
                total_successful += agent_metrics["total_tasks"] * agent_metrics["success_rate"]
                # Approximate duration distribution
                avg_duration = agent_metrics["avg_duration_ms"]
                task_count = agent_metrics["total_tasks"]
                all_durations.extend([avg_duration] * task_count)

            fleet_metrics = {
                "total_tasks": total_tasks,
                "fleet_success_rate": total_successful / total_tasks if total_tasks > 0 else 0,
                "fleet_avg_duration_ms": statistics.mean(all_durations) if all_durations else 0,
                "active_agents": len(agent_perf),
                "most_active_agent": (
                    max(
                        agent_perf.keys(),
                        key=lambda agent: agent_perf[agent]["total_tasks"],
                    )
                    if agent_perf
                    else None
                ),
                "fastest_agent": (
                    min(
                        agent_perf.keys(),
                        key=lambda agent: agent_perf[agent]["avg_duration_ms"],
                    )
                    if agent_perf
                    else None
                ),
            }

            report["fleet_metrics"] = fleet_metrics

        return report

    def save_report(self, report: dict[str, Any], filename: str | None = None) -> str:
        """Save performance report to file."""
        if not filename:
            timestamp = datetime.now(UTC).strftime("%Y%m%d_%H%M%S")
            filename = f"performance_report_{timestamp}.json"

        report_path = self.reports_dir / filename
        with report_path.open("w") as f:
            json.dump(report, f, indent=2)

        return str(report_path)

    def get_agent_rankings(
        self,
        hours_back: int = 24,
        metric: str = "success_rate",
    ) -> list[tuple[str, float]]:
        """Get agent rankings by specified metric."""
        agent_perf = self.analyze_agent_performance(hours_back)

        if not agent_perf:
            return []

        rankings = []
        for agent_id, metrics in agent_perf.items():
            if metric in metrics:
                rankings.append((agent_id, metrics[metric]))

        return sorted(rankings, key=lambda x: x[1], reverse=True)

# Example usage functions for CLI integration
def generate_daily_report() -> str:
    """Generate and save daily performance report."""
    analyzer = PerformanceAnalyzer()
    report = analyzer.generate_performance_report(24)
    return analyzer.save_report(report)


def get_top_performers(
    metric: str = "success_rate",
    limit: int = 5,
) -> list[tuple[str, float]]:
    """Get top performing agents by metric."""
    analyzer = PerformanceAnalyzer()
    rankings = analyzer.get_agent_rankings(24, metric)
    return rankings[:limit]
