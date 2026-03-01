#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Review baseline proposal against current baseline and flag suspicious drift."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--proposal", required=True)
    parser.add_argument("--threshold-percent", type=float, default=20.0)
    parser.add_argument("--output", required=True)
    parser.add_argument("--markdown-output", required=True)
    parser.add_argument("--fail-on-flag", action="store_true")
    args = parser.parse_args()

    baseline = _load_json(Path(args.baseline))
    proposal = _load_json(Path(args.proposal))

    base = float(baseline["previous_release_p95_ms"])
    proposed = float(proposal["proposed_previous_release_p95_ms"])
    delta_ms = proposed - base
    delta_percent = (delta_ms / base * 100.0) if base else 0.0
    flagged = abs(delta_percent) > args.threshold_percent

    report = {
        "baseline_p95_ms": round(base, 2),
        "proposed_p95_ms": round(proposed, 2),
        "delta_ms": round(delta_ms, 2),
        "delta_percent": round(delta_percent, 2),
        "threshold_percent": args.threshold_percent,
        "flagged_for_manual_review": flagged,
    }

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(report, indent=2), encoding="utf-8")

    md = (
        "# Baseline Proposal Review\n\n"
        f"- Baseline p95: `{base:.2f} ms`\n"
        f"- Proposed p95: `{proposed:.2f} ms`\n"
        f"- Delta: `{delta_ms:.2f} ms` (`{delta_percent:.2f}%`)\n"
        f"- Threshold: `{args.threshold_percent:.2f}%`\n"
        f"- Manual review required: `{'yes' if flagged else 'no'}`\n"
    )
    md_path = Path(args.markdown_output)
    md_path.parent.mkdir(parents=True, exist_ok=True)
    md_path.write_text(md, encoding="utf-8")

    print(json.dumps(report, indent=2))
    if flagged and args.fail_on_flag:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
