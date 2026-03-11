#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Validate aggregated release evidence against quality policy."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def _fail(message: str) -> int:
    print(f"Release evidence validation failed: {message}", file=sys.stderr)
    return 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence", required=True)
    parser.add_argument("--min-overall", type=float, default=9.0)
    parser.add_argument("--min-category", type=float, default=8.0)
    parser.add_argument("--require-signature-verified", action="store_true")
    args = parser.parse_args()

    evidence_path = Path(args.evidence)
    if not evidence_path.exists():
        return _fail(f"missing evidence file: {evidence_path}")

    evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
    scorecard = evidence.get("scorecard", {})
    checklist = evidence.get("release_checklist", {})
    baseline_review = evidence.get("baseline_review", {})
    signature = evidence.get("signature_verification", {})

    overall = scorecard.get("overall")
    if not isinstance(overall, (int, float)):
        return _fail("scorecard.overall must be numeric")
    if float(overall) < args.min_overall:
        return _fail(f"scorecard overall {overall} < required {args.min_overall}")

    categories = scorecard.get("categories", {})
    if not isinstance(categories, dict) or not categories:
        return _fail("scorecard.categories missing")
    for key, value in categories.items():
        if not isinstance(value, (int, float)):
            return _fail(f"category {key} score is not numeric")
        if float(value) < args.min_category:
            return _fail(f"category {key} score {value} < required {args.min_category}")

    if checklist.get("release_ok") is not True:
        return _fail("release_checklist.release_ok must be true")

    if baseline_review.get("flagged_for_manual_review") is True:
        return _fail("baseline proposal flagged for manual review")

    status = str(signature.get("status", "unknown")).lower()
    if args.require_signature_verified and status != "verified":
        return _fail(f"signature status is '{status}', expected 'verified'")

    print("Release evidence validation passed.")
    print(
        json.dumps(
            {
                "overall": overall,
                "min_overall": args.min_overall,
                "min_category": args.min_category,
                "signature_status": status,
                "require_signature_verified": args.require_signature_verified,
            },
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
