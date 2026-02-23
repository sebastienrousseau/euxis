# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Core infrastructure for ETX."""

from pathlib import Path
import os


def _resolve_euxis_home() -> Path:
    """Resolve the EUXIS_HOME directory.

    Checks in order:
    1. EUXIS_HOME environment variable
    2. Walk up from cwd looking for euxis-core/agents/registry.json
    3. Fall back to ~/.euxis
    """
    env_home = os.environ.get("EUXIS_HOME")
    if env_home:
        return Path(env_home)

    cwd = Path.cwd()
    for candidate in [cwd, *cwd.parents]:
        # New structure: euxis-core/agents/registry.json
        if (candidate / "euxis-core" / "agents" / "registry.json").exists():
            return candidate

    return Path.home() / ".euxis"


EUXIS_HOME = _resolve_euxis_home()
