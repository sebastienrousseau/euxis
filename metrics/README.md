# Euxis Agent Performance Metrics System

## Overview

The Euxis Agent Performance Metrics System provides comprehensive observability for agent operations, tracking task completion times, success rates, delegation patterns, and tool usage across the fleet.

## Architecture

```
~/.euxis/metrics/
├── schemas/           # Event schemas and data models
├── collectors/        # Metrics collection implementations
├── aggregators/       # Analysis and aggregation logic
├── dashboards/        # Dashboard configurations
├── reports/          # Generated reports (auto-created)
├── events.jsonl      # Raw event stream (auto-created)
├── sessions.jsonl    # Session summaries (auto-created)
├── metrics_cli.py    # Command-line interface
└── README.md         # This documentation
```

## Event Taxonomy

### Performance Events
- **Agent:TaskStarted** - Agent begins task execution
- **Agent:TaskCompleted** - Agent successfully completes task
- **Agent:TaskFailed** - Agent fails to complete task
- **Agent:DelegationStarted** - Agent delegates work to another agent
- **Agent:DelegationCompleted** - Delegated work returns with results

### Resource Events
- **Agent:MemoryOperation** - Cortex memory operations (remember/recall/relate)
- **Agent:ToolExecution** - Tool execution with timing and success metrics

### Quality Events
- **Agent:ReflexionGenerated** - Failure analysis and learning
- **Agent:ConflictDetected** - Inter-agent conflict resolution

## Usage

### Command Line Interface

#### Generate Performance Report
```bash
# Summary report for last 24 hours
python3 ~/.euxis/metrics/metrics_cli.py report

# JSON report for last week
python3 ~/.euxis/metrics/metrics_cli.py report --hours 168 --format json

# CSV export for analysis
python3 ~/.euxis/metrics/metrics_cli.py report --format csv > agent_performance.csv
```

#### Agent Rankings
```bash
# Top performers by success rate
python3 ~/.euxis/metrics/metrics_cli.py rankings --metric success_rate

# Fastest agents (lowest average duration)
python3 ~/.euxis/metrics/metrics_cli.py rankings --metric avg_duration_ms

# Most active agents
python3 ~/.euxis/metrics/metrics_cli.py rankings --metric total_tasks
```

#### Live Monitoring
```bash
# Monitor fleet metrics in real-time
python3 ~/.euxis/metrics/metrics_cli.py monitor --refresh 10
```

#### Data Management
```bash
# Clean up large log files
python3 ~/.euxis/metrics/metrics_cli.py cleanup --max-size-mb 50

# Export metrics for external analysis
python3 ~/.euxis/metrics/metrics_cli.py export --output metrics_backup.json --hours 720
```

### Programmatic Integration

#### Collector Usage
```python
from metrics.collectors.performance_collector import collector

# Track task lifecycle
correlation_id = collector.task_started("architect", "20260204-081716", "user_request", "P1")

# Record tool usage
collector.tool_execution("architect", "Read", 150, success=True, correlation_id=correlation_id)

# Record completion
collector.task_completed("20260204-081716", "SUCCESS", artifacts_created=3,
                        tool_calls_count=5, handoff_required=False)
```

#### Analyzer Usage
```python
from metrics.aggregators.performance_analyzer import PerformanceAnalyzer

analyzer = PerformanceAnalyzer()

# Get agent performance metrics
agent_metrics = analyzer.analyze_agent_performance(24)
print(f"Architect success rate: {agent_metrics['architect']['success_rate']:.1%}")

# Analyze delegation patterns
delegation_patterns = analyzer.analyze_delegation_patterns(24)

# Generate comprehensive report
report = analyzer.generate_performance_report(24)
report_path = analyzer.save_report(report)
```

## Integration with Existing Infrastructure

### Bus Integration
The metrics system subscribes to existing bus topics:
- `agent-status` - Lifecycle events for automatic task tracking
- `task-events` - Task creation and completion
- `health-checks` - Periodic agent health status

### Lifecycle Integration
Extends existing lifecycle tracking in `/home/seb/.euxis/data/lifecycle/transitions.jsonl` with detailed performance metrics.

### Cortex Integration
Integrates with `euxis-cortex` for storing performance insights as procedural memories.

## Metrics Collected

### Task Performance
- **Duration**: Task execution time from start to completion
- **Success Rate**: Percentage of successful task completions
- **Failure Analysis**: Categorized failure reasons and patterns
- **Complexity Tracking**: Simple/medium/complex task performance variations

### Delegation Analysis
- **Delegation Frequency**: How often agents delegate work
- **Handoff Success**: Success rate of delegation handoffs
- **Delegation Duration**: Time from delegation to completion
- **Agent Pair Analysis**: Which agents work well together

### Tool Usage Patterns
- **Tool Performance**: Success rates and execution times per tool
- **Retry Analysis**: Tool failure and retry patterns
- **Agent Tool Preferences**: Which agents use which tools most effectively

### Fleet Health
- **Active Agents**: Number of agents processing tasks
- **Workload Distribution**: Task distribution across agents
- **Performance Trends**: Historical performance patterns

## Dashboard Configuration

The system includes pre-configured dashboards for:

### Agent Performance Overview
- Fleet-wide success rates and task volumes
- Agent performance comparisons
- Task duration distributions

### Delegation Analysis
- Delegation frequency heatmaps
- Handoff success rates
- Cross-agent collaboration patterns

### Tool Usage Analysis
- Tool performance rankings
- Execution time comparisons
- Retry frequency analysis

### Performance Trends
- Historical success rate trends
- Duration trend analysis
- Task volume patterns

## Alerting

Built-in alerts for:
- **Low Fleet Success Rate** (< 90%)
- **High Delegation Failure** (< 95% handoff success)
- **Tool Performance Degradation** (< 95% tool success)
- **High Task Duration** (> 5min P95)

## Data Retention

- **Events Stream**: Raw events in `/home/seb/.euxis/metrics/events.jsonl`
- **Session Summaries**: Completed sessions in `/home/seb/.euxis/metrics/sessions.jsonl`
- **Reports**: Historical reports in `/home/seb/.euxis/metrics/reports/`

Data is automatically rotated when files exceed configured size limits to prevent disk space issues.

## Example Output

### Performance Report Summary
```
📊 AGENT PERFORMANCE REPORT
═══════════════════════════════════════
Analysis Period: 24 hours
Generated: 2026-02-04 08:17:16

🚀 FLEET OVERVIEW
─────────────────────
Total Tasks: 157
Success Rate: 94.3%
Avg Duration: 2,845ms
Active Agents: 23
Most Active: architect
Fastest: reviewer

🤖 TOP PERFORMERS
─────────────────────
1. reviewer            Success: 98.5% Tasks: 12   Avg: 1,245ms
2. edge-hunter         Success: 97.2% Tasks: 8    Avg: 1,892ms
3. architect           Success: 95.8% Tasks: 34   Avg: 3,156ms
4. compliance-officer  Success: 95.0% Tasks: 6    Avg: 2,103ms
5. qa-coordinator      Success: 94.1% Tasks: 9    Avg: 2,567ms
```

## Schema Validation

All events conform to the schema defined in `schemas/agent-performance.json`. The schema includes:
- Required and optional properties for each event type
- Data type validation
- Enumerated values for categorical fields
- Cardinality estimates for storage planning

This ensures consistent, analyzable data across all agent operations.