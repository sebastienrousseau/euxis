#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Propose next release baseline from current runtime metrics."""

from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

PERF_DIR = Path(__file__).resolve().parent
if str(PERF_DIR) not in sys.path:
    sys.path.insert(0, str(PERF_DIR))

from perf_utils import extract_latencies_ms, p95


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--metrics", required=True)
    parser.add_argument("--previous-release", required=True)
    parser.add_argument("--next-release", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    metrics_path = Path(args.metrics)
    output_path = Path(args.output)
    current_p95 = p95(extract_latencies_ms(metrics_path))
    if current_p95 == float("inf"):
        print("Unable to compute p95 from metrics", file=sys.stderr)
        return 1

    proposal = {
        "previous_release": args.previous_release,
        "next_release": args.next_release,
        "proposed_previous_release_p95_ms": round(current_p95, 2),
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(proposal, indent=2), encoding="utf-8")
    print(json.dumps(proposal, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
