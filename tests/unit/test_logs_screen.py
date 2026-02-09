"""Comprehensive unit tests for LogViewerScreen.

Tests cover: _load_log_list (no project dir, skip non-dirs, skip dirs without
output/, count .md files), on_option_list_option_selected (empty ID, no output
dir, no .md files, file content display, most-recent sorting), actions.
"""

from __future__ import annotations

import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, PropertyMock, patch

from tui.screens.logs import LogViewerScreen


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


class TestLogViewerScreenInit(unittest.TestCase):
    def test_bindings(self):
        screen = LogViewerScreen()
        keys = [b[0] for b in screen.BINDINGS]
        assert "escape" in keys
        assert "ctrl+k" in keys

    def test_euxis_app_property(self):
        screen = LogViewerScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)
        try:
            assert screen.euxis_app is mock_app
        finally:
            patcher.stop()


class TestLogViewerScreenCompose(unittest.TestCase):
    @patch("tui.screens.logs.Footer")
    @patch("tui.screens.logs.OutputPanel")
    @patch("tui.screens.logs.OptionList")
    @patch("tui.screens.logs.Horizontal")
    @patch("tui.screens.logs.Container")
    @patch("tui.screens.logs.ETXHeader")
    def test_compose_yields_widgets(self, *mocks):
        screen = LogViewerScreen()
        result = list(screen.compose())
        assert len(result) > 0


class TestLogViewerScreenOnMount(unittest.TestCase):
    def test_on_mount_header_and_load(self):
        screen = LogViewerScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)

        mock_header = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            if hasattr(selector, "__name__"):
                return mock_header
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)

        with patch.object(screen, "_load_log_list") as mock_load:
            screen.on_mount()
            mock_load.assert_called_once()

        assert mock_header.project == "test-project"
        assert mock_header.branch == "main"
        assert mock_header.provider == "claude"
        patcher.stop()


class TestLogViewerScreenLoadLogList(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="euxis_logs_")
        self.euxis_home = Path(self.tmpdir)

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def _make_screen(self):
        screen = LogViewerScreen()
        self.mock_option_list = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            if selector == "#log-list":
                return self.mock_option_list
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)
        return screen

    def test_no_project_dir(self):
        screen = self._make_screen()
        with patch("tui.screens.logs.EUXIS_HOME", self.euxis_home):
            with patch("tui.screens.logs.get_project_name", return_value="myproject"):
                screen._load_log_list()

        call_args = self.mock_option_list.add_option.call_args[0][0]
        assert "No logs" in call_args.prompt

    def test_skips_non_directories(self):
        project_dir = self.euxis_home / "data" / "projects" / "myproject"
        project_dir.mkdir(parents=True)

        # Create a regular file (not a directory)
        (project_dir / "somefile.txt").write_text("not a dir")

        screen = self._make_screen()
        with patch("tui.screens.logs.EUXIS_HOME", self.euxis_home):
            with patch("tui.screens.logs.get_project_name", return_value="myproject"):
                screen._load_log_list()

        # No options should be added (only the "No logs" disabled option)
        # Actually: if there are no valid dirs, no options at all
        self.mock_option_list.add_option.assert_not_called()

    def test_skips_dirs_without_output(self):
        project_dir = self.euxis_home / "data" / "projects" / "myproject"
        agent_dir = project_dir / "architect"
        agent_dir.mkdir(parents=True)
        # No output/ subdirectory

        screen = self._make_screen()
        with patch("tui.screens.logs.EUXIS_HOME", self.euxis_home):
            with patch("tui.screens.logs.get_project_name", return_value="myproject"):
                screen._load_log_list()

        self.mock_option_list.add_option.assert_not_called()

    def test_skips_output_with_no_md_files(self):
        project_dir = self.euxis_home / "data" / "projects" / "myproject"
        output_dir = project_dir / "architect" / "output"
        output_dir.mkdir(parents=True)
        (output_dir / "data.json").write_text("{}")

        screen = self._make_screen()
        with patch("tui.screens.logs.EUXIS_HOME", self.euxis_home):
            with patch("tui.screens.logs.get_project_name", return_value="myproject"):
                screen._load_log_list()

        self.mock_option_list.add_option.assert_not_called()

    def test_lists_agents_with_md_files(self):
        project_dir = self.euxis_home / "data" / "projects" / "myproject"

        for agent in ["architect", "debugger"]:
            output_dir = project_dir / agent / "output"
            output_dir.mkdir(parents=True)
            (output_dir / "run-001.md").write_text("# Output")

        # Add extra md file to architect
        (project_dir / "architect" / "output" / "run-002.md").write_text("# More")

        screen = self._make_screen()
        with patch("tui.screens.logs.EUXIS_HOME", self.euxis_home):
            with patch("tui.screens.logs.get_project_name", return_value="myproject"):
                screen._load_log_list()

        assert self.mock_option_list.add_option.call_count == 2

        # Check architect has count=2
        calls = self.mock_option_list.add_option.call_args_list
        prompts = [c[0][0].prompt for c in calls]
        assert any("architect (2)" in p for p in prompts)
        assert any("debugger (1)" in p for p in prompts)


