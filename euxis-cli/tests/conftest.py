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
    extra_path = module_root / 'bin'
    if extra_path.exists() and str(extra_path) not in sys.path:
        sys.path.insert(0, str(extra_path))
