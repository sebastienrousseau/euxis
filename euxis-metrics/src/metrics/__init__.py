# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Euxis Metrics Library.

Provides both standard and high-performance metrics collection:
- PerformanceMetricsCollector: Full-featured agent metrics (JSON format)
- FastMetricsCollector: High-performance fire-and-forget metrics (<1ms overhead)

Usage:
    # Standard collector for detailed agent metrics
    from metrics.collectors.performance_collector import collector
    collector.task_started("architect", "session-123")

    # Fast collector for hot path metrics
    from metrics.collectors.fast_collector import record, record_timing
    record_timing("api_response", 5.2)
"""

__version__ = "0.1.0"

# Lazy imports to minimize cold start overhead
_perf_collector = None
_fast_collector = None


def get_performance_collector():
    """Get the standard performance metrics collector."""
    global _perf_collector
    if _perf_collector is None:
        from metrics.collectors.performance_collector import collector
        _perf_collector = collector
    return _perf_collector


def get_fast_collector():
    """Get the high-performance fast metrics collector."""
    global _fast_collector
    if _fast_collector is None:
        from metrics.collectors.fast_collector import fast_collector
        _fast_collector = fast_collector
    return _fast_collector
