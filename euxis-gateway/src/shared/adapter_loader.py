# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Adapter loader to avoid hard dependency from API to adapters."""
from __future__ import annotations

import importlib
from typing import Any


def build_adapters() -> dict[str, Any]:
    try:
        registry = importlib.import_module("adapters.registry")
    except ImportError:
        return {}
    return registry.build_adapters()
