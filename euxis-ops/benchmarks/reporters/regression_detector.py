"""Regression detector comparing benchmark results against a baseline."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass(frozen=True, slots=True)
class RegressionFinding:
    benchmark: str
    suite: str
    baseline_ms: float
    current_ms: float
    regression_pct: float


def detect_regressions(
    current_path: Path,
    baseline_path: Path,
    threshold_pct: float = 20.0,
) -> list[RegressionFinding]:
    """Compare current benchmarks against baseline, flag regressions."""
    if not baseline_path.exists():
        return []

    current = json.loads(current_path.read_text(encoding="utf-8"))
    baseline = json.loads(baseline_path.read_text(encoding="utf-8"))

    baseline_map: dict[str, float] = {}
    for suite in baseline.get("suites", baseline.get("reports", [])):
        for r in suite.get("results", []):
            key = f"{suite.get('suite', '')}::{r['name']}"
            baseline_map[key] = r["duration_ms"]

    findings: list[RegressionFinding] = []
    for suite in current.get("suites", current.get("reports", [])):
        for r in suite.get("results", []):
            key = f"{suite.get('suite', '')}::{r['name']}"
            if key in baseline_map and baseline_map[key] > 0:
                pct = ((r["duration_ms"] - baseline_map[key]) / baseline_map[key]) * 100
                if pct > threshold_pct:
                    findings.append(RegressionFinding(
                        benchmark=r["name"],
                        suite=suite.get("suite", ""),
                        baseline_ms=baseline_map[key],
                        current_ms=r["duration_ms"],
                        regression_pct=round(pct, 2),
                    ))

    return findings
