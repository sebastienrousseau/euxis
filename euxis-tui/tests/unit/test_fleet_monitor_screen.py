# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Comprehensive unit tests for FleetMonitorScreen.

Tests cover: AgentMonitorRow (init, compose, set_status), FleetMonitorScreen
(init, compose, on_mount, _execute_operation for squad/combo, _set_agent_status
with NoMatches, actions).
"""

from __future__ import annotations

import asyncio
import unittest
from unittest.mock import Mock, PropertyMock, patch

from tui.screens.fleet_monitor import AgentMonitorRow, FleetMonitorScreen


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


# ===========================================================================
# AgentMonitorRow tests
# ===========================================================================


class TestAgentMonitorRow(unittest.TestCase):
    """Tests for AgentMonitorRow widget."""

    def test_init(self):
        row = AgentMonitorRow("architect")
        assert row.agent_id == "architect"
        assert row._status == "pending"

    @patch("tui.screens.fleet_monitor.ProgressBar")
    @patch("tui.screens.fleet_monitor.Static")
    def test_compose(self, mock_static, mock_progress):
        row = AgentMonitorRow("debugger")
        result = list(row.compose())
        assert len(result) == 3

    def test_set_status_running(self):
        row = AgentMonitorRow("architect")
        mock_status = Mock()
        mock_progress = Mock()
        row.query = Mock(return_value=Mock(first=Mock(return_value=mock_status)))
        row.query_one = Mock(return_value=mock_progress)

        row.set_status("running", 50)

        assert row._status == "running"
        mock_status.update.assert_called_once()
        assert "Running" in mock_status.update.call_args[0][0]
        assert mock_progress.progress == 50

    def test_set_status_complete(self):
        row = AgentMonitorRow("architect")
        mock_status = Mock()
        mock_progress = Mock()
        row.query = Mock(return_value=Mock(first=Mock(return_value=mock_status)))
        row.query_one = Mock(return_value=mock_progress)

        row.set_status("complete")

        assert row._status == "complete"
        assert "Complete" in mock_status.update.call_args[0][0]
        assert mock_progress.progress == 100

    def test_set_status_error(self):
        row = AgentMonitorRow("architect")
        mock_status = Mock()
        mock_progress = Mock()
        row.query = Mock(return_value=Mock(first=Mock(return_value=mock_status)))
        row.query_one = Mock(return_value=mock_progress)

        row.set_status("error")

        assert row._status == "error"
        assert "Error" in mock_status.update.call_args[0][0]

    def test_set_status_pending(self):
        row = AgentMonitorRow("architect")
        mock_status = Mock()
        mock_progress = Mock()
        row.query = Mock(return_value=Mock(first=Mock(return_value=mock_status)))
        row.query_one = Mock(return_value=mock_progress)

        row.set_status("pending")

        assert row._status == "pending"
        assert "Pending" in mock_status.update.call_args[0][0]
        assert mock_progress.progress == 0

    def test_set_status_unknown(self):
        """Unknown status falls through to pending branch."""
        row = AgentMonitorRow("architect")
        mock_status = Mock()
        mock_progress = Mock()
        row.query = Mock(return_value=Mock(first=Mock(return_value=mock_status)))
        row.query_one = Mock(return_value=mock_progress)

        row.set_status("unknown-status")

        assert "Pending" in mock_status.update.call_args[0][0]
        assert mock_progress.progress == 0


# ===========================================================================
# FleetMonitorScreen init/compose tests
# ===========================================================================


class TestFleetMonitorScreenInit(unittest.TestCase):
    """Tests for FleetMonitorScreen initialization."""

    def test_defaults(self):
        screen = FleetMonitorScreen()
        assert screen.operation_type == "squad"
        assert screen.operation_id == ""
        assert screen.members == []
        assert screen.task_description == ""

    def test_custom_init(self):
        screen = FleetMonitorScreen(
            operation_type="combo",
            operation_id="build-combo",
            members=["a", "b"],
            task_description="run tests",
        )
        assert screen.operation_type == "combo"
        assert screen.operation_id == "build-combo"
        assert screen.members == ["a", "b"]
        assert screen.task_description == "run tests"

    def test_bindings(self):
        screen = FleetMonitorScreen()
        keys = [b[0] for b in screen.BINDINGS]
        assert "escape" in keys
        assert "ctrl+k" in keys

    def test_euxis_app_property(self):
        screen = FleetMonitorScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)
        try:
            assert screen.euxis_app is mock_app
        finally:
            patcher.stop()


class TestFleetMonitorScreenCompose(unittest.TestCase):
    @patch("tui.screens.fleet_monitor.OutputPanel")
    @patch("tui.screens.fleet_monitor.VerticalScroll")
    @patch("tui.screens.fleet_monitor.Static")
    @patch("tui.screens.fleet_monitor.Horizontal")
    @patch("tui.screens.fleet_monitor.Container")
    @patch("tui.screens.fleet_monitor.ETXHeader")
    def test_compose_yields_widgets(self, *mocks):
        screen = FleetMonitorScreen()
        result = list(screen.compose())
        assert len(result) > 0


# ===========================================================================
# FleetMonitorScreen on_mount tests
# ===========================================================================


class TestFleetMonitorScreenOnMount(unittest.TestCase):
    def setUp(self):
        self.screen = FleetMonitorScreen(
            operation_type="squad",
            operation_id="dev-squad",
            members=["architect", "debugger"],
            task_description="",
        )
        self.mock_app = _make_mock_app()
        self._patcher = _patch_screen_app(self.screen, self.mock_app)

        self.mock_header = Mock()
        self.mock_title = Mock()
        self.mock_stats = Mock()
        self.mock_grid = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            if hasattr(selector, "__name__"):
                return self.mock_header
            mapping = {
                "#monitor-title": self.mock_title,
                "#monitor-stats": self.mock_stats,
                "#monitor-grid": self.mock_grid,
            }
            return mapping.get(selector, Mock())

        self.screen.query_one = Mock(side_effect=query_one_side_effect)
        self.screen.run_worker = Mock()

    def tearDown(self):
        self._patcher.stop()

    def test_header_configured(self):
        self.screen.on_mount()
        assert self.mock_header.project == "test-project"
        assert self.mock_header.branch == "main"
        assert self.mock_header.provider == "claude"

    def test_title_set(self):
        self.screen.on_mount()
        call_args = self.mock_title.update.call_args[0][0]
        assert "Squad" in call_args
        assert "dev-squad" in call_args

    def test_stats_set(self):
        self.screen.on_mount()
        call_args = self.mock_stats.update.call_args[0][0]
        assert "2 agents" in call_args

    def test_grid_rows_created(self):
        self.screen.on_mount()
        assert self.mock_grid.mount.call_count == 2

    def test_no_worker_when_no_task(self):
        self.screen.on_mount()
        self.screen.run_worker.assert_not_called()

    def test_worker_started_when_task_provided(self):
        self.screen.task_description = "build feature"
        self.screen.on_mount()
        self.screen.run_worker.assert_called_once()


# ===========================================================================
# _execute_operation tests
# ===========================================================================


class TestFleetMonitorExecuteOperation(unittest.TestCase):
    def setUp(self):
        self.screen = FleetMonitorScreen(
            operation_type="squad",
            operation_id="dev-squad",
            members=["architect", "debugger"],
            task_description="build it",
        )
        self.mock_app = _make_mock_app()
        self._patcher = _patch_screen_app(self.screen, self.mock_app)
        self.mock_output = Mock()

        # Mock query_one to handle OutputPanel, Container, and ShortcutBar queries
        mock_container = Mock()
        mock_container.mount = Mock(return_value=None)

        def query_one_side_effect(selector, *args, **kwargs):
            if selector == "#monitor-output":
                return self.mock_output
            if selector == "#fleet-monitor":
                return mock_container
            # For ShortcutBar and other class selectors
            return Mock()

        self.screen.query_one = Mock(side_effect=query_one_side_effect)
        self.screen.notify = Mock()
        # Mock _show_next_actions to avoid Input.focus() needing active Textual app
        self.screen._show_next_actions = Mock()

    def tearDown(self):
        self._patcher.stop()

    def _run(self):
        loop = asyncio.new_event_loop()
        try:
            loop.run_until_complete(self.screen._execute_operation())
        finally:
            loop.close()

    @patch("tui.screens.fleet_monitor.run_squad")
    def test_squad_operation_success(self, mock_run):
        async def fake_stream(*a, **kw):
            yield "[euxis] Agent: architect started"
            yield "doing work"
            yield "[euxis] Agent: debugger started"
            yield "more work"

        mock_run.return_value = fake_stream()
        # Mock _set_agent_status to avoid query issues
        self.screen._set_agent_status = Mock()
        self._run()

        mock_run.assert_called_once_with("dev-squad", "build it")
        assert self.mock_output.write_line.call_count == 4
        self.screen.notify.assert_called_once()

    @patch("tui.screens.fleet_monitor.run_combo")
    def test_combo_operation(self, mock_run):
        self.screen.operation_type = "combo"

        async def fake_stream(*a, **kw):
            yield "combo output"

        mock_run.return_value = fake_stream()
        self.screen._set_agent_status = Mock()
        self._run()

        mock_run.assert_called_once_with("dev-squad", "build it")

    @patch("tui.screens.fleet_monitor.run_squad")
    def test_agent_status_transitions(self, mock_run):
        async def fake_stream(*a, **kw):
            yield "[euxis] Agent: architect"
            yield "[euxis] Agent: debugger"

        mock_run.return_value = fake_stream()
        self.screen._set_agent_status = Mock()
        self._run()

        # architect should be set to running then complete, debugger to running then complete
        calls = self.screen._set_agent_status.call_args_list
        assert any(c[0][0] == "architect" for c in calls)
        assert any(c[0][0] == "debugger" for c in calls)

    @patch("tui.screens.fleet_monitor.run_squad")
    def test_error_handling(self, mock_run):
        async def error_stream(*a, **kw):
            msg = "connection failed"
            raise OSError(msg)
            yield

        mock_run.return_value = error_stream()
        self.screen._set_agent_status = Mock()
        self._run()

        error_calls = [c[0][0] for c in self.mock_output.write_status.call_args_list]
        assert any("connection failed" in c for c in error_calls)

    @patch("tui.screens.fleet_monitor.run_squad")
    def test_runtime_error(self, mock_run):
        async def error_stream(*a, **kw):
            msg = "timeout"
            raise RuntimeError(msg)
            yield

        mock_run.return_value = error_stream()
        self.screen._set_agent_status = Mock()
        self._run()

        self.screen.notify.assert_called_once()
        assert "error" in self.screen.notify.call_args[1].get("severity", "")


# ===========================================================================
# _set_agent_status tests
# ===========================================================================


class TestFleetMonitorSetAgentStatus(unittest.TestCase):
    def test_sets_status_on_found_row(self):
        screen = FleetMonitorScreen()
        mock_row = Mock()
        screen.query_one = Mock(return_value=mock_row)

        screen._set_agent_status("architect", "running", 50)
        mock_row.set_status.assert_called_once_with("running", 50)

    def test_nomatches_handled_gracefully(self):
        from textual.css.query import NoMatches

        screen = FleetMonitorScreen()
        screen.query_one = Mock(side_effect=NoMatches("not found"))

        # Should not raise
        screen._set_agent_status("missing-agent", "complete")


# ===========================================================================
# Actions
# ===========================================================================


class TestFleetMonitorActions(unittest.TestCase):
    def test_action_go_back(self):
        screen = FleetMonitorScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)
        try:
            screen.action_go_back()
            mock_app.pop_screen.assert_called_once()
        finally:
            patcher.stop()


if __name__ == "__main__":
    unittest.main()
