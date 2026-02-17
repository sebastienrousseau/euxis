# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Pytest configuration for module imports."""
from __future__ import annotations
import sys
from pathlib import Path
def pytest_sessionstart(session) -> None:
    module_root = Path(__file__).resolve().parents[1]
    src_dir = module_root / "src"
    if src_dir.exists() and str(src_dir) not in sys.path:
        sys.path.insert(0, str(src_dir))
    package_dir = src_dir / "metrics"
    if package_dir.exists() and str(package_dir) not in sys.path:
        sys.path.insert(0, str(package_dir))
