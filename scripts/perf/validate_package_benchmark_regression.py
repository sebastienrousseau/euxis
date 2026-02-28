#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Validate package benchmark regressions against baseline."""

from __future__ import annotations

import argparse
import json
import sys
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
    parser.add_argument("--threshold-percent", type=float, default=20.0)
    parser.add_argument("--min-baseline-wall-ms", type=float, default=2.0)
    parser.add_argument("--min-baseline-cpu-ms", type=float, default=2.0)
    parser.add_argument("--min-baseline-memory-mb", type=float, default=32.0)
    parser.add_argument("--min-absolute-wall-ms", type=float, default=0.5)
    parser.add_argument("--min-absolute-cpu-ms", type=float, default=0.5)
    parser.add_argument("--min-absolute-memory-mb", type=float, default=4.0)
    parser.add_argument(
        "--warn-only",
        action="store_true",
        help="Exit 0 and print warnings instead of failing on regressions.",
    )
    parser.add_argument("--summary-output", default="", help="Optional markdown summary output")
    args = parser.parse_args()

    current = _load_json(Path(args.current))
    baseline = _load_json(Path(args.baseline))
    current_pkgs = current.get("packages", {})
    baseline_pkgs = baseline.get("packages", {})

    regressions: list[str] = []
    lines = [
        "## Package Benchmark Regression Validation",
        "",
        f"Threshold: `{args.threshold_percent:.2f}%`",
        f"Mode: `{'warn-only' if args.warn_only else 'blocking'}`",
        "",
        "| Package | Wall p95 Δ | CPU ms p95 Δ | Memory p95 Δ | Status |",
        "| --- | ---: | ---: | ---: | --- |",
    ]

    for package in sorted(set(current_pkgs) & set(baseline_pkgs)):
        c = current_pkgs[package]
        b = baseline_pkgs[package]
        c_wall = float(c["wall_ms"]["p95"])
        c_cpu_ms = float(c["cpu_ms"]["p95"])
        c_memory = float(c["memory_mb"]["p95"])
        b_wall = float(b["wall_ms"]["p95"])
        b_cpu_ms = float(b["cpu_ms"]["p95"])
        b_memory = float(b["memory_mb"]["p95"])

        wall = _pct_delta(c_wall, b_wall)
        cpu_ms = _pct_delta(c_cpu_ms, b_cpu_ms)
        memory = _pct_delta(c_memory, b_memory)

        wall_regressed = (
            b_wall >= args.min_baseline_wall_ms
            and (c_wall - b_wall) >= args.min_absolute_wall_ms
            and wall > args.threshold_percent
        )
        cpu_ms_regressed = (
            b_cpu_ms >= args.min_baseline_cpu_ms
            and (c_cpu_ms - b_cpu_ms) >= args.min_absolute_cpu_ms
            and cpu_ms > args.threshold_percent
        )
        memory_regressed = (
            b_memory >= args.min_baseline_memory_mb
            and (c_memory - b_memory) >= args.min_absolute_memory_mb
            and memory > args.threshold_percent
        )

        is_regressed = wall_regressed or cpu_ms_regressed or memory_regressed
        status = "regressed" if is_regressed else "ok"
        lines.append(f"| `{package}` | {wall:+.2f}% | {cpu_ms:+.2f}% | {memory:+.2f}% | {status} |")
        if is_regressed:
            regressions.append(package)

    lines.extend(["", f"Regressed packages: `{len(regressions)}`"])

    if args.summary_output:
        out = Path(args.summary_output)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text("\n".join(lines) + "\n", encoding="utf-8")

    if regressions:
        if args.warn_only:
            print("Package benchmark regression warnings:")
            for pkg in regressions:
                print(f"- {pkg}")
            print("\n".join(lines))
            return 0
        print("Package benchmark regression gate failed.", file=sys.stderr)
        for pkg in regressions:
            print(f"- {pkg}", file=sys.stderr)
        if args.summary_output:
            print(f"summary: {args.summary_output}", file=sys.stderr)
        return 1

    print("Package benchmark regression validation passed.")
    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
