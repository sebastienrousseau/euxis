#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Check p95 latency budget from a JSONL metrics file."""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

from perf_utils import extract_latencies_ms, p95

def _load_budget(policy_path: Path, stage: str) -> float:
    policy = json.loads(policy_path.read_text(encoding="utf-8"))
    stages = policy.get("stages", {})
    if stage not in stages:
        raise KeyError(f"Stage '{stage}' not found in policy {policy_path}")
    budget = stages[stage].get("p95_budget_ms")
    if not isinstance(budget, (int, float)):
        raise ValueError(f"Invalid p95_budget_ms for stage '{stage}' in {policy_path}")
    return float(budget)


def main() -> int:
    if len(sys.argv) not in {3, 4}:
        print(
            "Usage: check_perf_budget.py <metrics.jsonl> <p95_budget_ms>|<policy.json> [stage]",
            file=sys.stderr,
        )
        return 2

    metrics_path = Path(sys.argv[1])
    budget_arg = sys.argv[2]
    stage = sys.argv[3] if len(sys.argv) == 4 else "current"

    if budget_arg.endswith(".json"):
        p95_budget_ms = _load_budget(Path(budget_arg), stage)
    else:
        p95_budget_ms = float(budget_arg)

    if not metrics_path.exists():
        print(f"Metrics file not found: {metrics_path}", file=sys.stderr)
        return 2

    latencies = extract_latencies_ms(metrics_path)
    value = p95(latencies)
    if value == math.inf:
        print("No usable latency_ms values were found.", file=sys.stderr)
        return 1

    print(f"p95 latency: {value:.2f} ms (budget: {p95_budget_ms:.2f} ms)")
    if value > p95_budget_ms:
        print("Performance gate failed.", file=sys.stderr)
        return 1
    print("Performance gate passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
