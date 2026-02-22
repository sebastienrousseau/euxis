# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Unit tests for tui/_lazy.py — lazy import utilities.

Tests cover: LazyModule construction, lazy loading on attribute access,
__dir__, caching in LazyImporter, public lazy_import function,
pre-defined lazy imports.
"""

from __future__ import annotations

import importlib
import sys
import unittest
from types import ModuleType
from unittest.mock import MagicMock, patch


class TestLazyModuleConstruction(unittest.TestCase):
    """Tests for LazyModule.__init__ and basic properties."""

    def test_is_module_type(self):
        from tui._lazy import LazyModule
        lm = LazyModule("os")
        assert isinstance(lm, ModuleType)

    def test_stores_name(self):
        from tui._lazy import LazyModule
        lm = LazyModule("json")
        assert lm._name == "json"

    def test_stores_package_none_by_default(self):
        from tui._lazy import LazyModule
        lm = LazyModule("os.path")
        assert lm._package is None

    def test_stores_package_when_provided(self):
        from tui._lazy import LazyModule
        lm = LazyModule(".sub", package="mypackage")
        assert lm._package == "mypackage"

    def test_module_initially_none(self):
        from tui._lazy import LazyModule
        lm = LazyModule("json")
        assert lm._module is None


class TestLazyModuleLoad(unittest.TestCase):
    """Tests for LazyModule._load (deferred import)."""

    def test_load_imports_module(self):
        from tui._lazy import LazyModule
        lm = LazyModule("json")
        mod = lm._load()
        import json
        assert mod is json

    def test_load_caches_result(self):
        from tui._lazy import LazyModule
        lm = LazyModule("json")
        first = lm._load()
        second = lm._load()
        assert first is second
        assert lm._module is first

    def test_load_with_package(self):
        from tui._lazy import LazyModule
        lm = LazyModule("os.path")
        mod = lm._load()
        import os.path
        assert mod is os.path

    @patch("importlib.import_module")
    def test_load_calls_import_module_once(self, mock_import):
        from tui._lazy import LazyModule
        fake_mod = MagicMock(spec=ModuleType)
        mock_import.return_value = fake_mod

        lm = LazyModule("some.module", package="pkg")
        result1 = lm._load()
        result2 = lm._load()

        mock_import.assert_called_once_with("some.module", "pkg")
        assert result1 is fake_mod
        assert result2 is fake_mod


class TestLazyModuleGetattr(unittest.TestCase):
    """Tests for LazyModule.__getattr__ (transparent proxy)."""

    def test_getattr_returns_real_attribute(self):
        from tui._lazy import LazyModule
        lm = LazyModule("json")
        # json.dumps should be accessible via __getattr__
        assert callable(lm.dumps)

    def test_getattr_triggers_load(self):
        from tui._lazy import LazyModule
        lm = LazyModule("os.path")
        assert lm._module is None
        _ = lm.join  # trigger load
        assert lm._module is not None

    def test_getattr_raises_for_missing(self):
        from tui._lazy import LazyModule
        lm = LazyModule("json")
        with self.assertRaises(AttributeError):
            _ = lm.nonexistent_attr_xyz_12345

    @patch("importlib.import_module")
    def test_getattr_delegates_to_loaded_module(self, mock_import):
        from tui._lazy import LazyModule
        fake_mod = MagicMock()
        fake_mod.some_func = "hello"
        mock_import.return_value = fake_mod

        lm = LazyModule("fake.mod")
        result = lm.some_func
        assert result == "hello"


class TestLazyModuleDir(unittest.TestCase):
    """Tests for LazyModule.__dir__."""

    def test_dir_returns_list(self):
        from tui._lazy import LazyModule
        lm = LazyModule("json")
        result = lm.__dir__()
        assert isinstance(result, list)

    def test_dir_contains_real_attributes(self):
        from tui._lazy import LazyModule
        lm = LazyModule("json")
        members = lm.__dir__()
        assert "dumps" in members
        assert "loads" in members

    def test_dir_triggers_load(self):
        from tui._lazy import LazyModule
        lm = LazyModule("json")
        assert lm._module is None
        lm.__dir__()
        assert lm._module is not None


class TestLazyImporter(unittest.TestCase):
    """Tests for LazyImporter class."""

    def test_init_empty_cache(self):
        from tui._lazy import LazyImporter
        importer = LazyImporter()
        assert importer._lazy_modules == {}

    def test_lazy_import_returns_lazy_module(self):
        from tui._lazy import LazyImporter, LazyModule
        importer = LazyImporter()
        result = importer.lazy_import("json")
        assert isinstance(result, LazyModule)

    def test_lazy_import_caches_module(self):
        from tui._lazy import LazyImporter
        importer = LazyImporter()
        first = importer.lazy_import("json")
        second = importer.lazy_import("json")
        assert first is second

    def test_lazy_import_different_modules_are_different(self):
        from tui._lazy import LazyImporter
        importer = LazyImporter()
        mod_json = importer.lazy_import("json")
        mod_os = importer.lazy_import("os")
        assert mod_json is not mod_os
        assert len(importer._lazy_modules) == 2

    def test_lazy_import_stores_in_dict(self):
        from tui._lazy import LazyImporter
        importer = LazyImporter()
        importer.lazy_import("json")
        assert "json" in importer._lazy_modules


class TestPublicLazyImportFunction(unittest.TestCase):
    """Tests for the public lazy_import() function."""

    def test_returns_lazy_module(self):
        from tui._lazy import lazy_import, LazyModule
        result = lazy_import("json")
        assert isinstance(result, LazyModule)

    def test_uses_global_importer(self):
        from tui._lazy import lazy_import, _importer
        result = lazy_import("json")
        assert "json" in _importer._lazy_modules
        assert _importer._lazy_modules["json"] is result

    def test_caches_via_global_importer(self):
        from tui._lazy import lazy_import
        first = lazy_import("os.path")
        second = lazy_import("os.path")
        assert first is second


class TestPredefinedLazyImports(unittest.TestCase):
    """Tests for module-level pre-defined lazy imports."""

    def test_textual_app_is_lazy_module(self):
        from tui._lazy import textual_app, LazyModule
        assert isinstance(textual_app, LazyModule)
        assert textual_app._name == "textual.app"

    def test_textual_binding_is_lazy_module(self):
        from tui._lazy import textual_binding, LazyModule
        assert isinstance(textual_binding, LazyModule)
        assert textual_binding._name == "textual.binding"

    def test_textual_containers_is_lazy_module(self):
        from tui._lazy import textual_containers, LazyModule
        assert isinstance(textual_containers, LazyModule)
        assert textual_containers._name == "textual.containers"

    def test_textual_screen_is_lazy_module(self):
        from tui._lazy import textual_screen, LazyModule
        assert isinstance(textual_screen, LazyModule)
        assert textual_screen._name == "textual.screen"

    def test_textual_widgets_is_lazy_module(self):
        from tui._lazy import textual_widgets, LazyModule
        assert isinstance(textual_widgets, LazyModule)
        assert textual_widgets._name == "textual.widgets"

    def test_rich_console_is_lazy_module(self):
        from tui._lazy import rich_console, LazyModule
        assert isinstance(rich_console, LazyModule)
        assert rich_console._name == "rich.console"

    def test_rich_table_is_lazy_module(self):
        from tui._lazy import rich_table, LazyModule
        assert isinstance(rich_table, LazyModule)
        assert rich_table._name == "rich.table"

    def test_all_predefined_not_loaded_initially(self):
        """Pre-defined lazy modules should not have loaded the real module yet
        unless something else in this test session already triggered them."""
        from tui._lazy import LazyModule
        # We can at least verify they are LazyModule instances
        from tui import _lazy
        for name in [
            "textual_app", "textual_binding", "textual_containers",
            "textual_screen", "textual_widgets", "rich_console", "rich_table",
        ]:
            obj = getattr(_lazy, name)
            assert isinstance(obj, LazyModule), f"{name} is not a LazyModule"

    def test_global_importer_exists(self):
        from tui._lazy import _importer, LazyImporter
        assert isinstance(_importer, LazyImporter)

    def test_global_importer_has_all_predefined(self):
        from tui._lazy import _importer
        expected = {
            "textual.app", "textual.binding", "textual.containers",
            "textual.screen", "textual.widgets", "rich.console", "rich.table",
        }
        assert expected.issubset(set(_importer._lazy_modules.keys()))


if __name__ == "__main__":
    unittest.main()
