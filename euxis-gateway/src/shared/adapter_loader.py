# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Adapter loader to avoid hard dependency from API to adapters."""
from __future__ import annotations

import importlib
from typing import Any


import inspect

def build_adapters(config: dict[str, Any] | None = None, on_message: Any = None) -> dict[str, Any]:
    try:
        registry = importlib.import_module("adapters.registry")
    except ImportError:
        return {}
        
    sig = inspect.signature(registry.build_adapters)
    if not sig.parameters:
        return registry.build_adapters()
    return registry.build_adapters(config or {}, on_message=on_message)
