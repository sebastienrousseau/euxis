"""Performance benchmarks: bridge latency, crypto throughput, serialization."""

from __future__ import annotations

import statistics
import tempfile
import time
from pathlib import Path

from euxis_ops.benchmarks.runner import BenchmarkResult, BenchmarkRunner


def _bench_crypto_throughput() -> BenchmarkResult:
    """Benchmark AES-256-GCM encryption throughput."""
    try:
        from crypto_lib.core import decrypt, encrypt
        from crypto_lib.key_management import generate_key
    except ImportError:
        return BenchmarkResult(
            name="crypto_throughput", suite="performance", passed=True,
            duration_ms=0, metrics={"skipped": True},
        )

    key = generate_key(32)
    data = "x" * 1024  # 1KB payload
    iterations = 200

    start = time.monotonic()
    for _ in range(iterations):
        enc = encrypt(data, key)
        decrypt(enc, key)
    elapsed = (time.monotonic() - start) * 1000

    ops_per_sec = iterations / (elapsed / 1000)
    return BenchmarkResult(
        name="crypto_throughput",
        suite="performance",
        passed=ops_per_sec > 100,
        duration_ms=elapsed,
        metrics={
            "iterations": iterations,
            "ops_per_sec": round(ops_per_sec, 1),
            "avg_ms": round(elapsed / iterations, 3),
        },
    )


def _bench_crypto_aad_throughput() -> BenchmarkResult:
    """Benchmark AES-256-GCM AAD encryption throughput."""
    try:
        from crypto_lib.core import decrypt_aad, encrypt_aad
        from crypto_lib.key_management import generate_key
    except ImportError:
        return BenchmarkResult(
            name="crypto_aad_throughput", suite="performance", passed=True,
            duration_ms=0, metrics={"skipped": True},
        )

    key = generate_key(32)
    data = "x" * 1024
    aad = b"tier:hot"
    iterations = 200

    start = time.monotonic()
    for _ in range(iterations):
        enc = encrypt_aad(data, key, aad)
        decrypt_aad(enc, key, aad)
    elapsed = (time.monotonic() - start) * 1000

    ops_per_sec = iterations / (elapsed / 1000)
    return BenchmarkResult(
        name="crypto_aad_throughput",
        suite="performance",
        passed=ops_per_sec > 100,
        duration_ms=elapsed,
        metrics={"iterations": iterations, "ops_per_sec": round(ops_per_sec, 1)},
    )


def _bench_key_derivation() -> BenchmarkResult:
    """Benchmark PBKDF2 key derivation latency."""
    try:
        from crypto_lib.key_management import derive_key
    except ImportError:
        return BenchmarkResult(
            name="key_derivation", suite="performance", passed=True,
            duration_ms=0, metrics={"skipped": True},
        )

    iterations = 5
    latencies: list[float] = []
    for _ in range(iterations):
        start = time.monotonic()
        derive_key("password123", iterations=10_000)
        latencies.append((time.monotonic() - start) * 1000)

    return BenchmarkResult(
        name="key_derivation",
        suite="performance",
        passed=statistics.median(latencies) < 5000,
        duration_ms=sum(latencies),
        metrics={
            "p50_ms": round(statistics.median(latencies), 2),
            "p95_ms": round(sorted(latencies)[int(len(latencies) * 0.95)], 2),
            "iterations": iterations,
        },
    )


def _bench_skill_import_throughput() -> BenchmarkResult:
    """Benchmark skill import throughput."""
    from euxis_bridge.core.importer import ClawHubImporter

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "skills"
        for i in range(20):
            skill_dir = root / f"skill-{i}"
            skill_dir.mkdir(parents=True)
            (skill_dir / "SKILL.md").write_text(
                f"---\nname: Skill {i}\nslug: skill-{i}\nruntime: node\nentrypoint: index.js\n---\n",
                encoding="utf-8",
            )

        importer = ClawHubImporter(root)
        start = time.monotonic()
        skills = importer.import_skills()
        elapsed = (time.monotonic() - start) * 1000

    return BenchmarkResult(
        name="skill_import_throughput",
        suite="performance",
        passed=elapsed < 5000,
        duration_ms=elapsed,
        metrics={"skills_imported": len(skills), "throughput_per_sec": round(len(skills) / (elapsed / 1000), 1)},
    )


def _bench_serialization_speed() -> BenchmarkResult:
    """Benchmark BridgedSkill serialization."""
    from euxis_bridge.core.models import BridgedSkill

    skill = BridgedSkill(
        name="Test", slug="test", source_dir="/tmp/test",
        description="Test skill", runtime="node", entrypoint="index.js",
    )

    iterations = 1000
    start = time.monotonic()
    for _ in range(iterations):
        skill.to_dict()
    elapsed = (time.monotonic() - start) * 1000

    return BenchmarkResult(
        name="serialization_speed",
        suite="performance",
        passed=elapsed < 1000,
        duration_ms=elapsed,
        metrics={"iterations": iterations, "avg_us": round(elapsed / iterations * 1000, 2)},
    )


def register_benchmarks(runner: BenchmarkRunner) -> None:
    runner.register("performance", _bench_crypto_throughput)
    runner.register("performance", _bench_crypto_aad_throughput)
    runner.register("performance", _bench_key_derivation)
    runner.register("performance", _bench_skill_import_throughput)
    runner.register("performance", _bench_serialization_speed)
