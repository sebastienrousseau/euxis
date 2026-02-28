#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Render performance trend summary markdown from a checklist JSON report."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--report", required=True)
    parser.add_argument("--status-code", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    report = json.loads(Path(args.report).read_text(encoding="utf-8"))
    status = "PASS" if str(args.status_code) == "0" else "WARNING"
    lines = [
        "## Performance Trend (Warning Only)",
        "",
        f"- Status: **{status}** (non-blocking)",
        f"- Current p95: `{report['current_p95_ms']} ms`",
        f"- Q2 Target Budget: `{report['budget_ms']} ms`",
        f"- Baseline p95: `{report['baseline_p95_ms']} ms`",
        f"- Delta vs baseline: `{report['delta_ms']} ms` (`{report['delta_percent']}%`)",
    ]

    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
