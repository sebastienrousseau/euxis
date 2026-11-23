#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Complexity ratchet for the C++ sources under apps/ and libs/.

Ceilings, per function: cyclomatic (McCabe) 10, cognitive 15, Halstead
difficulty 30, length 60 lines; per file: 500 lines. Measured with
lizard (-Ecognitive -Ehalstead) over the tracked sources.

Functions and files already over a ceiling are listed in
complexity_baseline.json next to this script. The gate fails when:

- a function or file not in the baseline goes over a ceiling;
- a baselined one gets worse on a metric that is over its ceiling;
- a baselined one is fixed or gone but still listed: the baseline may
  only shrink, so run with --update and commit the smaller file.

--update rewrites the baseline from the current tree; it refuses while
any regression is present, so it can only drop entries or record
improvements. With no baseline file it writes the first one.

Exit codes:
    0  no regression, baseline up to date
    1  regression, or stale baseline entries
    2  lizard missing or failed
"""
from __future__ import annotations

import argparse
import csv
import json
import shutil
import subprocess
import sys
from pathlib import Path

CEILINGS = {"ccn": 10, "cognitive": 15, "halstead_difficulty": 30, "length": 60}
FILE_CEILING = 500
ROOTS = ("apps", "libs")
SUFFIXES = (".cpp", ".hpp", ".h", ".cc", ".cxx", ".hxx", ".ipp")
BASELINE = Path(__file__).resolve().parent / "complexity_baseline.json"

# lizard --csv columns, with -Ecognitive -Ehalstead appended at the end.
COL_CCN, COL_LENGTH, COL_FILE, COL_LONG_NAME = 1, 4, 6, 8
COL_COGNITIVE, COL_H_DIFFICULTY = 11, 13


def tracked_sources(root: Path) -> list[str]:
    out = subprocess.run(
        ["git", "ls-files", *ROOTS], cwd=root, check=True,
        capture_output=True, text=True,
    ).stdout
    return [p for p in out.splitlines() if p.endswith(SUFFIXES)]


def run_lizard(root: Path, files: list[str]) -> str:
    if shutil.which("lizard") is None:
        raise RuntimeError("lizard not found (pip install lizard==1.26.1)")
    proc = subprocess.run(
        ["lizard", "-l", "cpp", "-Ecognitive", "-Ehalstead", "--csv", *files],
        cwd=root, capture_output=True, text=True, check=False,
    )
    if proc.returncode != 0:
        raise RuntimeError(f"lizard failed: {proc.stderr.strip()}")
    return proc.stdout


def parse_lizard_csv(text: str) -> dict[str, dict[str, float]]:
    """Map 'file::signature' to its metrics; overload clashes keep the max."""
    measured: dict[str, dict[str, float]] = {}
    for row in csv.reader(text.splitlines()):
        key = f"{row[COL_FILE]}::{row[COL_LONG_NAME]}"
        metrics = {
            "ccn": int(row[COL_CCN]),
            "cognitive": int(row[COL_COGNITIVE]),
            "halstead_difficulty": round(float(row[COL_H_DIFFICULTY]), 2),
            "length": int(row[COL_LENGTH]),
        }
        prev = measured.get(key, metrics)
        measured[key] = {k: max(v, prev[k]) for k, v in metrics.items()}
    return measured


def over_ceiling(metrics: dict[str, float]) -> bool:
    return any(metrics[k] > limit for k, limit in CEILINGS.items())


def function_offenders(measured: dict[str, dict[str, float]]
                       ) -> dict[str, dict[str, float]]:
    return {k: m for k, m in measured.items() if over_ceiling(m)}


def file_offenders(root: Path, files: list[str]) -> dict[str, int]:
    lengths = {}
    for rel in files:
        with open(root / rel, encoding="utf-8", errors="replace") as fh:
            lengths[rel] = sum(1 for _ in fh)
    return {f: n for f, n in lengths.items() if n > FILE_CEILING}


def worsened(metrics: dict[str, float], base: dict[str, float]) -> list[str]:
    """Metrics over their ceiling that exceed the baselined value."""
    return [
        f"{k} {base.get(k, 0)} -> {v} (ceiling {CEILINGS[k]})"
        for k, v in metrics.items()
        if v > max(base.get(k, 0), CEILINGS[k])
    ]


def compare_functions(current: dict, baseline: dict) -> tuple[list, list]:
    errors, stale = [], []
    for key, metrics in sorted(current.items()):
        if key not in baseline:
            errors.append(f"new offender: {key} {metrics}")
        elif diffs := worsened(metrics, baseline[key]):
            errors.append(f"worse: {key}: {', '.join(diffs)}")
        elif metrics != baseline[key]:
            stale.append(f"changed: {key} {baseline[key]} -> {metrics}")
    gone = sorted(baseline.keys() - current.keys())
    stale += [f"fixed or gone: {k}" for k in gone]
    return errors, stale


def compare_files(current: dict, baseline: dict) -> tuple[list, list]:
    errors, stale = [], []
    for path, lines in sorted(current.items()):
        if path not in baseline:
            errors.append(f"new file offender: {path} {lines} lines "
                          f"(ceiling {FILE_CEILING})")
        elif lines > baseline[path]:
            errors.append(f"file grew: {path} {baseline[path]} -> {lines} lines")
        elif lines < baseline[path]:
            stale.append(f"file shrank: {path} {baseline[path]} -> {lines} lines")
    gone = sorted(baseline.keys() - current.keys())
    stale += [f"file fixed or gone: {p}" for p in gone]
    return errors, stale


def load_baseline(path: Path) -> dict | None:
    if not path.is_file():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def write_baseline(path: Path, functions: dict, files: dict) -> None:
    data = {
        "tool": "lizard 1.26.1 -l cpp -Ecognitive -Ehalstead",
        "ceilings": {**CEILINGS, "file_lines": FILE_CEILING},
        "functions": dict(sorted(functions.items())),
        "files": dict(sorted(files.items())),
    }
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n",
                    encoding="utf-8")


def evaluate(functions: dict, files: dict, baseline: dict) -> tuple[list, list]:
    f_err, f_stale = compare_functions(functions, baseline.get("functions", {}))
    p_err, p_stale = compare_files(files, baseline.get("files", {}))
    return f_err + p_err, f_stale + p_stale


def report(errors: list, stale: list, update: bool) -> int:
    for line in errors:
        print(f"ERROR: {line}", file=sys.stderr)
    if errors:
        print("\nBring the code under the ceilings; the baseline never grows.",
              file=sys.stderr)
        return 1
    if stale and not update:
        for line in stale:
            print(f"STALE: {line}", file=sys.stderr)
        print("\nThe baseline may only shrink: run "
              "scripts/architecture/check_complexity.py --update and commit it.",
              file=sys.stderr)
        return 1
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--repo-root", type=Path,
                        default=Path(__file__).resolve().parents[2])
    parser.add_argument("--baseline", type=Path, default=BASELINE)
    parser.add_argument("--update", action="store_true",
                        help="rewrite the baseline (refused on any regression)")
    args = parser.parse_args()

    files = tracked_sources(args.repo_root)
    try:
        measured = parse_lizard_csv(run_lizard(args.repo_root, files))
    except RuntimeError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2
    functions = function_offenders(measured)
    long_files = file_offenders(args.repo_root, files)

    baseline = load_baseline(args.baseline)
    if baseline is None and args.update:
        write_baseline(args.baseline, functions, long_files)
        print(f"wrote first baseline: {len(functions)} functions, "
              f"{len(long_files)} files")
        return 0
    errors, stale = evaluate(functions, long_files, baseline or {})
    status = report(errors, stale, args.update)
    if status == 0 and args.update:
        write_baseline(args.baseline, functions, long_files)
    print(f"{len(measured)} functions in {len(files)} files; "
          f"over a ceiling now: {len(functions)} functions, "
          f"{len(long_files)} files")
    return status


if __name__ == "__main__":
    sys.exit(main())
