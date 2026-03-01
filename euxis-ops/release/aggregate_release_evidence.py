#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Aggregate release gate artifacts into one evidence bundle."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def _load_optional(path: str | None) -> dict | None:
    if not path:
        return None
    p = Path(path)
    if not p.exists():
        return None
    return json.loads(p.read_text(encoding="utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scorecard", required=True)
    parser.add_argument("--checklist", required=True)
    parser.add_argument("--proposal", required=True)
    parser.add_argument("--baseline-review", required=True)
    parser.add_argument("--signature-status", default="")
    parser.add_argument("--output", required=True)
    parser.add_argument("--markdown-output", required=True)
    args = parser.parse_args()

    scorecard = _load_optional(args.scorecard) or {}
    checklist = _load_optional(args.checklist) or {}
    proposal = _load_optional(args.proposal) or {}
    baseline_review = _load_optional(args.baseline_review) or {}
    signature_status = _load_optional(args.signature_status)
    if signature_status is None:
        signature_status = {"status": "unknown", "reason": "signature status artifact not provided"}

    evidence = {
        "scorecard": scorecard,
        "release_checklist": checklist,
        "baseline_proposal": proposal,
        "baseline_review": baseline_review,
        "signature_verification": signature_status,
    }

    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(evidence, indent=2), encoding="utf-8")

    summary = (
        "# Release Evidence Bundle\n\n"
        f"- Scorecard overall: `{scorecard.get('overall', 'n/a')}`\n"
        f"- Scorecard gate: `{scorecard.get('release_gate_pass', 'n/a')}`\n"
        f"- Checklist release_ok: `{checklist.get('release_ok', 'n/a')}`\n"
        f"- Baseline review flagged: `{baseline_review.get('flagged_for_manual_review', 'n/a')}`\n"
        f"- Signature verification status: `{signature_status.get('status', 'unknown')}`\n"
    )
    md_path = Path(args.markdown_output)
    md_path.parent.mkdir(parents=True, exist_ok=True)
    md_path.write_text(summary, encoding="utf-8")
    print(summary)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
