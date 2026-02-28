# SPDX-License-Identifier: AGPL-3.0-or-later

from __future__ import annotations

import sys
from pathlib import Path


def pytest_sessionstart(session) -> None:
    module_root = Path(__file__).resolve().parents[1]
    src_dir = module_root / "src"
    if src_dir.exists() and str(src_dir) not in sys.path:
        sys.path.insert(0, str(src_dir))

