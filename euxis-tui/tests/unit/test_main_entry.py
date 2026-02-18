# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Unit tests for tui/__main__.py entry point.

Tests cover: EuxisApp instantiation and .run() invocation.
"""

from __future__ import annotations

import sys
import unittest
from unittest.mock import Mock, patch


class TestMainEntry(unittest.TestCase):
    """Tests for the __main__.py entry point."""

    @patch("tui.app.EuxisApp")
    def test_app_instantiated_and_run(self, mock_app_cls):
        mock_instance = Mock()
        mock_app_cls.return_value = mock_instance

        # Remove cached module to ensure clean reload
        sys.modules.pop("tui.__main__", None)

        from tui.__main__ import main
        # Simulate normal TUI mode (no CLI args)
        with patch.object(sys, "argv", ["tui"]):
            main()

        mock_app_cls.assert_called()
        mock_instance.run.assert_called_once_with()


if __name__ == "__main__":
    unittest.main()
