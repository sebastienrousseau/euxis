# euxis-metrics-cpp

C++23 agent performance metrics collection and analysis.

## Overview

Captures structured performance metrics from agent lifecycle events. Includes a high-performance fire-and-forget collector with background flush via `std::jthread`, and a statistical analyzer for generating fleet-wide performance reports.

## Key Types

- `PerformanceMetricsCollector` -- Structured event collector with session tracking
- `FastMetricsCollector` -- High-performance collector with bounded buffer and background flush
- `FastMetricsBuffer` -- Thread-safe circular deque
- `PerformanceAnalyzer` -- Statistical analysis and report generation
- Context structs: `TaskStartContext`, `TaskCompletionContext`, `TaskFailureContext`, `DelegationContext`, `ToolExecutionContext`

## Key Functions

- `task_started/completed/failed()` -- Agent task lifecycle tracking
- `delegation_started/completed()` -- Agent-to-agent delegation metrics
- `tool_execution()` -- Tool call performance recording
- `analyze_agent_performance()` -- Per-agent success rates and duration percentiles
- `generate_performance_report()` -- Comprehensive fleet metrics report

## Dependencies

- `nlohmann_json::nlohmann_json`
- `spdlog::spdlog`

## Build

Part of the euxis-cpp CMake project. Built via `euxis_add_library()` macro.
