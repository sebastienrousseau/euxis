#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Generate release readiness checklist and enforce perf regression policy."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

PERF_DIR = Path(__file__).resolve().parents[1] / "perf"
if str(PERF_DIR) not in sys.path:
    sys.path.insert(0, str(PERF_DIR))

from perf_utils import extract_latencies_ms, p95


def _load_budget(policy_path: Path, stage: str) -> float:
    policy = json.loads(policy_path.read_text(encoding="utf-8"))
    budget = policy["stages"][stage]["p95_budget_ms"]
    return float(budget)


def _load_baseline(baseline_path: Path) -> float:
    baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
    value = baseline.get("previous_release_p95_ms")
    if not isinstance(value, (int, float)):
        raise ValueError(f"Invalid previous_release_p95_ms in {baseline_path}")
    return float(value)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--metrics", required=True)
    parser.add_argument("--policy", required=True)
    parser.add_argument("--stage", default="current")
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--threshold-percent", type=float, default=10.0)
    parser.add_argument("--output", required=True)
    parser.add_argument("--json-output", required=True)
    args = parser.parse_args()

    metrics_path = Path(args.metrics)
    policy_path = Path(args.policy)
    baseline_path = Path(args.baseline)

    current_p95 = p95(extract_latencies_ms(metrics_path))
    budget = _load_budget(policy_path, args.stage)
    baseline_p95 = _load_baseline(baseline_path)
    delta_ms = current_p95 - baseline_p95
    delta_percent = (delta_ms / baseline_p95 * 100.0) if baseline_p95 else 0.0
    budget_ok = current_p95 <= budget
    regression_ok = delta_percent <= args.threshold_percent
    release_ok = budget_ok and regression_ok

    report = {
        "current_p95_ms": round(current_p95, 2),
        "budget_ms": round(budget, 2),
        "baseline_p95_ms": round(baseline_p95, 2),
        "delta_ms": round(delta_ms, 2),
        "delta_percent": round(delta_percent, 2),
        "threshold_percent": args.threshold_percent,
        "budget_ok": budget_ok,
        "regression_ok": regression_ok,
        "release_ok": release_ok,
        "stage": args.stage,
    }

    checklist = (
        "# Release Readiness Checklist\n\n"
        f"- [{'x' if budget_ok else ' '}] p95 is within stage budget (`{current_p95:.2f} <= {budget:.2f}` ms)\n"
        f"- [{'x' if regression_ok else ' '}] p95 regression is <= {args.threshold_percent:.2f}% "
        f"(delta: {delta_percent:.2f}% vs baseline {baseline_p95:.2f} ms)\n"
        f"- [{'x' if release_ok else ' '}] Release performance gate status\n"
    )

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(checklist, encoding="utf-8")

    json_output_path = Path(args.json_output)
    json_output_path.parent.mkdir(parents=True, exist_ok=True)
    json_output_path.write_text(json.dumps(report, indent=2), encoding="utf-8")

    print(json.dumps(report, indent=2))
    return 0 if release_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
