#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Import ClawHub/OpenClaw skill bundles into Euxis bridge registry."""

from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
PACKAGE_SRC = REPO_ROOT / "euxis-bridge" / "src"
if str(PACKAGE_SRC) not in sys.path:
    sys.path.insert(0, str(PACKAGE_SRC))

from euxis_bridge.cli import main


if __name__ == "__main__":
    raise SystemExit(main())
