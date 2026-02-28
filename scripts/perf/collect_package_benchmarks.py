#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Collect package benchmark metrics across all workspace packages."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _percentile(values: list[float], pct: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    idx = int(round((pct / 100.0) * (len(ordered) - 1)))
    return float(ordered[max(0, min(idx, len(ordered) - 1))])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--standards", required=True)
    parser.add_argument("--iterations", type=int, default=3)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    root = Path(args.repo_root).resolve()
    standards = _load_json(root / args.standards)
    worker = root / "scripts" / "perf" / "package_probe_worker.py"

    results: dict[str, dict[str, object]] = {}
    failures: list[str] = []

    for pkg in standards.get("packages", []):
        name = pkg["name"]
        kind = pkg.get("kind", "unknown")
        pkg_path = root / pkg.get("path", "")
        wall: list[float] = []
        cpu_ms: list[float] = []
        cpu_pct: list[float] = []
        rss_mb: list[float] = []
        last_probe: dict[str, object] = {}

        for _ in range(max(1, args.iterations)):
            proc = subprocess.run(
                [
                    sys.executable,
                    str(worker),
                    "--package-path",
                    str(pkg_path),
                    "--kind",
                    kind,
                ],
                cwd=root,
                text=True,
                capture_output=True,
                check=False,
            )
            if proc.returncode != 0:
                failures.append(f"{name}: probe failed: {proc.stderr.strip()}")
                continue
            payload = json.loads(proc.stdout.strip())
            if "error" in payload:
                failures.append(f"{name}: {payload['error']}")
                continue

            wall.append(float(payload["wall_ms"]))
            cpu_ms.append(float(payload["cpu_ms"]))
            cpu_pct.append(float(payload["cpu_pct"]))
            rss_mb.append(float(payload["max_rss_kb"]) / 1024.0)
            last_probe = payload.get("probe", {})

        if not wall:
            failures.append(f"{name}: no successful probe samples")
            continue

        results[name] = {
            "samples": len(wall),
            "wall_ms": {
                "p50": round(_percentile(wall, 50), 3),
                "p95": round(_percentile(wall, 95), 3),
                "max": round(max(wall), 3),
            },
            "cpu_pct": {
                "p50": round(_percentile(cpu_pct, 50), 3),
                "p95": round(_percentile(cpu_pct, 95), 3),
                "max": round(max(cpu_pct), 3),
            },
            "cpu_ms": {
                "p50": round(_percentile(cpu_ms, 50), 3),
                "p95": round(_percentile(cpu_ms, 95), 3),
                "max": round(max(cpu_ms), 3),
            },
            "memory_mb": {
                "p50": round(_percentile(rss_mb, 50), 3),
                "p95": round(_percentile(rss_mb, 95), 3),
                "max": round(max(rss_mb), 3),
            },
            "probe": last_probe,
        }

    summary = {
        "status": "ok" if not failures else "failed",
        "iterations": max(1, args.iterations),
        "packages": results,
        "failures": failures,
    }

    out = root / args.output
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    if failures:
        print("Package benchmark collection completed with failures:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1

    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
