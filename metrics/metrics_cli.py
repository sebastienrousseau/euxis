#!/usr/bin/env python3
"""
Euxis Agent Performance Metrics CLI
Command-line interface for metrics collection and analysis
"""

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, Any

# Add local modules to path
sys.path.insert(0, str(Path(__file__).parent))

from collectors.performance_collector import PerformanceMetricsCollector
from aggregators.performance_analyzer import PerformanceAnalyzer

def cmd_report(args):
    """Generate performance report"""
    analyzer = PerformanceAnalyzer()

    if args.format == "summary":
        # Generate summary report
        report = analyzer.generate_performance_report(args.hours)

        print(f"\n📊 AGENT PERFORMANCE REPORT")
        print(f"═══════════════════════════════════════")
        print(f"Analysis Period: {args.hours} hours")
        print(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")

        fleet_metrics = report.get("fleet_metrics", {})
        if fleet_metrics:
            print(f"\n🚀 FLEET OVERVIEW")
            print(f"─────────────────────")
            print(f"Total Tasks: {fleet_metrics.get('total_tasks', 0)}")
            print(f"Success Rate: {fleet_metrics.get('fleet_success_rate', 0):.1%}")
            print(f"Avg Duration: {fleet_metrics.get('fleet_avg_duration_ms', 0):.0f}ms")
            print(f"Active Agents: {fleet_metrics.get('active_agents', 0)}")

            most_active = fleet_metrics.get('most_active_agent')
            fastest = fleet_metrics.get('fastest_agent')
            if most_active:
                print(f"Most Active: {most_active}")
            if fastest:
                print(f"Fastest: {fastest}")

        # Agent performance summary
        agent_perf = report.get("agent_performance", {})
        if agent_perf:
            print(f"\n🤖 TOP PERFORMERS")
            print(f"─────────────────────")

            # Sort by success rate
            sorted_agents = sorted(
                agent_perf.items(),
                key=lambda x: (x[1]["success_rate"], -x[1]["avg_duration_ms"]),
                reverse=True
            )

            for i, (agent_id, metrics) in enumerate(sorted_agents[:5]):
                print(f"{i+1}. {agent_id:<20} "
                      f"Success: {metrics['success_rate']:.1%} "
                      f"Tasks: {metrics['total_tasks']:<4} "
                      f"Avg: {metrics['avg_duration_ms']:.0f}ms")

        # Delegation patterns
        delegation = report.get("delegation_patterns", {})
        if delegation:
            print(f"\n🔄 TOP DELEGATION PATTERNS")
            print(f"─────────────────────────────")

            sorted_delegations = sorted(
                delegation.items(),
                key=lambda x: x[1]["delegation_frequency"],
                reverse=True
            )

            for pair, metrics in sorted_delegations[:5]:
                print(f"{pair:<25} "
                      f"Freq: {metrics['delegation_frequency']:<4} "
                      f"Success: {metrics['handoff_success_rate']:.1%} "
                      f"Avg: {metrics['avg_delegation_duration_ms']:.0f}ms")

        # Tool usage
        tools = report.get("tool_usage_patterns", {})
        if tools:
            print(f"\n🛠️  TOOL PERFORMANCE")
            print(f"─────────────────────")

            sorted_tools = sorted(
                tools.items(),
                key=lambda x: x[1]["total_executions"],
                reverse=True
            )

            for tool, metrics in sorted_tools[:5]:
                print(f"{tool:<15} "
                      f"Exec: {metrics['total_executions']:<6} "
                      f"Success: {metrics['success_rate']:.1%} "
                      f"Avg: {metrics['avg_duration_ms']:.0f}ms")

    elif args.format == "json":
        # Generate full JSON report
        report = analyzer.generate_performance_report(args.hours)
        print(json.dumps(report, indent=2))

    elif args.format == "csv":
        # Generate CSV format for agents
        agent_perf = analyzer.analyze_agent_performance(args.hours)
        print("agent_id,total_tasks,success_rate,avg_duration_ms,p95_duration_ms")
        for agent_id, metrics in agent_perf.items():
            print(f"{agent_id},{metrics['total_tasks']},{metrics['success_rate']:.3f},"
                  f"{metrics['avg_duration_ms']:.0f},{metrics['p95_duration_ms']:.0f}")

def cmd_rankings(args):
    """Show agent rankings by metric"""
    analyzer = PerformanceAnalyzer()
    rankings = analyzer.get_agent_rankings(args.hours, args.metric)

    print(f"\n🏆 AGENT RANKINGS: {args.metric.upper()}")
    print(f"═══════════════════════════════════════")
    print(f"Period: {args.hours} hours")

    for i, (agent_id, value) in enumerate(rankings[:args.limit]):
        if args.metric.endswith("_rate"):
            value_str = f"{value:.1%}"
        elif args.metric.endswith("_ms"):
            value_str = f"{value:.0f}ms"
        else:
            value_str = f"{value:.2f}"

        print(f"{i+1:2}. {agent_id:<20} {value_str}")

def cmd_monitor(args):
    """Monitor live metrics (simplified version)"""
    analyzer = PerformanceAnalyzer()

    print(f"\n📡 LIVE METRICS MONITOR")
    print(f"═══════════════════════════════════════")
    print(f"Refresh every {args.refresh}s (Ctrl+C to stop)")

    import time
    try:
        while True:
            report = analyzer.generate_performance_report(1)  # Last hour
            fleet = report.get("fleet_metrics", {})

            print(f"\r{datetime.now().strftime('%H:%M:%S')} | "
                  f"Tasks: {fleet.get('total_tasks', 0)} | "
                  f"Success: {fleet.get('fleet_success_rate', 0):.1%} | "
                  f"Agents: {fleet.get('active_agents', 0)}", end="", flush=True)

            time.sleep(args.refresh)
    except KeyboardInterrupt:
        print("\nMonitoring stopped.")

def cmd_cleanup(args):
    """Clean up old metrics data"""
    metrics_dir = Path("/home/seb/.euxis/metrics")

    events_file = metrics_dir / "events.jsonl"
    sessions_file = metrics_dir / "sessions.jsonl"

    # Simple cleanup: truncate files if they're too large
    max_size = args.max_size_mb * 1024 * 1024  # Convert to bytes

    cleaned = []
    for file_path in [events_file, sessions_file]:
        if file_path.exists() and file_path.stat().st_size > max_size:
            # Read last N lines to keep recent data
            with open(file_path) as f:
                lines = f.readlines()

            # Keep last 10,000 lines
            keep_lines = lines[-10000:]

            with open(file_path, 'w') as f:
                f.writelines(keep_lines)

            cleaned.append(str(file_path))

    if cleaned:
        print(f"Cleaned up {len(cleaned)} files:")
        for file_path in cleaned:
            print(f"  - {file_path}")
    else:
        print("No cleanup needed.")

def cmd_export(args):
    """Export metrics data"""
    analyzer = PerformanceAnalyzer()
    report = analyzer.generate_performance_report(args.hours)

    output_path = Path(args.output)
    with open(output_path, 'w') as f:
        if args.format == "json":
            json.dump(report, f, indent=2)
        elif args.format == "csv":
            # Export agent performance as CSV
            import csv
            agent_perf = report.get("agent_performance", {})

            fieldnames = ["agent_id", "total_tasks", "success_rate", "avg_duration_ms",
                         "p95_duration_ms", "failure_rate"]

            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()

            for agent_id, metrics in agent_perf.items():
                row = {"agent_id": agent_id}
                row.update({k: v for k, v in metrics.items() if k in fieldnames})
                writer.writerow(row)

    print(f"Exported metrics to: {output_path}")

def main():
    """Main CLI entry point"""
    parser = argparse.ArgumentParser(
        description="Euxis Agent Performance Metrics CLI",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    subparsers = parser.add_subparsers(dest="command", help="Available commands")

    # Report command
    report_parser = subparsers.add_parser("report", help="Generate performance report")
    report_parser.add_argument("--hours", type=int, default=24, help="Hours to analyze (default: 24)")
    report_parser.add_argument("--format", choices=["summary", "json", "csv"], default="summary",
                              help="Output format (default: summary)")

    # Rankings command
    rankings_parser = subparsers.add_parser("rankings", help="Show agent rankings")
    rankings_parser.add_argument("--metric", default="success_rate",
                                choices=["success_rate", "avg_duration_ms", "total_tasks"],
                                help="Metric to rank by (default: success_rate)")
    rankings_parser.add_argument("--hours", type=int, default=24, help="Hours to analyze (default: 24)")
    rankings_parser.add_argument("--limit", type=int, default=10, help="Number of agents to show (default: 10)")

    # Monitor command
    monitor_parser = subparsers.add_parser("monitor", help="Monitor live metrics")
    monitor_parser.add_argument("--refresh", type=int, default=30, help="Refresh interval in seconds (default: 30)")

    # Cleanup command
    cleanup_parser = subparsers.add_parser("cleanup", help="Clean up old metrics data")
    cleanup_parser.add_argument("--max-size-mb", type=int, default=100,
                               help="Maximum file size in MB before cleanup (default: 100)")

    # Export command
    export_parser = subparsers.add_parser("export", help="Export metrics data")
    export_parser.add_argument("--output", required=True, help="Output file path")
    export_parser.add_argument("--hours", type=int, default=24, help="Hours to export (default: 24)")
    export_parser.add_argument("--format", choices=["json", "csv"], default="json",
                              help="Export format (default: json)")

    args = parser.parse_args()

    if args.command == "report":
        cmd_report(args)
    elif args.command == "rankings":
        cmd_rankings(args)
    elif args.command == "monitor":
        cmd_monitor(args)
    elif args.command == "cleanup":
        cmd_cleanup(args)
    elif args.command == "export":
        cmd_export(args)
    else:
        parser.print_help()

if __name__ == "__main__":
    main()