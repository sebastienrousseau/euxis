#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Enforce 100% documentation coverage for phase 1-5 implementation artifacts."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


DEFAULT_ARTIFACTS = [
    "euxis-ops/architecture/check_boundaries.py",
    "libs/core/src/resilience.cpp",
    "libs/core/src/router.cpp",
    "libs/core/src/swarm.cpp",
    "libs/core/tests/test_resilience.cpp",
    "libs/core/tests/test_router.cpp",
    "libs/core/tests/test_swarm.cpp",
    ".github/workflows/supply-chain.yml",
    "euxis-ops/supply_chain/verify_signed_artifacts.sh",
    "euxis-ops/perf/check_perf_budget.py",
    "euxis-ops/perf/validate_perf_governance.py",
    "euxis-ops/eval/scorecard.py",
    "euxis-ops/release/generate_checklist.py",
    "euxis-ops/release/aggregate_release_evidence.py",
    "euxis-ops/release/validate_release_evidence.py",
    "euxis-ops/release/validate_phase_completion.py",
    "euxis-ops/quality/enforce_phase_code_coverage.py",
    "euxis-ops/quality/validate_phase_docs_coverage.py",
]

DEFAULT_DOCS = [
    "docs/architecture/core-platform-separation-2026.md",
]


def _load_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=".", help="Repository root")
    parser.add_argument("--json-output", default="", help="Optional JSON report path")
    args = parser.parse_args()

    root = Path(args.repo_root).resolve()

    docs_text = "\n".join(_load_text(root / doc) for doc in DEFAULT_DOCS if (root / doc).exists())
    missing: list[str] = [artifact for artifact in DEFAULT_ARTIFACTS if artifact not in docs_text]
    covered = len(DEFAULT_ARTIFACTS) - len(missing)
    coverage_percent = 100.0 if not DEFAULT_ARTIFACTS else (covered / len(DEFAULT_ARTIFACTS)) * 100.0

    report = {
        "artifacts_total": len(DEFAULT_ARTIFACTS),
        "covered": covered,
        "missing": missing,
        "coverage_percent": round(coverage_percent, 2),
        "status": "ok" if not missing else "failed",
    }

    if args.json_output:
        out_path = root / args.json_output
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text(json.dumps(report, indent=2), encoding="utf-8")

    if missing:
        print("Documentation coverage validation failed.", file=sys.stderr)
        print(f"Coverage: {coverage_percent:.2f}%", file=sys.stderr)
        for artifact in missing:
            print(f" - Missing docs reference: {artifact}", file=sys.stderr)
        return 1

    print("Documentation coverage validation passed (100%).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
