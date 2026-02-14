"""Comprehensive unit tests for PlaybookScreen.

Tests cover: _load_playbooks (no dir, JSON files, name extraction,
JSONDecodeError fallback), on_option_list_option_selected (empty ID,
file not found, invalid JSON, gate/delegate iteration), actions.
"""

from __future__ import annotations

import json
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, PropertyMock, patch

from tui.screens.playbooks import PlaybookScreen


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
    return mock_app


class TestPlaybookScreenInit(unittest.TestCase):
    def test_bindings(self):
        screen = PlaybookScreen()
        keys = [b[0] for b in screen.BINDINGS]
        assert "escape" in keys
        assert "ctrl+k" in keys

    def test_euxis_app_property(self):
        screen = PlaybookScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)
        try:
            assert screen.euxis_app is mock_app
        finally:
            patcher.stop()


class TestPlaybookScreenCompose(unittest.TestCase):
    @patch("tui.screens.playbooks.OutputPanel")
    @patch("tui.screens.playbooks.OptionList")
    @patch("tui.screens.playbooks.Horizontal")
    @patch("tui.screens.playbooks.Container")
    @patch("tui.screens.playbooks.ETXHeader")
    def test_compose_yields_widgets(self, *mocks):
        screen = PlaybookScreen()
        result = list(screen.compose())
        assert len(result) > 0


class TestPlaybookScreenLoadPlaybooks(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="euxis_playbooks_")
        self.euxis_home = Path(self.tmpdir)

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def _make_screen(self):
        screen = PlaybookScreen()
        self.mock_option_list = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            if selector == "#log-list":
                return self.mock_option_list
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)
        return screen

    def test_no_playbook_dir(self):
        screen = self._make_screen()
        with patch("tui.screens.playbooks.EUXIS_HOME", self.euxis_home):
            screen._load_playbooks()

        call_args = self.mock_option_list.add_option.call_args[0][0]
        assert "No playbooks" in call_args.prompt

    def test_loads_json_files(self):
        pb_dir = self.euxis_home / "config" / "playbooks"
        pb_dir.mkdir(parents=True)

        (pb_dir / "deploy.json").write_text(
            json.dumps({"name": "Deploy Pipeline"})
        )
        (pb_dir / "review.json").write_text(
            json.dumps({"name": "Code Review"})
        )

        screen = self._make_screen()
        with patch("tui.screens.playbooks.EUXIS_HOME", self.euxis_home):
            screen._load_playbooks()

        assert self.mock_option_list.add_option.call_count == 2

    def test_name_extraction(self):
        pb_dir = self.euxis_home / "config" / "playbooks"
        pb_dir.mkdir(parents=True)

        (pb_dir / "alpha.json").write_text(json.dumps({"name": "Alpha Playbook"}))

        screen = self._make_screen()
        with patch("tui.screens.playbooks.EUXIS_HOME", self.euxis_home):
            screen._load_playbooks()

        option = self.mock_option_list.add_option.call_args[0][0]
        assert option.prompt == "Alpha Playbook"

    def test_json_decode_error_fallback(self):
        pb_dir = self.euxis_home / "config" / "playbooks"
        pb_dir.mkdir(parents=True)

        (pb_dir / "broken.json").write_text("NOT VALID JSON")

        screen = self._make_screen()
        with patch("tui.screens.playbooks.EUXIS_HOME", self.euxis_home):
            screen._load_playbooks()

        option = self.mock_option_list.add_option.call_args[0][0]
        assert option.prompt == "broken"  # Falls back to stem

    def test_missing_name_fallback(self):
        pb_dir = self.euxis_home / "config" / "playbooks"
        pb_dir.mkdir(parents=True)

        (pb_dir / "noname.json").write_text(json.dumps({"version": "1.0"}))

        screen = self._make_screen()
        with patch("tui.screens.playbooks.EUXIS_HOME", self.euxis_home):
            screen._load_playbooks()

        option = self.mock_option_list.add_option.call_args[0][0]
        assert option.prompt == "noname"


