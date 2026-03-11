#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Validate collected package benchmark metrics against package resource policy."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--benchmarks", required=True)
    parser.add_argument("--policy", required=True)
    parser.add_argument("--stage", default="current")
    parser.add_argument(
        "--enforce-cpu-pct",
        action="store_true",
        help="Also enforce cpu_pct threshold if policy provides it.",
    )
    args = parser.parse_args()

    benchmarks = _load_json(Path(args.benchmarks))
    policy = _load_json(Path(args.policy))
    stage = args.stage
    default_cpu_ms_budget = float(policy.get("default_cpu_ms_budget", {}).get(stage, 0.0))

    errors: list[str] = []
    if benchmarks.get("status") != "ok":
        errors.append("benchmark dataset status is not 'ok'")

    for name, metrics in benchmarks.get("packages", {}).items():
        policy_pkg = policy.get("packages", {}).get(name)
        if policy_pkg is None:
            errors.append(f"{name}: missing policy")
            continue
        thresholds = policy_pkg.get(stage)
        if thresholds is None:
            errors.append(f"{name}: missing stage '{stage}' policy")
            continue

        wall_p95 = float(metrics["wall_ms"]["p95"])
        cpu_ms_p95 = float(metrics.get("cpu_ms", {}).get("p95", 0.0))
        cpu_pct_p95 = float(metrics.get("cpu_pct", {}).get("p95", 0.0))
        memory_p95 = float(metrics["memory_mb"]["p95"])

        wall_budget = float(thresholds["p95_ms"])
        memory_budget = float(thresholds["memory_mb"])
        cpu_ms_budget = float(thresholds.get("cpu_ms", default_cpu_ms_budget if default_cpu_ms_budget > 0 else wall_budget))
        cpu_pct_budget = float(thresholds.get("cpu_pct", 100.0))

        if wall_p95 > wall_budget:
            errors.append(f"{name}: wall p95 {wall_p95:.3f}ms > budget {wall_budget:.3f}ms")
        if cpu_ms_p95 > cpu_ms_budget:
            errors.append(f"{name}: cpu_ms p95 {cpu_ms_p95:.3f}ms > budget {cpu_ms_budget:.3f}ms")
        if args.enforce_cpu_pct and cpu_pct_p95 > cpu_pct_budget:
            errors.append(f"{name}: cpu p95 {cpu_pct_p95:.3f}% > budget {cpu_pct_budget:.3f}%")
        if memory_p95 > memory_budget:
            errors.append(f"{name}: memory p95 {memory_p95:.3f}MB > budget {memory_budget:.3f}MB")

    if errors:
        print("Package benchmark budget validation failed.", file=sys.stderr)
        for err in errors:
            print(f"- {err}", file=sys.stderr)
        return 1

    print("Package benchmark budget validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
