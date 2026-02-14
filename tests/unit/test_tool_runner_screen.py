"""Comprehensive unit tests for ToolRunnerScreen.

Tests cover: init (tool_name, tool_label default), _execute_tool (binary path
resolution home->EUXIS_HOME, not found, subprocess with stdout readline,
exit code 0 vs non-zero, notification, EUXIS_HOME env), actions.
"""

from __future__ import annotations

import asyncio
import unittest
from pathlib import Path
from unittest.mock import AsyncMock, Mock, PropertyMock, patch

from tui.screens.tool_runner import EUXIS_HOME, ToolRunnerScreen


def _patch_screen_app(screen, mock_app):
    patcher = patch.object(
        type(screen), "app", new_callable=PropertyMock, return_value=mock_app
    )
    patcher.start()
    return patcher


def _make_mock_app():
    mock_app = Mock()
    mock_app.project_name = "test-project"
    mock_app.git_branch = "main"
    mock_app.config = Mock()
    mock_app.config.default_provider = "claude"
    return mock_app


class TestToolRunnerScreenInit(unittest.TestCase):
    def test_init_defaults(self):
        screen = ToolRunnerScreen("euxis-audit")
        assert screen.tool_name == "euxis-audit"
        assert screen.tool_label == "euxis-audit"

    def test_init_custom_label(self):
        screen = ToolRunnerScreen("euxis-audit", tool_label="Security Audit")
        assert screen.tool_name == "euxis-audit"
        assert screen.tool_label == "Security Audit"

    def test_bindings(self):
        screen = ToolRunnerScreen("test")
        keys = [b[0] for b in screen.BINDINGS]
        assert "escape" in keys
        assert "ctrl+l" in keys

    def test_euxis_app_property(self):
        screen = ToolRunnerScreen("test")
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)
        try:
            assert screen.euxis_app is mock_app
        finally:
            patcher.stop()


class TestToolRunnerScreenCompose(unittest.TestCase):
    @patch("tui.screens.tool_runner.OutputPanel")
    @patch("tui.screens.tool_runner.Static")
    @patch("tui.screens.tool_runner.Container")
    @patch("tui.screens.tool_runner.ETXHeader")
    def test_compose_yields_widgets(self, *mocks):
        screen = ToolRunnerScreen("test")
        result = list(screen.compose())
        assert len(result) > 0


class TestToolRunnerScreenOnMount(unittest.TestCase):
    def test_on_mount(self):
        screen = ToolRunnerScreen("euxis-audit", tool_label="Audit")
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)

        mock_header = Mock()
        mock_title = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            if hasattr(selector, "__name__"):
                return mock_header
            if selector == "#tool-title":
                return mock_title
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)
        screen.run_worker = Mock()

        screen.on_mount()

        assert mock_header.project == "test-project"
        assert mock_header.branch == "main"
        call_args = mock_title.update.call_args[0][0]
        assert "Audit" in call_args
        screen.run_worker.assert_called_once()
        patcher.stop()


