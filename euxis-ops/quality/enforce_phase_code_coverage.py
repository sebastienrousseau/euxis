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

TESTS = [
    "euxis-core/tests/unit/test_contracts.py",
    "euxis-core/tests/unit/test_platform_adapter.py",
    "euxis-core/tests/unit/test_resilience.py",
    "euxis-core/tests/unit/test_swarm_router.py",
    "euxis-core/tests/unit/test_extism_runner.py",
    "euxis-core/tests/unit/test_runtime_adapters.py",
    "euxis-core/tests/unit/test_quality_scripts.py",
]

COVERAGE_TARGETS = [
    "euxis_core.contracts",
    "euxis_core.platform.adapters",
    "euxis_core.runtime.async_bridge",
    "euxis_core.runtime.concurrency",
    "euxis_core.runtime.gateway_ws",
    "euxis_core.runtime.wasm_adapter",
    "euxis_core.resilience",
]


def main() -> int:
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