class TestLogViewerScreenOptionSelected(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="euxis_logs_sel_")
        self.euxis_home = Path(self.tmpdir)

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def _make_screen(self):
        screen = LogViewerScreen()
        self.mock_output = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            return self.mock_output

        screen.query_one = Mock(side_effect=query_one_side_effect)
        return screen

    def test_empty_id_returns_early(self):
        screen = self._make_screen()
        event = Mock()
        event.option.id = ""
        screen.on_option_list_option_selected(event)
        self.mock_output.clear.assert_not_called()

    def test_no_output_dir(self):
        screen = self._make_screen()
        event = Mock()
        event.option.id = "architect"

        with patch("tui.screens.logs.EUXIS_HOME", self.euxis_home):
            with patch("tui.screens.logs.get_project_name", return_value="myproject"):
                screen.on_option_list_option_selected(event)

        status_calls = [c[0][0] for c in self.mock_output.write_status.call_args_list]
        assert any("No outputs" in c for c in status_calls)

    def test_no_md_files(self):
        project_dir = self.euxis_home / "data" / "projects" / "myproject"
        output_dir = project_dir / "architect" / "output"
        output_dir.mkdir(parents=True)

        screen = self._make_screen()
        event = Mock()
        event.option.id = "architect"

        with patch("tui.screens.logs.EUXIS_HOME", self.euxis_home):
            with patch("tui.screens.logs.get_project_name", return_value="myproject"):
                screen.on_option_list_option_selected(event)

        status_calls = [c[0][0] for c in self.mock_output.write_status.call_args_list]
        assert any("No output files" in c for c in status_calls)

    def test_displays_most_recent_file(self):
        project_dir = self.euxis_home / "data" / "projects" / "myproject"
        output_dir = project_dir / "architect" / "output"
        output_dir.mkdir(parents=True)

        (output_dir / "2024-01-01.md").write_text("# Old run\nOld content")
        (output_dir / "2024-12-31.md").write_text("# Latest run\nNew content")

        screen = self._make_screen()
        event = Mock()
        event.option.id = "architect"

        with patch("tui.screens.logs.EUXIS_HOME", self.euxis_home):
            with patch("tui.screens.logs.get_project_name", return_value="myproject"):
                screen.on_option_list_option_selected(event)

        # Should display latest file (2024-12-31.md sorts last)
        status_calls = [c[0][0] for c in self.mock_output.write_status.call_args_list]
        assert any("architect" in c for c in status_calls)
        assert any("2024-12-31.md" in c for c in status_calls)

        write_calls = [c[0][0] for c in self.mock_output.write_line.call_args_list]
        assert "# Latest run" in write_calls
        assert "New content" in write_calls

    def test_file_content_line_by_line(self):
        project_dir = self.euxis_home / "data" / "projects" / "myproject"
        output_dir = project_dir / "debugger" / "output"
        output_dir.mkdir(parents=True)
        (output_dir / "run.md").write_text("line1\nline2\nline3")

        screen = self._make_screen()
        event = Mock()
        event.option.id = "debugger"

        with patch("tui.screens.logs.EUXIS_HOME", self.euxis_home):
            with patch("tui.screens.logs.get_project_name", return_value="myproject"):
                screen.on_option_list_option_selected(event)

        write_calls = [c[0][0] for c in self.mock_output.write_line.call_args_list]
        assert "line1" in write_calls
        assert "line2" in write_calls
        assert "line3" in write_calls


class TestLogViewerScreenActions(unittest.TestCase):
    def test_action_go_back(self):
        screen = LogViewerScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)
        try:
            screen.action_go_back()
            mock_app.pop_screen.assert_called_once()
        finally:
            patcher.stop()


if __name__ == "__main__":
    unittest.main()
