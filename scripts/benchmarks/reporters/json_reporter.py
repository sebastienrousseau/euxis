"""JSON reporter for benchmark results."""

from __future__ import annotations

import json
from pathlib import Path

from euxis_ops.benchmarks.runner import SuiteReport


def export_json(reports: list[SuiteReport], output_path: Path) -> None:
    """Export benchmark results as JSON."""
    output_path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "all_passed": all(r.passed for r in reports),
        "suites": [r.to_dict() for r in reports],
    }
    output_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
