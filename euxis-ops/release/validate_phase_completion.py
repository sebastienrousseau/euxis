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
        "euxis-ops/architecture/check_boundaries.py",
        "euxis-cpp/euxis-core-cpp/src/resilience.cpp",
        "euxis-cpp/euxis-core-cpp/src/router.cpp",
        "euxis-cpp/euxis-core-cpp/src/swarm.cpp",
    ],
    "phase2": [
        ".github/workflows/cross-platform-ci.yml",
        "euxis-cpp/euxis-core-cpp/tests/test_resilience.cpp",
        "euxis-cpp/euxis-core-cpp/tests/test_router.cpp",
        "euxis-cpp/euxis-core-cpp/tests/test_swarm.cpp",
    ],
    "phase3": [
        ".github/workflows/supply-chain.yml",
        "euxis-ops/supply_chain/verify_signed_artifacts.sh",
    ],
    "phase4": [
        "euxis-cpp/euxis-core-cpp/src/resilience.cpp",
        "euxis-ops/perf/check_perf_budget.py",
        "euxis-ops/perf/validate_perf_governance.py",
    ],
    "phase5": [
        "euxis-ops/eval/scorecard.py",
        "euxis-ops/release/generate_checklist.py",
        "euxis-ops/release/aggregate_release_evidence.py",
        "euxis-ops/release/validate_release_evidence.py",
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
