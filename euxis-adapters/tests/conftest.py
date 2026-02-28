# SPDX-License-Identifier: AGPL-3.0-or-later

from __future__ import annotations

import sys
import types
from pathlib import Path


def pytest_sessionstart(session) -> None:
    module_root = Path(__file__).resolve().parents[1]
    src_dir = module_root / "src"
    if src_dir.exists() and str(src_dir) not in sys.path:
        sys.path.insert(0, str(src_dir))

    # Provide a lightweight shared.gateway_utils shim for adapter unit tests.
    if "shared.gateway_utils" not in sys.modules:
        shared_mod = types.ModuleType("shared")
        gateway_utils = types.ModuleType("shared.gateway_utils")
        gateway_utils.persist_message = lambda *_args, **_kwargs: None
        gateway_utils.timestamp = lambda: "2026-01-01T00:00:00Z"
        shared_mod.gateway_utils = gateway_utils
        sys.modules["shared"] = shared_mod
        sys.modules["shared.gateway_utils"] = gateway_utils

