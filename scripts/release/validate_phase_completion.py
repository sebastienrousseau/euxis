#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Validate that phase 1-5 implementation assets are present."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


REQUIRED_FILES = {
    "phase1": [
        "scripts/architecture/check_boundaries.py",
        "euxis-core/src/euxis_core/contracts/ports.py",
        "euxis-core/src/euxis_core/platform/adapters.py",
        "euxis-core/src/euxis_core/runtime/gateway_ws.py",
    ],
    "phase2": [
        ".github/workflows/cross-platform-ci.yml",
        "euxis-core/tests/unit/test_platform_adapter.py",
        "euxis-core/tests/unit/test_contracts.py",
    ],
    "phase3": [
        ".github/workflows/supply-chain.yml",
        "scripts/supply_chain/verify_signed_artifacts.sh",
    ],
    "phase4": [
        "euxis-core/src/euxis_core/resilience.py",
        "euxis-core/src/euxis_core/runtime/concurrency.py",
        "scripts/perf/check_perf_budget.py",
        "scripts/perf/validate_perf_governance.py",
    ],
    "phase5": [
        "scripts/eval/scorecard.py",
        "scripts/release/generate_checklist.py",
        "scripts/release/aggregate_release_evidence.py",
        "scripts/release/validate_release_evidence.py",
    ],
}


def _existing(root: Path, path: str) -> bool:
    return (root / path).exists()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=".", help="Repository root path")
    parser.add_argument("--json-output", default="", help="Optional JSON report output path")
    args = parser.parse_args()

    root = Path(args.repo_root).resolve()
    missing_by_phase: dict[str, list[str]] = {}

    for phase, files in REQUIRED_FILES.items():
        missing = [f for f in files if not _existing(root, f)]
        if missing:
            missing_by_phase[phase] = missing

    report = {
        "status": "ok" if not missing_by_phase else "failed",
        "missing": missing_by_phase,
    }

    if args.json_output:
        out = Path(args.json_output)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(report, indent=2), encoding="utf-8")

    if missing_by_phase:
        print("Phase completion validation failed.", file=sys.stderr)
        for phase, missing in sorted(missing_by_phase.items()):
            print(f"- {phase} missing:", file=sys.stderr)
            for path in missing:
                print(f"  - {path}", file=sys.stderr)
        return 1

    print("Phase completion validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