class TestToolRunnerExecuteTool(unittest.TestCase):
    def setUp(self):
        self.screen = ToolRunnerScreen("euxis-audit", tool_label="Audit Tool")
        self.mock_app = _make_mock_app()
        self._patcher = _patch_screen_app(self.screen, self.mock_app)
        self.mock_output = Mock()
        self.screen.query_one = Mock(return_value=self.mock_output)
        self.screen.notify = Mock()

    def tearDown(self):
        self._patcher.stop()

    def _run(self):
        loop = asyncio.new_event_loop()
        try:
            loop.run_until_complete(self.screen._execute_tool())
        finally:
            loop.close()

    def test_binary_not_found(self):
        with patch.object(Path, "exists", return_value=False):
            self._run()

        status_calls = [c[0][0] for c in self.mock_output.write_status.call_args_list]
        assert any("not found" in c.lower() for c in status_calls)

    def test_binary_found_streams_output(self):
        mock_process = AsyncMock()
        mock_stdout = AsyncMock()
        mock_stdout.readline = AsyncMock(side_effect=[b"output line\n", b""])
        mock_process.stdout = mock_stdout
        mock_process.wait = AsyncMock()
        mock_process.returncode = 0

        with patch.object(Path, "exists", return_value=True):
            with patch("asyncio.create_subprocess_exec", return_value=mock_process):
                self._run()

        write_calls = [c[0][0] for c in self.mock_output.write_line.call_args_list]
        assert "output line" in write_calls

    def test_subprocess_exit_code_0(self):
        mock_process = AsyncMock()
        mock_stdout = AsyncMock()
        mock_stdout.readline = AsyncMock(side_effect=[b"done\n", b""])
        mock_process.stdout = mock_stdout
        mock_process.wait = AsyncMock()
        mock_process.returncode = 0

        with patch.object(Path, "exists", return_value=True):
            with patch("asyncio.create_subprocess_exec", return_value=mock_process):
                self._run()

        status_calls = [c[0][0] for c in self.mock_output.write_status.call_args_list]
        assert any("exit 0" in c.lower() for c in status_calls)
        self.screen.notify.assert_called_once()
        assert "complete" in self.screen.notify.call_args[0][0].lower()

    def test_subprocess_exit_code_nonzero(self):
        mock_process = AsyncMock()
        mock_stdout = AsyncMock()
        mock_stdout.readline = AsyncMock(side_effect=[b"error\n", b""])
        mock_process.stdout = mock_stdout
        mock_process.wait = AsyncMock()
        mock_process.returncode = 1

        with patch.object(Path, "exists", return_value=True):
            with patch("asyncio.create_subprocess_exec", return_value=mock_process):
                self._run()

        status_calls = [c[0][0] for c in self.mock_output.write_status.call_args_list]
        assert any("exit 1" in c.lower() for c in status_calls)
        self.screen.notify.assert_called_once()
        assert "failed" in self.screen.notify.call_args[0][0].lower()

    def test_none_stdout(self):
        mock_process = AsyncMock()
        mock_process.stdout = None
        mock_process.wait = AsyncMock()
        mock_process.returncode = 0

        with patch.object(Path, "exists", return_value=True):
            with patch("asyncio.create_subprocess_exec", return_value=mock_process):
                self._run()

        status_calls = [c[0][0] for c in self.mock_output.write_status.call_args_list]
        assert any("Failed to capture" in c for c in status_calls)

    def test_multiple_output_lines(self):
        mock_process = AsyncMock()
        mock_stdout = AsyncMock()
        mock_stdout.readline = AsyncMock(
            side_effect=[b"line1\n", b"line2\n", b"line3\n", b""]
        )
        mock_process.stdout = mock_stdout
        mock_process.wait = AsyncMock()
        mock_process.returncode = 0

        with patch.object(Path, "exists", return_value=True):
            with patch("asyncio.create_subprocess_exec", return_value=mock_process):
                self._run()

        write_calls = [c[0][0] for c in self.mock_output.write_line.call_args_list]
        assert "line1" in write_calls
        assert "line2" in write_calls
        assert "line3" in write_calls


class TestToolRunnerScreenActions(unittest.TestCase):
    def setUp(self):
        self.screen = ToolRunnerScreen("test")
        self.mock_app = _make_mock_app()
        self._patcher = _patch_screen_app(self.screen, self.mock_app)

    def tearDown(self):
        self._patcher.stop()

    def test_action_go_back(self):
        self.screen.action_go_back()
        self.mock_app.pop_screen.assert_called_once()

    def test_action_clear_output(self):
        mock_output = Mock()
        self.screen.query_one = Mock(return_value=mock_output)
        self.screen.action_clear_output()
        mock_output.clear.assert_called_once()


if __name__ == "__main__":
    unittest.main()
