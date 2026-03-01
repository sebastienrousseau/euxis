#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Render package benchmark trend summary markdown from current vs baseline data."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _pct_delta(current: float, baseline: float) -> float:
    if baseline == 0:
        return 0.0
    return ((current - baseline) / baseline) * 100.0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--current", required=True, help="Current benchmark JSON")
    parser.add_argument("--baseline", required=True, help="Baseline benchmark JSON")
    parser.add_argument("--threshold-percent", type=float, default=10.0)
    parser.add_argument("--output", required=True, help="Markdown output path")
    args = parser.parse_args()

    current = _load_json(Path(args.current))
    baseline = _load_json(Path(args.baseline))

    current_pkgs = current.get("packages", {})
    baseline_pkgs = baseline.get("packages", {})

    lines = [
        "## Package Benchmark Trend (Warning Only)",
        "",
        f"Threshold: `{args.threshold_percent:.2f}%` regression",
        "",
        "| Package | Wall p95 Δ | CPU ms p95 Δ | Memory p95 Δ | Status |",
        "| --- | ---: | ---: | ---: | --- |",
    ]

    warned = 0
    for package in sorted(set(current_pkgs) & set(baseline_pkgs)):
        c = current_pkgs[package]
        b = baseline_pkgs[package]
        wall_delta = _pct_delta(float(c["wall_ms"]["p95"]), float(b["wall_ms"]["p95"]))
        cpu_ms_delta = _pct_delta(float(c["cpu_ms"]["p95"]), float(b["cpu_ms"]["p95"]))
        mem_delta = _pct_delta(float(c["memory_mb"]["p95"]), float(b["memory_mb"]["p95"]))
        regressed = any(delta > args.threshold_percent for delta in (wall_delta, cpu_ms_delta, mem_delta))
        status = "warning" if regressed else "ok"
        if regressed:
            warned += 1
        lines.append(
            f"| `{package}` | {wall_delta:+.2f}% | {cpu_ms_delta:+.2f}% | {mem_delta:+.2f}% | {status} |"
        )

    lines.extend(
        [
            "",
            f"Regressed packages: `{warned}`",
        ]
    )

    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
