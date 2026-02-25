# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Extended unit tests for tui/__main__.py — run_audit, show_help, main.

Tests cover: run_audit() output sections, show_help() output,
main() argument dispatch (--audit, --help, unknown, no args).
All Textual runtime and subprocess calls are fully mocked.
"""

from __future__ import annotations

import io
import sys
import unittest
from unittest.mock import MagicMock, Mock, patch


class TestRunAudit(unittest.TestCase):
    """Tests for the run_audit() function."""

    def _call_run_audit(self, env_overrides=None, uname_release="6.5.0-generic"):
        """Helper to call run_audit with mocked environment."""
        from tui.__main__ import run_audit

        env = {
            "TERM": "xterm-256color",
            "COLORTERM": "truecolor",
            "TERM_PROGRAM": "iTerm.app",
        }
        if env_overrides:
            env.update(env_overrides)

        mock_uname = Mock()
        mock_uname.release = uname_release

        captured = io.StringIO()
        with patch.dict("os.environ", env, clear=False), \
             patch("os.uname", return_value=mock_uname), \
             patch("shutil.get_terminal_size", return_value=(80, 24)), \
             patch("sys.stdout", captured), \
             patch("time.perf_counter", side_effect=[0.0, 0.010, 0.0, 0.001, 0.0, 0.005]):
            # Mock the textual/rich imports within run_audit
            mock_app = MagicMock()
            mock_text = MagicMock()
            mock_console_cls = MagicMock()
            mock_console_instance = MagicMock()
            mock_console_cls.return_value = mock_console_instance
            mock_rich_text = MagicMock(return_value=MagicMock())
            mock_richlog = MagicMock()
            mock_euxis_app = MagicMock()

            with patch.dict("sys.modules", {
                "textual.app": MagicMock(App=mock_app),
                "rich.text": MagicMock(Text=mock_text),
                "rich.console": MagicMock(Console=mock_console_cls),
                "textual.widgets": MagicMock(RichLog=mock_richlog),
                "tui.app": MagicMock(EuxisApp=mock_euxis_app),
            }):
                run_audit()

        return captured.getvalue()

    def test_output_contains_header(self):
        output = self._call_run_audit()
        assert "Euxis TUI Performance Audit" in output

    def test_output_contains_environment_section(self):
        output = self._call_run_audit()
        assert "Environment Detection" in output

    def test_output_shows_term(self):
        output = self._call_run_audit()
        assert "xterm-256color" in output

    def test_output_shows_colorterm(self):
        output = self._call_run_audit()
        assert "truecolor" in output

    def test_output_shows_term_program(self):
        output = self._call_run_audit()
        assert "iTerm.app" in output

    def test_output_contains_color_support_section(self):
        output = self._call_run_audit()
        assert "Color Support" in output

    def test_output_contains_unicode_section(self):
        output = self._call_run_audit()
        assert "Unicode Support" in output

    def test_output_contains_framework_section(self):
        output = self._call_run_audit()
        assert "Framework Tests" in output

    def test_output_contains_rendering_section(self):
        output = self._call_run_audit()
        assert "Rendering Benchmark" in output

    def test_output_contains_signal_section(self):
        output = self._call_run_audit()
        assert "Signal Handling" in output

    def test_output_contains_crossplatform_section(self):
        output = self._call_run_audit()
        assert "Cross-Platform Notes" in output

    def test_output_contains_footer(self):
        output = self._call_run_audit()
        assert "Audit complete" in output

    def test_wsl_detected_from_uname(self):
        """When uname release contains 'microsoft', WSL should appear."""
        output = self._call_run_audit(uname_release="5.15.0-microsoft-standard-WSL2")
        assert "WSL" in output

    def test_macos_platform_notes(self):
        """On darwin, macOS-specific notes appear."""
        captured = io.StringIO()
        mock_uname = Mock()
        mock_uname.release = "23.0.0"

        with patch.dict("os.environ", {"TERM": "xterm", "COLORTERM": "", "TERM_PROGRAM": ""}, clear=False), \
             patch("os.uname", return_value=mock_uname), \
             patch("shutil.get_terminal_size", return_value=(80, 24)), \
             patch("sys.stdout", captured), \
             patch("sys.platform", "darwin"), \
             patch("time.perf_counter", side_effect=[0.0, 0.010, 0.0, 0.001, 0.0, 0.005]):
            with patch.dict("sys.modules", {
                "textual.app": MagicMock(),
                "rich.text": MagicMock(),
                "rich.console": MagicMock(),
                "textual.widgets": MagicMock(),
                "tui.app": MagicMock(),
            }):
                from tui.__main__ import run_audit
                run_audit()

        assert "macOS" in captured.getvalue()

    def test_truecolor_detected(self):
        output = self._call_run_audit(env_overrides={"COLORTERM": "truecolor"})
        assert "True color" in output

    def test_no_colorterm_shows_not_set(self):
        output = self._call_run_audit(env_overrides={"COLORTERM": ""})
        assert "(not set)" in output

    def test_linux_platform_notes(self):
        """On linux (non-WSL), Linux-specific notes appear."""
        captured = io.StringIO()
        mock_uname = Mock()
        mock_uname.release = "6.5.0-generic"

        with patch.dict("os.environ", {"TERM": "xterm", "COLORTERM": "", "TERM_PROGRAM": ""}, clear=False), \
             patch("os.uname", return_value=mock_uname), \
             patch("shutil.get_terminal_size", return_value=(80, 24)), \
             patch("sys.stdout", captured), \
             patch("sys.platform", "linux"), \
             patch("time.perf_counter", side_effect=[0.0, 0.010, 0.0, 0.001, 0.0, 0.005]):
            with patch.dict("sys.modules", {
                "textual.app": MagicMock(),
                "rich.text": MagicMock(),
                "rich.console": MagicMock(),
                "textual.widgets": MagicMock(),
                "tui.app": MagicMock(),
            }):
                from tui.__main__ import run_audit
                run_audit()

        output = captured.getvalue()
        assert "Linux" in output
        assert "xterm-256color" in output

    def test_linux_wsl_platform_notes(self):
        """On linux with WSL, WSL-specific notes appear."""
        captured = io.StringIO()
        mock_uname = Mock()
        mock_uname.release = "5.15.0-microsoft-standard-WSL2"

        with patch.dict("os.environ", {"TERM": "xterm", "COLORTERM": "", "TERM_PROGRAM": ""}, clear=False), \
             patch("os.uname", return_value=mock_uname), \
             patch("shutil.get_terminal_size", return_value=(80, 24)), \
             patch("sys.stdout", captured), \
             patch("sys.platform", "linux"), \
             patch("time.perf_counter", side_effect=[0.0, 0.010, 0.0, 0.001, 0.0, 0.005]):
            with patch.dict("sys.modules", {
                "textual.app": MagicMock(),
                "rich.text": MagicMock(),
                "rich.console": MagicMock(),
                "textual.widgets": MagicMock(),
                "tui.app": MagicMock(),
            }):
                from tui.__main__ import run_audit
                run_audit()

        output = captured.getvalue()
        assert "WSL" in output
        assert "Windows Terminal" in output

    def test_win32_platform_notes(self):
        """On win32, Windows-specific notes appear."""
        captured = io.StringIO()
        mock_uname = Mock()
        mock_uname.release = "10.0.19041"

        with patch.dict("os.environ", {"TERM": "xterm", "COLORTERM": "", "TERM_PROGRAM": ""}, clear=False), \
             patch("os.uname", return_value=mock_uname), \
             patch("shutil.get_terminal_size", return_value=(80, 24)), \
             patch("sys.stdout", captured), \
             patch("sys.platform", "win32"), \
             patch("time.perf_counter", side_effect=[0.0, 0.010, 0.0, 0.001, 0.0, 0.005]):
            with patch.dict("sys.modules", {
                "textual.app": MagicMock(),
                "rich.text": MagicMock(),
                "rich.console": MagicMock(),
                "textual.widgets": MagicMock(),
                "tui.app": MagicMock(),
            }):
                from tui.__main__ import run_audit
                run_audit()

        output = captured.getvalue()
        assert "Windows" in output
        assert "cmd.exe" in output

    def test_framework_import_error_handled(self):
        """When textual/rich imports fail, error is shown gracefully."""
        captured = io.StringIO()
        mock_uname = Mock()
        mock_uname.release = "6.5.0"

        # Remove textual from sys.modules so import fails
        with patch.dict("os.environ", {"TERM": "xterm", "COLORTERM": "", "TERM_PROGRAM": ""}, clear=False), \
             patch("os.uname", return_value=mock_uname), \
             patch("shutil.get_terminal_size", return_value=(80, 24)), \
             patch("sys.stdout", captured):

            # Make 'from textual.app import App' raise ImportError
            original_import = __builtins__.__import__ if hasattr(__builtins__, '__import__') else __import__

            def mock_import(name, *args, **kwargs):
                if name in ("textual.app", "textual.widgets", "rich.text", "rich.console"):
                    raise ImportError(f"No module named '{name}'")
                if name == "tui.app":
                    raise ImportError("No module named 'tui.app'")
                return original_import(name, *args, **kwargs)

            with patch("builtins.__import__", side_effect=mock_import):
                from tui.__main__ import run_audit
                run_audit()

        output = captured.getvalue()
        # Should still contain the header and not crash
        assert "Euxis TUI Performance Audit" in output


class TestShowHelp(unittest.TestCase):
    """Tests for the show_help() function."""

    def test_output_contains_usage(self):
        from tui.__main__ import show_help
        captured = io.StringIO()
        with patch("sys.stdout", captured):
            show_help()
        output = captured.getvalue()
        assert "Usage:" in output

    def test_output_contains_title(self):
        from tui.__main__ import show_help
        captured = io.StringIO()
        with patch("sys.stdout", captured):
            show_help()
        output = captured.getvalue()
        assert "Euxis TUI" in output

    def test_output_contains_audit_option(self):
        from tui.__main__ import show_help
        captured = io.StringIO()
        with patch("sys.stdout", captured):
            show_help()
        output = captured.getvalue()
        assert "--audit" in output

    def test_output_contains_help_option(self):
        from tui.__main__ import show_help
        captured = io.StringIO()
        with patch("sys.stdout", captured):
            show_help()
        output = captured.getvalue()
        assert "--help" in output

    def test_output_contains_keyboard_shortcuts(self):
        from tui.__main__ import show_help
        captured = io.StringIO()
        with patch("sys.stdout", captured):
            show_help()
        output = captured.getvalue()
        assert "Keyboard Shortcuts" in output
        assert "Ctrl+K" in output
        assert "Ctrl+Q" in output

    def test_output_contains_all_shortcuts(self):
        from tui.__main__ import show_help
        captured = io.StringIO()
        with patch("sys.stdout", captured):
            show_help()
        output = captured.getvalue()
        for shortcut in ["Ctrl+K", "F3", "Ctrl+M", "F4", "F6", "F1", "/", "Ctrl+Q"]:
            assert shortcut in output, f"Missing shortcut: {shortcut}"


class TestMainAuditArg(unittest.TestCase):
    """Tests for main() with --audit argument."""

    @patch("tui.__main__.run_audit")
    def test_audit_long_flag(self, mock_audit):
        from tui.__main__ import main
        with patch.object(sys, "argv", ["tui", "--audit"]):
            main()
        mock_audit.assert_called_once()

    @patch("tui.__main__.run_audit")
    def test_audit_short_flag(self, mock_audit):
        from tui.__main__ import main
        with patch.object(sys, "argv", ["tui", "-a"]):
            main()
        mock_audit.assert_called_once()

    @patch("tui.__main__.run_audit")
    def test_audit_word(self, mock_audit):
        from tui.__main__ import main
        with patch.object(sys, "argv", ["tui", "audit"]):
            main()
        mock_audit.assert_called_once()

    @patch("tui.__main__.run_audit")
    def test_audit_case_insensitive(self, mock_audit):
        from tui.__main__ import main
        with patch.object(sys, "argv", ["tui", "--AUDIT"]):
            main()
        mock_audit.assert_called_once()


class TestMainHelpArg(unittest.TestCase):
    """Tests for main() with --help argument."""

    @patch("tui.__main__.show_help")
    def test_help_long_flag(self, mock_help):
        from tui.__main__ import main
        with patch.object(sys, "argv", ["tui", "--help"]):
            main()
        mock_help.assert_called_once()

    @patch("tui.__main__.show_help")
    def test_help_short_flag(self, mock_help):
        from tui.__main__ import main
        with patch.object(sys, "argv", ["tui", "-h"]):
            main()
        mock_help.assert_called_once()

    @patch("tui.__main__.show_help")
    def test_help_word(self, mock_help):
        from tui.__main__ import main
        with patch.object(sys, "argv", ["tui", "help"]):
            main()
        mock_help.assert_called_once()


class TestMainUnknownArg(unittest.TestCase):
    """Tests for main() with unknown argument."""

    def test_unknown_arg_exits_with_1(self):
        from tui.__main__ import main
        captured = io.StringIO()
        with patch.object(sys, "argv", ["tui", "--bogus"]), \
             patch("sys.stdout", captured), \
             self.assertRaises(SystemExit) as ctx:
            main()
        assert ctx.exception.code == 1

    def test_unknown_arg_prints_error(self):
        from tui.__main__ import main
        captured = io.StringIO()
        with patch.object(sys, "argv", ["tui", "--bogus"]), \
             patch("sys.stdout", captured), \
             patch("sys.exit", side_effect=SystemExit(1)):
            try:
                main()
            except SystemExit:
                pass
        output = captured.getvalue()
        assert "Unknown argument" in output
        assert "--bogus" in output

    def test_unknown_arg_suggests_help(self):
        from tui.__main__ import main
        captured = io.StringIO()
        with patch.object(sys, "argv", ["tui", "xyz"]), \
             patch("sys.stdout", captured), \
             patch("sys.exit", side_effect=SystemExit(1)):
            try:
                main()
            except SystemExit:
                pass
        output = captured.getvalue()
        assert "--help" in output


class TestMainNoArgs(unittest.TestCase):
    """Tests for main() with no arguments (normal TUI mode)."""

    def _patch_tui_app(self):
        """Insert a mock tui.app module into sys.modules and return the mock EuxisApp."""
        mock_app_module = MagicMock()
        mock_instance = Mock()
        mock_app_module.EuxisApp.return_value = mock_instance
        return mock_app_module, mock_instance

    def test_no_args_creates_app(self):
        mock_app_module, mock_instance = self._patch_tui_app()
        from tui.__main__ import main
        with patch.object(sys, "argv", ["tui"]), \
             patch.dict("sys.modules", {"tui.app": mock_app_module}):
            main()
        mock_app_module.EuxisApp.assert_called_once()

    def test_no_args_calls_run(self):
        mock_app_module, mock_instance = self._patch_tui_app()
        from tui.__main__ import main
        with patch.object(sys, "argv", ["tui"]), \
             patch.dict("sys.modules", {"tui.app": mock_app_module}):
            main()
        mock_instance.run.assert_called_once()

    @patch("tui.__main__.run_audit")
    @patch("tui.__main__.show_help")
    def test_no_args_does_not_call_audit_or_help(self, mock_help, mock_audit):
        mock_app_module, _ = self._patch_tui_app()
        from tui.__main__ import main
        with patch.object(sys, "argv", ["tui"]), \
             patch.dict("sys.modules", {"tui.app": mock_app_module}):
            main()
        mock_audit.assert_not_called()
        mock_help.assert_not_called()

    @patch("tui.__main__.run_audit")
    def test_audit_returns_without_running_app(self, mock_audit):
        """After --audit, main() should return, not attempt to start the app."""
        mock_app_module, _ = self._patch_tui_app()
        from tui.__main__ import main
        with patch.object(sys, "argv", ["tui", "--audit"]), \
             patch.dict("sys.modules", {"tui.app": mock_app_module}):
            main()
        mock_app_module.EuxisApp.assert_not_called()

    @patch("tui.__main__.show_help")
    def test_help_returns_without_running_app(self, mock_help):
        """After --help, main() should return, not attempt to start the app."""
        mock_app_module, _ = self._patch_tui_app()
        from tui.__main__ import main
        with patch.object(sys, "argv", ["tui", "--help"]), \
             patch.dict("sys.modules", {"tui.app": mock_app_module}):
            main()
        mock_app_module.EuxisApp.assert_not_called()


class TestMainGuard(unittest.TestCase):
    """Tests for the __name__ == '__main__' guard."""

    def test_module_has_main_guard(self):
        """Verify the module contains the if __name__ == '__main__' guard."""
        import inspect
        import tui.__main__ as mod
        source = inspect.getsource(mod)
        assert 'if __name__ == "__main__"' in source or "if __name__ == '__main__'" in source


if __name__ == "__main__":
    unittest.main()
