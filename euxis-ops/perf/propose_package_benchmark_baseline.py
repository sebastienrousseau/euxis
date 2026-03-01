#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Propose next package benchmark baseline from current measurements."""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _pct_delta(current: float, baseline: float) -> float:
    if baseline == 0:
        return 0.0
    return ((current - baseline) / baseline) * 100.0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--current", required=True)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--previous-release", required=True)
    parser.add_argument("--next-release", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    current = _load_json(Path(args.current))
    baseline = _load_json(Path(args.baseline))

    current_pkgs = current.get("packages", {})
    baseline_pkgs = baseline.get("packages", {})
    deltas: dict[str, dict[str, float]] = {}

    for package in sorted(set(current_pkgs) & set(baseline_pkgs)):
        c = current_pkgs[package]
        b = baseline_pkgs[package]
        deltas[package] = {
            "wall_p95_delta_percent": round(
                _pct_delta(float(c["wall_ms"]["p95"]), float(b["wall_ms"]["p95"])), 3
            ),
            "cpu_ms_p95_delta_percent": round(
                _pct_delta(float(c["cpu_ms"]["p95"]), float(b["cpu_ms"]["p95"])), 3
            ),
            "memory_p95_delta_percent": round(
                _pct_delta(float(c["memory_mb"]["p95"]), float(b["memory_mb"]["p95"])), 3
            ),
        }

    proposal = {
        "previous_release": args.previous_release,
        "next_release": args.next_release,
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "packages": current_pkgs,
        "deltas_vs_current_baseline": deltas,
    }

    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(proposal, indent=2), encoding="utf-8")
    print(json.dumps(proposal, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
