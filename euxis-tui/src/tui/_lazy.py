# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Lazy import utilities for performance optimization.

Defers heavy framework imports until actually needed, reducing cold start time.
Target: cold start <=20ms
"""

from __future__ import annotations

import importlib
import sys
from types import ModuleType
from typing import Any


class LazyModule(ModuleType):
    """A module that defers importing until first attribute access."""

    def __init__(self, name: str, package: str | None = None) -> None:
        super().__init__(name)
        self._name = name
        self._package = package
        self._module: ModuleType | None = None

    def _load(self) -> ModuleType:
        if self._module is None:
            self._module = importlib.import_module(self._name, self._package)
        return self._module

    def __getattr__(self, name: str) -> Any:
        return getattr(self._load(), name)

    def __dir__(self) -> list[str]:
        return dir(self._load())


class LazyImporter:
    """Context manager for lazy imports."""

    def __init__(self) -> None:
        self._lazy_modules: dict[str, LazyModule] = {}

    def lazy_import(self, module_name: str) -> LazyModule:
        """Register a module for lazy loading."""
        if module_name not in self._lazy_modules:
            self._lazy_modules[module_name] = LazyModule(module_name)
        return self._lazy_modules[module_name]


# Pre-defined lazy imports for heavy dependencies
_importer = LazyImporter()

# Textual framework (heavy - ~10MB)
textual_app = _importer.lazy_import("textual.app")
textual_binding = _importer.lazy_import("textual.binding")
textual_containers = _importer.lazy_import("textual.containers")
textual_screen = _importer.lazy_import("textual.screen")
textual_widgets = _importer.lazy_import("textual.widgets")

# Rich library (heavy - ~5MB)
rich_console = _importer.lazy_import("rich.console")
rich_table = _importer.lazy_import("rich.table")


def lazy_import(module_name: str) -> LazyModule:
    """Public API for lazy importing."""
    return _importer.lazy_import(module_name)