class TestPlaybookScreenOptionSelected(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="euxis_playbooks_sel_")
        self.euxis_home = Path(self.tmpdir)

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def _make_screen(self):
        screen = PlaybookScreen()
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

    def test_file_not_found(self):
        screen = self._make_screen()
        event = Mock()
        event.option.id = "nonexistent"

        with patch("tui.screens.playbooks.EUXIS_HOME", self.euxis_home):
            screen.on_option_list_option_selected(event)

        error_calls = [c[0][0] for c in self.mock_output.write_status.call_args_list]
        assert any("not found" in c.lower() for c in error_calls)

    def test_invalid_json(self):
        pb_dir = self.euxis_home / "config" / "playbooks"
        pb_dir.mkdir(parents=True)
        (pb_dir / "broken.json").write_text("INVALID")

        screen = self._make_screen()
        event = Mock()
        event.option.id = "broken"

        with patch("tui.screens.playbooks.EUXIS_HOME", self.euxis_home):
            screen.on_option_list_option_selected(event)

        error_calls = [c[0][0] for c in self.mock_output.write_status.call_args_list]
        assert any("Invalid" in c for c in error_calls)

    def test_displays_playbook_details(self):
        pb_dir = self.euxis_home / "config" / "playbooks"
        pb_dir.mkdir(parents=True)
        (pb_dir / "deploy.json").write_text(json.dumps({
            "name": "Deploy Pipeline",
            "version": "2.0",
            "description": "Automated deployment",
        }))

        screen = self._make_screen()
        event = Mock()
        event.option.id = "deploy"

        with patch("tui.screens.playbooks.EUXIS_HOME", self.euxis_home):
            screen.on_option_list_option_selected(event)

        status_calls = [c[0][0] for c in self.mock_output.write_status.call_args_list]
        assert any("Deploy Pipeline" in c for c in status_calls)
        assert any("2.0" in c for c in status_calls)

        write_calls = [c[0][0] for c in self.mock_output.write_line.call_args_list]
        assert any("Automated deployment" in c for c in write_calls)

    def test_displays_gates_and_delegates(self):
        pb_dir = self.euxis_home / "config" / "playbooks"
        pb_dir.mkdir(parents=True)
        (pb_dir / "review.json").write_text(json.dumps({
            "name": "Review",
            "gates": [
                {
                    "id": "G1",
                    "name": "Lint Check",
                    "checkpoint": "pre-commit",
                    "delegates": [
                        {"agent": "linter"},
                        {"agent": "formatter"},
                    ],
                },
                {
                    "id": "G2",
                    "name": "Test Check",
                    "delegates": [],
                },
            ],
        }))

        screen = self._make_screen()
        event = Mock()
        event.option.id = "review"

        with patch("tui.screens.playbooks.EUXIS_HOME", self.euxis_home):
            screen.on_option_list_option_selected(event)

        write_calls = [c[0][0] for c in self.mock_output.write_line.call_args_list]
        gate_lines = [c for c in write_calls if "Gate" in c]
        assert len(gate_lines) >= 2
        assert any("Lint Check" in c for c in gate_lines)
        assert any("linter" in c for c in write_calls)

    def test_gates_with_no_checkpoint(self):
        pb_dir = self.euxis_home / "config" / "playbooks"
        pb_dir.mkdir(parents=True)
        (pb_dir / "simple.json").write_text(json.dumps({
            "name": "Simple",
            "gates": [{"id": "G1", "name": "Basic", "delegates": []}],
        }))

        screen = self._make_screen()
        event = Mock()
        event.option.id = "simple"

        with patch("tui.screens.playbooks.EUXIS_HOME", self.euxis_home):
            screen.on_option_list_option_selected(event)

        write_calls = [c[0][0] for c in self.mock_output.write_line.call_args_list]
        # No "Checkpoint:" line since checkpoint is empty
        assert not any("Checkpoint:" in c for c in write_calls)


class TestPlaybookScreenActions(unittest.TestCase):
    def test_action_go_back(self):
        screen = PlaybookScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)
        try:
            screen.action_go_back()
            mock_app.pop_screen.assert_called_once()
        finally:
            patcher.stop()


class TestPlaybookScreenOnMount(unittest.TestCase):
    def test_on_mount_header_and_load(self):
        screen = PlaybookScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)

        mock_header = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            if hasattr(selector, "__name__"):
                return mock_header
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)

        with patch.object(screen, "_load_playbooks") as mock_load:
            screen.on_mount()
            mock_load.assert_called_once()

        assert mock_header.project == "test-project"
        patcher.stop()


if __name__ == "__main__":
    unittest.main()
