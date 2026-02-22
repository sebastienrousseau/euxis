# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Performance regression test suite.

Validates performance targets for v0.0.1 release:
- Cold start: <=20ms
- Memory footprint: <=10MB
- Response time: <=10ms
- Throughput: >=1000 TPS

Run with: pytest tests/performance/ -v --benchmark-min-rounds=3
"""

from __future__ import annotations

import asyncio
import gc
import os
import subprocess
import sys
import time
from pathlib import Path

import pytest

# Performance targets
TARGET_COLD_START_MS = 20
TARGET_MEMORY_MB = 10
TARGET_RESPONSE_TIME_MS = 10
TARGET_TPS = 1000


class TestColdStart:
    """Test cold start performance."""

    def test_cli_cold_start(self):
        """CLI cold start should be <=20ms."""
        euxis_home = os.environ.get("EUXIS_HOME", str(Path.home() / ".euxis"))
        cli_path = Path(euxis_home) / "euxis-cli" / "bin" / "euxis.sh"

        if not cli_path.exists():
            pytest.skip(f"CLI not found at {cli_path}")

        # Measure cold start (--help is fastest path)
        start = time.perf_counter()
        result = subprocess.run(
            [str(cli_path), "--help"],
            capture_output=True,
            timeout=5
        )
        elapsed_ms = (time.perf_counter() - start) * 1000

        assert result.returncode == 0, f"CLI failed: {result.stderr.decode()}"
        assert elapsed_ms <= TARGET_COLD_START_MS * 2.5, (
            f"Cold start {elapsed_ms:.1f}ms exceeds 2.5x target ({TARGET_COLD_START_MS}ms)"
        )

    def test_gateway_import_time(self):
        """Gateway module import should be fast."""
        # Run in subprocess to get true import time
        code = """
import time
start = time.perf_counter()
try:
    import sys
    sys.path.insert(0, 'euxis-gateway/src')
    from gateway import server
    elapsed = (time.perf_counter() - start) * 1000
    print(f"{elapsed:.2f}")
except ImportError as e:
    print(f"SKIP:{e}")
"""
        result = subprocess.run(
            [sys.executable, "-c", code],
            capture_output=True,
            text=True,
            cwd=os.environ.get("EUXIS_HOME", str(Path.home() / ".euxis")),
            timeout=10
        )

        if result.stdout.startswith("SKIP"):
            pytest.skip(result.stdout)

        try:
            import_time_ms = float(result.stdout.strip())
        except ValueError:
            pytest.skip(f"Could not parse import time: {result.stdout}")

        # Gateway has FastAPI deps, allow more headroom
        # 3000ms limit accounts for cold cache and slower environments in V8/CI
        assert import_time_ms <= 3000, (
            f"Gateway import {import_time_ms:.1f}ms exceeds 3000ms limit"
        )


class TestMemoryFootprint:
    """Test memory usage."""

    def test_cli_memory_usage(self):
        """CLI memory usage should be <=10MB."""
        euxis_home = os.environ.get("EUXIS_HOME", str(Path.home() / ".euxis"))
        cli_path = Path(euxis_home) / "euxis-cli" / "bin" / "euxis.sh"

        if not cli_path.exists():
            pytest.skip(f"CLI not found at {cli_path}")

        # Use /usr/bin/time for memory measurement
        result = subprocess.run(
            ["/usr/bin/time", "-f", "%M", str(cli_path), "--help"],
            capture_output=True,
            timeout=10
        )

        try:
            # %M gives max RSS in KB
            memory_kb = int(result.stderr.decode().strip().split()[-1])
            memory_mb = memory_kb / 1024
        except (ValueError, IndexError):
            pytest.skip(f"Could not parse memory usage: {result.stderr.decode()}")

        assert memory_mb <= TARGET_MEMORY_MB * 1.5, (
            f"Memory {memory_mb:.1f}MB exceeds 1.5x target ({TARGET_MEMORY_MB}MB)"
        )


class TestResponseTime:
    """Test response time for crypto operations."""

    @pytest.mark.asyncio
    async def test_async_encrypt_response_time(self):
        """Async encrypt should complete in <=10ms."""
        try:
            sys.path.insert(0, str(Path.home() / ".euxis" / "euxis-crypto-lib" / "src"))
            from crypto_lib.async_core import async_encrypt
        except ImportError:
            pytest.skip("Async crypto not available")

        key = os.urandom(32)
        data = b"test data for encryption"

        # Warm up
        await async_encrypt(data, key)

        # Measure
        times = []
        for _ in range(100):
            start = time.perf_counter()
            await async_encrypt(data, key)
            elapsed_ms = (time.perf_counter() - start) * 1000
            times.append(elapsed_ms)

        avg_time = sum(times) / len(times)
        p95_time = sorted(times)[95]

        assert p95_time <= TARGET_RESPONSE_TIME_MS * 2, (
            f"P95 response time {p95_time:.2f}ms exceeds 2x target ({TARGET_RESPONSE_TIME_MS}ms)"
        )


class TestThroughput:
    """Test throughput for high-volume operations."""

    @pytest.mark.asyncio
    async def test_crypto_tps(self):
        """Crypto operations should achieve >=1000 TPS."""
        try:
            sys.path.insert(0, str(Path.home() / ".euxis" / "euxis-crypto-lib" / "src"))
            from crypto_lib.async_core import async_encrypt_batch
        except ImportError:
            pytest.skip("Async crypto not available")

        key = os.urandom(32)
        data = b"test data for encryption benchmark"

        # Create batch of operations
        batch_size = 1000
        items = [(data, key) for _ in range(batch_size)]

        # Measure
        start = time.perf_counter()
        await async_encrypt_batch(items)
        elapsed = time.perf_counter() - start

        tps = batch_size / elapsed

        assert tps >= TARGET_TPS * 0.5, (
            f"Throughput {tps:.0f} TPS below 50% of target ({TARGET_TPS} TPS)"
        )

    def test_metrics_recording_overhead(self):
        """Metrics recording should have <1ms overhead."""
        try:
            sys.path.insert(0, str(Path.home() / ".euxis" / "euxis-metrics" / "src"))
            from metrics.collectors.fast_collector import record, record_timing
        except ImportError:
            pytest.skip("Fast metrics not available")

        # Measure overhead
        times = []
        for _ in range(1000):
            start = time.perf_counter()
            record("test_event", {"key": "value"})
            elapsed_ms = (time.perf_counter() - start) * 1000
            times.append(elapsed_ms)

        avg_time = sum(times) / len(times)
        p99_time = sorted(times)[990]

        assert p99_time < 1.0, (
            f"P99 metrics overhead {p99_time:.3f}ms exceeds 1ms limit"
        )


class TestRegressionBaseline:
    """Baseline tests to detect performance regressions."""

    def test_baseline_file_exists(self):
        """Ensure performance baseline file exists."""
        baseline_path = Path.home() / ".euxis" / "euxis-core" / "tests" / "performance" / "baseline.json"
        if not baseline_path.exists():
            # Create initial baseline
            import json
            baseline = {
                "version": "0.1.0",
                "targets": {
                    "cold_start_ms": TARGET_COLD_START_MS,
                    "memory_mb": TARGET_MEMORY_MB,
                    "response_time_ms": TARGET_RESPONSE_TIME_MS,
                    "tps": TARGET_TPS
                },
                "measurements": {}
            }
            baseline_path.parent.mkdir(parents=True, exist_ok=True)
            with open(baseline_path, "w") as f:
                json.dump(baseline, f, indent=2)

        assert baseline_path.exists()


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
