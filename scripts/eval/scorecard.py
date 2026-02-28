#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Compute release scorecard from observed metrics."""

from __future__ import annotations

import json
import statistics
import sys
from pathlib import Path


CATEGORIES = (
    "portability",
    "usability",
    "functionality",
    "security",
    "speed",
    "cost",
    "design",
    "accuracy",
)


def _clamp(score: float) -> float:
    return max(0.0, min(10.0, score))


def _read_metrics(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def compute_scores(metrics: dict) -> dict[str, float]:
    availability = float(metrics.get("availability", 99.0))
    p95_latency_ms = float(metrics.get("p95_latency_ms", 1500))
    security_findings_high = float(metrics.get("security_findings_high", 0))
    portability_pass_rate = float(metrics.get("platform_parity_pass_rate", 80))
    task_success = float(metrics.get("task_success_rate", 90))
    ux_score = float(metrics.get("ux_sus_score", 70))
    cost_per_task = float(metrics.get("cost_per_task_usd", 0.05))
    visual_consistency = float(metrics.get("design_consistency_score", 80))

    scores = {
        "portability": _clamp(portability_pass_rate / 10.0),
        "usability": _clamp(ux_score / 10.0),
        "functionality": _clamp(task_success / 10.0),
        "security": _clamp(10.0 - (security_findings_high * 2.0)),
        "speed": _clamp(10.0 - (p95_latency_ms / 300.0)),
        "cost": _clamp(10.0 - (cost_per_task * 100.0)),
        "design": _clamp(visual_consistency / 10.0),
        "accuracy": _clamp((task_success * 0.6 + availability * 0.4) / 10.0),
    }
    return scores


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: scorecard.py <metrics.json> <output.json>", file=sys.stderr)
        return 2

    in_path = Path(sys.argv[1])
    out_path = Path(sys.argv[2])
    metrics = _read_metrics(in_path)
    scores = compute_scores(metrics)
    overall = statistics.mean(scores[c] for c in CATEGORIES)
    release_gate_pass = overall >= 9.0 and all(scores[c] >= 8.0 for c in CATEGORIES)

    report = {
        "categories": scores,
        "overall": round(overall, 2),
        "release_gate_pass": release_gate_pass,
        "thresholds": {"min_category": 8.0, "overall": 9.0},
    }
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))

    return 0 if release_gate_pass else 1


if __name__ == "__main__":
    raise SystemExit(main())
