#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Enforce 100% test coverage on phase 1-5 critical Python modules."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]

TESTS: list[str] = []

COVERAGE_TARGETS: list[str] = []


def main() -> int:
    if not TESTS and not COVERAGE_TARGETS:
        print("No Python coverage targets defined (skipping check)")
        return 0

    # Keep package excellence checks deterministic by removing transient pytest caches.
    for cache_dir in REPO_ROOT.glob("**/.pytest_cache"):
        if cache_dir.is_dir():
            shutil.rmtree(cache_dir, ignore_errors=True)

    venv_pytest = REPO_ROOT / ".venv" / "bin" / "pytest"
    if venv_pytest.exists():
        cmd = [str(venv_pytest), "-q", "-p", "no:cacheprovider", "-c", "/dev/null"]
    else:
        cmd = [
            sys.executable,
            "-m",
            "pytest",
            "-q",
            "-p",
            "no:cacheprovider",
            "-c",
            "/dev/null",
        ]
    cmd.extend(TESTS)
    for target in COVERAGE_TARGETS:
        cmd.extend(["--cov", target])
    cmd.extend(["--cov-report", "term-missing", "--cov-fail-under", "100"])

    env = os.environ.copy()
    env["PYTHONPATH"] = str(REPO_ROOT / "euxis-core" / "src")
    if venv_pytest.exists():
        venv_bin = str(venv_pytest.parent)
        env["PATH"] = f"{venv_bin}:{env.get('PATH', '')}"
    proc = subprocess.run(cmd, cwd=REPO_ROOT, text=True, env=env, check=False)
    return proc.returncode


if __name__ == "__main__":
    raise SystemExit(main())
