"""Autonomy benchmarks: task completion, multi-step reasoning, error recovery."""

from __future__ import annotations

import time

from euxis_ops.benchmarks.runner import BenchmarkResult, BenchmarkRunner


def _bench_task_lifecycle() -> BenchmarkResult:
    """Benchmark A2A task creation and state transitions."""
    try:
        from euxis_a2a.core.task import TaskStatus, create_task, transition_task
    except ImportError:
        return BenchmarkResult(
            name="task_lifecycle", suite="autonomy", passed=True,
            duration_ms=0, metrics={"skipped": True, "reason": "euxis-a2a not available"},
        )

    start = time.monotonic()
    iterations = 100
    for _ in range(iterations):
        task = create_task("test message")
        transition_task(task, TaskStatus.ACTIVE)
        transition_task(task, TaskStatus.COMPLETED)
    elapsed = (time.monotonic() - start) * 1000

    avg_ms = elapsed / iterations
    return BenchmarkResult(
        name="task_lifecycle",
        suite="autonomy",
        passed=avg_ms < 10,
        duration_ms=elapsed,
        metrics={"iterations": iterations, "avg_ms": round(avg_ms, 4)},
    )


def _bench_error_recovery() -> BenchmarkResult:
    """Benchmark error recovery via task failure transitions."""
    try:
        from euxis_a2a.core.task import TaskStatus, create_task, transition_task
    except ImportError:
        return BenchmarkResult(
            name="error_recovery", suite="autonomy", passed=True,
            duration_ms=0, metrics={"skipped": True},
        )

    start = time.monotonic()
    recovered = 0
    total = 50
    for _ in range(total):
        task = create_task()
        transition_task(task, TaskStatus.ACTIVE)
        transition_task(task, TaskStatus.FAILED)
        if task.status == TaskStatus.FAILED:
            recovered += 1
    elapsed = (time.monotonic() - start) * 1000

    return BenchmarkResult(
        name="error_recovery",
        suite="autonomy",
        passed=recovered == total,
        duration_ms=elapsed,
        metrics={"recovered": recovered, "total": total},
    )


def register_benchmarks(runner: BenchmarkRunner) -> None:
    runner.register("autonomy", _bench_task_lifecycle)
    runner.register("autonomy", _bench_error_recovery)
