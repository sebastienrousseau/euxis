#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Validate package-level performance/resource governance for all packages."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


METRICS = ("p95_ms", "memory_mb", "cpu_pct")


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _stage_value(entry: dict, stage: str, metric: str) -> float:
    try:
        value = float(entry[stage][metric])
    except (KeyError, TypeError, ValueError) as exc:
        raise ValueError(f"missing/invalid '{metric}' for stage '{stage}'") from exc
    if value <= 0:
        raise ValueError(f"non-positive '{metric}' for stage '{stage}'")
    return value


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--standards", required=True, help="Package standards JSON")
    parser.add_argument("--policy", required=True, help="Package resource policy JSON")
    args = parser.parse_args()

    standards = _load_json(Path(args.standards))
    policy = _load_json(Path(args.policy))

    stages = policy.get("stages", [])
    expected_stages = ["current", "target_q2_2026", "target_q4_2026"]
    if stages != expected_stages:
        print(f"invalid stages sequence: {stages}", file=sys.stderr)
        return 1

    default_cpu_ms = policy.get("default_cpu_ms_budget", {})
    try:
        cpu_current = float(default_cpu_ms["current"])
        cpu_q2 = float(default_cpu_ms["target_q2_2026"])
        cpu_q4 = float(default_cpu_ms["target_q4_2026"])
        if not (cpu_current > 0 and cpu_q2 > 0 and cpu_q4 > 0):
            raise ValueError("default cpu_ms budgets must be positive")
        if not (cpu_current >= cpu_q2 >= cpu_q4):
            raise ValueError("default cpu_ms budgets must ratchet monotonically")
    except Exception as exc:
        print(f"invalid default_cpu_ms_budget: {exc}", file=sys.stderr)
        return 1

    package_names = [pkg.get("name") for pkg in standards.get("packages", [])]
    policies = policy.get("packages", {})

    errors: list[str] = []
    for name in package_names:
        if name not in policies:
            errors.append(f"missing package policy for '{name}'")
            continue
        entry = policies[name]
        for metric in METRICS:
            try:
                current = _stage_value(entry, "current", metric)
                q2 = _stage_value(entry, "target_q2_2026", metric)
                q4 = _stage_value(entry, "target_q4_2026", metric)
            except ValueError as exc:
                errors.append(f"{name}: {exc}")
                continue
            if not (current >= q2 >= q4):
                errors.append(
                    f"{name}: non-monotonic {metric} targets (current={current}, q2={q2}, q4={q4})"
                )

    unknown = sorted(set(policies) - set(package_names))
    for name in unknown:
        errors.append(f"policy contains unknown package '{name}'")

    if errors:
        print("Package resource governance validation failed.", file=sys.stderr)
        for err in errors:
            print(f"- {err}", file=sys.stderr)
        return 1

    print("Package resource governance validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
