# SPDX-License-Identifier: AGPL-3.0-or-later

from __future__ import annotations

import sys
from pathlib import Path


def pytest_sessionstart(session) -> None:
    module_root = Path(__file__).resolve().parents[1]
    if str(module_root) not in sys.path:
        sys.path.insert(0, str(module_root))

