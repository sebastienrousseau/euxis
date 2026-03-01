"""Markdown reporter for benchmark results."""

from __future__ import annotations

from pathlib import Path

from euxis_ops.benchmarks.runner import SuiteReport


def export_markdown(reports: list[SuiteReport], output_path: Path) -> None:
    """Export benchmark results as Markdown."""
    output_path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["# Euxis Benchmark Report\n"]

    for report in reports:
        status = "PASS" if report.passed else "FAIL"
        lines.append(f"## {report.suite} ({status})\n")
        lines.append(f"| Benchmark | Status | Duration (ms) |")
        lines.append(f"|-----------|--------|---------------|")
        for r in report.results:
            s = "PASS" if r.passed else "FAIL"
            lines.append(f"| {r.name} | {s} | {r.duration_ms:.1f} |")
        lines.append("")

    overall = "ALL PASSED" if all(r.passed for r in reports) else "FAILURES DETECTED"
    lines.append(f"**Overall: {overall}**\n")
    output_path.write_text("\n".join(lines), encoding="utf-8")
