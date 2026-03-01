"""Benchmark orchestrator for the Euxis platform."""

from __future__ import annotations

import argparse
import json
import sys
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Callable

# Ensure the benchmarks directory and source packages are importable.
_BENCH_DIR = Path(__file__).resolve().parent
_REPO_ROOT = _BENCH_DIR.parent.parent
for _p in [
    str(_BENCH_DIR),
    str(_REPO_ROOT / "euxis-bridge" / "src"),
    str(_REPO_ROOT / "euxis-core" / "src"),
    str(_REPO_ROOT / "euxis-crypto" / "src"),
    str(_REPO_ROOT / "euxis-identity" / "src"),
    str(_REPO_ROOT / "euxis-inference" / "src"),
    str(_REPO_ROOT / "euxis-a2a" / "src"),
]:
    if _p not in sys.path:
        sys.path.insert(0, _p)


@dataclass(frozen=True, slots=True)
class BenchmarkResult:
    """Result of a single benchmark."""

    name: str
    suite: str
    passed: bool
    duration_ms: float
    metrics: dict[str, Any] = field(default_factory=dict)
    error: str | None = None


@dataclass(slots=True)
class SuiteReport:
    """Aggregated report for a benchmark suite."""

    suite: str
    results: list[BenchmarkResult] = field(default_factory=list)
    total_duration_ms: float = 0.0

    @property
    def passed(self) -> bool:
        return all(r.passed for r in self.results)

    @property
    def pass_count(self) -> int:
        return sum(1 for r in self.results if r.passed)

    @property
    def fail_count(self) -> int:
        return sum(1 for r in self.results if not r.passed)

    def to_dict(self) -> dict[str, Any]:
        return {
            "suite": self.suite,
            "passed": self.passed,
            "pass_count": self.pass_count,
            "fail_count": self.fail_count,
            "total_duration_ms": round(self.total_duration_ms, 2),
            "results": [asdict(r) for r in self.results],
        }


class BenchmarkRunner:
    """Orchestrates benchmark execution."""

    def __init__(self) -> None:
        self._suites: dict[str, list[Callable[[], BenchmarkResult]]] = {}

    def register(self, suite: str, bench_fn: Callable[[], BenchmarkResult]) -> None:
        self._suites.setdefault(suite, []).append(bench_fn)

    def run_suite(self, suite: str) -> SuiteReport:
        report = SuiteReport(suite=suite)
        fns = self._suites.get(suite, [])
        suite_start = time.monotonic()
        for fn in fns:
            start = time.monotonic()
            try:
                result = fn()
            except Exception as exc:
                elapsed = (time.monotonic() - start) * 1000
                result = BenchmarkResult(
                    name=fn.__name__, suite=suite, passed=False,
                    duration_ms=elapsed, error=str(exc),
                )
            report.results.append(result)
        report.total_duration_ms = (time.monotonic() - suite_start) * 1000
        return report

    def run_all(self) -> list[SuiteReport]:
        return [self.run_suite(s) for s in sorted(self._suites)]

    @property
    def suite_names(self) -> list[str]:
        return sorted(self._suites.keys())


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Euxis benchmark runner")
    parser.add_argument("--suite", default="all", help="Suite to run (or 'all')")
    parser.add_argument("--output", default=None, help="Output JSON path")
    parser.add_argument("--format", choices=["json", "text"], default="text")
    return parser.parse_args()


def build_runner() -> BenchmarkRunner:
    """Build a runner with all available benchmark suites registered."""
    runner = BenchmarkRunner()

    try:
        from security_bench import register_benchmarks
        register_benchmarks(runner)
    except ImportError:
        pass

    try:
        from performance_bench import register_benchmarks
        register_benchmarks(runner)
    except ImportError:
        pass

    try:
        from autonomy_bench import register_benchmarks
        register_benchmarks(runner)
    except ImportError:
        pass

    try:
        from portability_bench import register_benchmarks
        register_benchmarks(runner)
    except ImportError:
        pass

    try:
        from interop_bench import register_benchmarks
        register_benchmarks(runner)
    except ImportError:
        pass

    return runner


def main() -> int:
    args = _parse_args()
    runner = build_runner()

    if args.suite == "all":
        reports = runner.run_all()
    else:
        reports = [runner.run_suite(args.suite)]

    all_passed = all(r.passed for r in reports)

    if args.format == "json":
        output = {"reports": [r.to_dict() for r in reports], "all_passed": all_passed}
        text = json.dumps(output, indent=2)
    else:
        lines = []
        for r in reports:
            lines.append(f"\n{'='*60}")
            lines.append(f"Suite: {r.suite} | {'PASS' if r.passed else 'FAIL'} | {r.total_duration_ms:.1f}ms")
            lines.append(f"  Passed: {r.pass_count} | Failed: {r.fail_count}")
            for result in r.results:
                status = "PASS" if result.passed else "FAIL"
                lines.append(f"  [{status}] {result.name} ({result.duration_ms:.1f}ms)")
                if result.error:
                    lines.append(f"         Error: {result.error}")
        lines.append(f"\n{'='*60}")
        lines.append(f"Overall: {'ALL PASSED' if all_passed else 'FAILURES DETECTED'}")
        text = "\n".join(lines)

    print(text)

    if args.output:
        out_path = Path(args.output)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_data = {"reports": [r.to_dict() for r in reports], "all_passed": all_passed}
        out_path.write_text(json.dumps(out_data, indent=2), encoding="utf-8")

    return 0 if all_passed else 1


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
