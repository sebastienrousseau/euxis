#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Review package benchmark baseline proposal and flag suspicious drift."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _canonical_sha256(payload: dict) -> str:
    canonical = json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(canonical).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--proposal", required=True)
    parser.add_argument("--threshold-percent", type=float, default=25.0)
    parser.add_argument("--output", required=True)
    parser.add_argument("--markdown-output", required=True)
    parser.add_argument("--fail-on-flag", action="store_true")
    args = parser.parse_args()

    proposal = _load_json(Path(args.proposal))
    proposal_sha256 = _canonical_sha256(proposal)
    deltas = proposal.get("deltas_vs_current_baseline", {})
    flagged_packages: dict[str, list[str]] = {}

    for package, metrics in deltas.items():
        metric_flags: list[str] = []
        for metric_name, delta in metrics.items():
            if abs(float(delta)) > args.threshold_percent:
                metric_flags.append(f"{metric_name}={delta:.3f}%")
        if metric_flags:
            flagged_packages[package] = metric_flags

    report = {
        "review_schema_version": "2026.1",
        "previous_release": proposal.get("previous_release"),
        "next_release": proposal.get("next_release"),
        "proposal_sha256": proposal_sha256,
        "threshold_percent": args.threshold_percent,
        "flagged_for_manual_review": bool(flagged_packages),
        "flagged_packages": flagged_packages,
    }

    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, indent=2), encoding="utf-8")

    md_lines = [
        "# Package Baseline Proposal Review",
        "",
        f"- Previous release: `{proposal.get('previous_release')}`",
        f"- Next release: `{proposal.get('next_release')}`",
        f"- Proposal SHA-256: `{proposal_sha256}`",
        f"- Threshold: `{args.threshold_percent:.2f}%`",
        f"- Manual review required: `{'yes' if flagged_packages else 'no'}`",
        "",
    ]
    if flagged_packages:
        md_lines.append("## Flagged Packages")
        for package, metrics in sorted(flagged_packages.items()):
            md_lines.append(f"- `{package}`: {', '.join(metrics)}")
    else:
        md_lines.append("No suspicious drift detected.")

    md = Path(args.markdown_output)
    md.parent.mkdir(parents=True, exist_ok=True)
    md.write_text("\n".join(md_lines) + "\n", encoding="utf-8")

    print(json.dumps(report, indent=2))
    if flagged_packages and args.fail_on_flag:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
