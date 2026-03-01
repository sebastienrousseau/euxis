"""Portability benchmarks: platform detection, path normalization, env allowlist."""

from __future__ import annotations

import time

from euxis_ops.benchmarks.runner import BenchmarkResult, BenchmarkRunner


def _bench_platform_detection() -> BenchmarkResult:
    """Benchmark platform detection consistency."""
    from euxis_bridge.platform.factory import resolve_platform_ops

    start = time.monotonic()
    iterations = 100
    platforms = set()
    for _ in range(iterations):
        ops = resolve_platform_ops()
        info = ops.platform_info()
        platforms.add(info.platform)
    elapsed = (time.monotonic() - start) * 1000

    return BenchmarkResult(
        name="platform_detection_consistency",
        suite="portability",
        passed=len(platforms) == 1,
        duration_ms=elapsed,
        metrics={"platforms_detected": list(platforms), "consistent": len(platforms) == 1},
    )


def _bench_path_normalization() -> BenchmarkResult:
    """Benchmark path normalization across platform adapters."""
    from euxis_bridge.platform.factory import resolve_platform_ops

    ops = resolve_platform_ops()
    paths = ["relative/path", "another/deep/path", "simple"]

    start = time.monotonic()
    results = []
    for p in paths:
        normalized = ops.join_workspace(p)
        results.append(normalized)
    elapsed = (time.monotonic() - start) * 1000

    all_valid = all(".euxis" in r for r in results)
    return BenchmarkResult(
        name="path_normalization",
        suite="portability",
        passed=all_valid,
        duration_ms=elapsed,
        metrics={"paths_tested": len(paths), "all_valid": all_valid},
    )


def _bench_env_allowlist() -> BenchmarkResult:
    """Benchmark env allowlist filtering in NodeSkillBridge."""
    from euxis_bridge.core.node_bridge import NodeSkillBridge

    bridge = NodeSkillBridge()
    start = time.monotonic()
    iterations = 1000
    for _ in range(iterations):
        env = bridge._safe_env()
    elapsed = (time.monotonic() - start) * 1000

    no_secrets = not any(k in env for k in ("AWS_SECRET_ACCESS_KEY", "GITHUB_TOKEN", "PASSWORD"))
    return BenchmarkResult(
        name="env_allowlist_portability",
        suite="portability",
        passed=no_secrets,
        duration_ms=elapsed,
        metrics={"allowed_keys": list(env.keys()), "no_secrets_leaked": no_secrets},
    )


def register_benchmarks(runner: BenchmarkRunner) -> None:
    runner.register("portability", _bench_platform_detection)
    runner.register("portability", _bench_path_normalization)
    runner.register("portability", _bench_env_allowlist)
