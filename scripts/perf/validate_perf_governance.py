#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Validate performance policy and release baseline governance rules."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


SEMVER_TAG_RE = re.compile(r"^v\d+\.\d+\.\d+$")
REQUIRED_STAGES = ("current", "target_q2_2026", "target_q4_2026")


def _err(message: str) -> int:
    print(f"Governance validation failed: {message}", file=sys.stderr)
    return 1


def _validate_policy(policy_path: Path) -> tuple[bool, dict]:
    try:
        policy = json.loads(policy_path.read_text(encoding="utf-8"))
    except Exception as exc:
        _err(f"Cannot parse policy JSON {policy_path}: {exc}")
        return False, {}

    if not isinstance(policy.get("policy_version"), str):
        return False, {"error": "policy_version must be a string"}

    stages = policy.get("stages")
    if not isinstance(stages, dict):
        return False, {"error": "stages must be an object"}

    budgets: dict[str, float] = {}
    for stage in REQUIRED_STAGES:
        if stage not in stages:
            return False, {"error": f"missing stage '{stage}'"}
        raw_budget = stages[stage].get("p95_budget_ms")
        if not isinstance(raw_budget, (int, float)):
            return False, {"error": f"{stage}.p95_budget_ms must be numeric"}
        if raw_budget <= 0:
            return False, {"error": f"{stage}.p95_budget_ms must be > 0"}
        budgets[stage] = float(raw_budget)

    # Ratcheting rule: future stages must be stricter or equal.
    if not (budgets["current"] >= budgets["target_q2_2026"] >= budgets["target_q4_2026"]):
        return False, {"error": "budgets must be monotonic: current >= q2 >= q4"}

    return True, {"budgets": budgets}


def _validate_baseline(baseline_path: Path) -> tuple[bool, dict]:
    try:
        baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
    except Exception as exc:
        _err(f"Cannot parse baseline JSON {baseline_path}: {exc}")
        return False, {}

    release = baseline.get("previous_release")
    if not isinstance(release, str) or not SEMVER_TAG_RE.match(release):
        return False, {"error": "previous_release must match vX.Y.Z"}

    p95_value = baseline.get("previous_release_p95_ms")
    if not isinstance(p95_value, (int, float)) or p95_value <= 0:
        return False, {"error": "previous_release_p95_ms must be > 0 numeric"}

    return True, {"previous_release": release, "previous_release_p95_ms": float(p95_value)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--policy", required=True)
    parser.add_argument("--baseline", required=True)
    args = parser.parse_args()

    policy_ok, policy_info = _validate_policy(Path(args.policy))
    if not policy_ok:
        return _err(policy_info.get("error", "unknown policy error"))

    baseline_ok, baseline_info = _validate_baseline(Path(args.baseline))
    if not baseline_ok:
        return _err(baseline_info.get("error", "unknown baseline error"))

    print("Performance governance validation passed.")
    print(
        json.dumps(
            {
                "policy": policy_info,
                "baseline": baseline_info,
            },
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
