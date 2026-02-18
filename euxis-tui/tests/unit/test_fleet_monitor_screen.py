# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Comprehensive unit tests for FleetMonitorScreen.

Tests cover: AgentMonitorRow (init, compose, set_status for all states,
status property, CSS), FleetMonitorScreen (init, compose, on_mount,
_get_stream, _set_active_agent, _handle_step_line, _handle_agent_line,
_handle_delegate_line, _handle_error_line, _handle_throttle_line,
_mark_remaining_complete, _finish_success, _finish_error,
_show_next_actions, on_input_submitted, _execute_operation for
squad/combo/step/delegate/throttle/cooling/error paths, actions).
"""

from __future__ import annotations

import asyncio
import unittest
from unittest.mock import Mock, PropertyMock, patch, MagicMock

from textual.css.query import NoMatches

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

    def test_init_with_kwargs(self):
        row = AgentMonitorRow("debugger", id="row-debugger")
        assert row.agent_id == "debugger"
        assert row.id == "row-debugger"

    @patch("tui.screens.fleet_monitor.ProgressBar")
    @patch("tui.screens.fleet_monitor.Static")
    def test_compose(self, mock_static, mock_progress):
        row = AgentMonitorRow("debugger")
        result = list(row.compose())
        assert len(result) == 3

    def test_status_property(self):
        """Test status property returns _status."""
        row = AgentMonitorRow("architect")
        assert row.status == "pending"
        row._status = "running"
        assert row.status == "running"

    def test_css_is_defined(self):
        """Test DEFAULT_CSS is a non-empty string."""
        assert isinstance(AgentMonitorRow.DEFAULT_CSS, str)
        assert len(AgentMonitorRow.DEFAULT_CSS) > 0

    def test_css_sets_height(self):
        assert "height: 3" in AgentMonitorRow.DEFAULT_CSS

    def _make_row_with_mocks(self, agent_id="architect"):
        row = AgentMonitorRow(agent_id)
        mock_status = Mock()
        mock_progress = Mock()
        row.query = Mock(return_value=Mock(first=Mock(return_value=mock_status)))
        row.query_one = Mock(return_value=mock_progress)
        return row, mock_status, mock_progress

    def test_set_status_running(self):
        row, mock_status, mock_progress = self._make_row_with_mocks()
        row.set_status("running", 50)
        assert row._status == "running"
        assert "Running" in mock_status.update.call_args[0][0]
        assert mock_progress.progress == 50

    def test_set_status_complete(self):
        row, mock_status, mock_progress = self._make_row_with_mocks()
        row.set_status("complete")
        assert row._status == "complete"
        assert "Complete" in mock_status.update.call_args[0][0]
        assert mock_progress.progress == 100

    def test_set_status_error(self):
        row, mock_status, mock_progress = self._make_row_with_mocks()
        row.set_status("error")
        assert row._status == "error"
        assert "Error" in mock_status.update.call_args[0][0]

    def test_set_status_throttled(self):
        """Test throttled status displays correctly."""
        row, mock_status, mock_progress = self._make_row_with_mocks()
        row.set_status("throttled")
        assert row._status == "throttled"
        text = mock_status.update.call_args[0][0]
        assert "Throttled" in text

    def test_set_status_cooling(self):
        """Test cooling status displays correctly."""
        row, mock_status, mock_progress = self._make_row_with_mocks()
        row.set_status("cooling")
        assert row._status == "cooling"
        text = mock_status.update.call_args[0][0]
        assert "Cooling" in text

    def test_set_status_queued(self):
        """Test queued status displays correctly."""
        row, mock_status, mock_progress = self._make_row_with_mocks()
        row.set_status("queued")
        assert row._status == "queued"
        text = mock_status.update.call_args[0][0]
        assert "Queued" in text

    def test_set_status_pending(self):
        row, mock_status, mock_progress = self._make_row_with_mocks()
        row.set_status("pending")
        assert row._status == "pending"
        assert "Pending" in mock_status.update.call_args[0][0]
        assert mock_progress.progress == 0

    def test_set_status_unknown(self):
        """Unknown status falls through to pending branch."""
        row, mock_status, mock_progress = self._make_row_with_mocks()
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

    def test_members_default_to_empty_list(self):
        """Test members defaults to empty list when None."""
        screen = FleetMonitorScreen(members=None)
        assert screen.members == []

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
    @patch("tui.screens.fleet_monitor.ShortcutBar")
    @patch("tui.screens.fleet_monitor.OutputPanel")
    @patch("tui.screens.fleet_monitor.ResourceMonitor")
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
        self.mock_resource = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            if hasattr(selector, "__name__"):
                if selector.__name__ == "ETXHeader":
                    return self.mock_header
                if selector.__name__ == "ResourceMonitor":
                    return self.mock_resource
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

    def test_title_combo_operation(self):
        """Test title uses Combo label for combo operation type."""
        self.screen.operation_type = "combo"
        self.screen.on_mount()
        call_args = self.mock_title.update.call_args[0][0]
        assert "Combo" in call_args

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

    def test_header_with_no_branch(self):
        """Test header branch set to empty when git_branch is None."""
        self.mock_app.git_branch = None
        self.screen.on_mount()
        assert self.mock_header.branch == ""


# ===========================================================================
# _get_stream tests
# ===========================================================================


class TestFleetMonitorGetStream(unittest.TestCase):
    """Tests for _get_stream() method."""

    @patch("tui.screens.fleet_monitor.run_squad")
    def test_get_stream_squad(self, mock_run_squad):
        """Test _get_stream returns run_squad for squad operation."""
        screen = FleetMonitorScreen(operation_type="squad", operation_id="dev", task_description="test")
        mock_run_squad.return_value = "squad-stream"
        result = screen._get_stream()
        mock_run_squad.assert_called_once_with("dev", "test")
        assert result == "squad-stream"

    @patch("tui.screens.fleet_monitor.run_combo")
    def test_get_stream_combo(self, mock_run_combo):
        """Test _get_stream returns run_combo for combo operation."""
        screen = FleetMonitorScreen(operation_type="combo", operation_id="build", task_description="test")
        mock_run_combo.return_value = "combo-stream"
        result = screen._get_stream()
        mock_run_combo.assert_called_once_with("build", "test")
        assert result == "combo-stream"


# ===========================================================================
# _set_active_agent tests
# ===========================================================================


class TestFleetMonitorSetActiveAgent(unittest.TestCase):
    """Tests for _set_active_agent() method."""

    def setUp(self):
        self.screen = FleetMonitorScreen(members=["a", "b", "c"])
        self.screen._set_agent_status = Mock()

    def test_set_active_agent_first_time(self):
        """Test first agent activation with no current agent."""
        agent, count = self.screen._set_active_agent(None, "a", 0, 50)
        assert agent == "a"
        assert count == 0
        self.screen._set_agent_status.assert_called_once_with("a", "running", 50)

    def test_set_active_agent_transition(self):
        """Test transitioning from one agent to another completes the previous one."""
        agent, count = self.screen._set_active_agent("a", "b", 0, 60)
        assert agent == "b"
        assert count == 1
        calls = self.screen._set_agent_status.call_args_list
        assert calls[0] == (("a", "complete"),)
        assert calls[1] == (("b", "running", 60),)

    def test_set_active_agent_same_agent(self):
        """Test setting same agent does not mark complete."""
        agent, count = self.screen._set_active_agent("a", "a", 0, 70)
        assert agent == "a"
        assert count == 0
        # Only one call: running update
        self.screen._set_agent_status.assert_called_once_with("a", "running", 70)

    def test_set_active_agent_minimum_progress(self):
        """Test progress is at least 10."""
        agent, count = self.screen._set_active_agent(None, "a", 0, 5)
        self.screen._set_agent_status.assert_called_with("a", "running", 10)


# ===========================================================================
# _handle_step_line tests
# ===========================================================================


class TestFleetMonitorHandleStepLine(unittest.TestCase):
    """Tests for _handle_step_line() method."""

    def setUp(self):
        self.screen = FleetMonitorScreen(members=["planner", "coder"])
        self.screen._set_agent_status = Mock()

    def test_step_line_match(self):
        """Test matching a combo-style step line."""
        agent, count, handled = self.screen._handle_step_line(
            "  Step 1/4: planner", None, 0
        )
        assert handled is True
        assert agent == "planner"

    def test_step_line_no_match(self):
        """Test non-step line returns handled=False."""
        agent, count, handled = self.screen._handle_step_line(
            "some other output", None, 0
        )
        assert handled is False
        assert agent is None
        assert count == 0

    def test_step_line_progress_calculation(self):
        """Test progress is calculated as step/total * 100."""
        self.screen._set_active_agent = Mock(return_value=("planner", 0))
        self.screen._handle_step_line("  Step 2/4: planner", None, 0)
        call_args = self.screen._set_active_agent.call_args
        assert call_args[0][3] == 50  # 2/4 * 100 = 50

    def test_step_line_last_step(self):
        """Test progress for last step is 100%."""
        self.screen._set_active_agent = Mock(return_value=("coder", 1))
        self.screen._handle_step_line("  Step 4/4: coder", "planner", 0)
        call_args = self.screen._set_active_agent.call_args
        assert call_args[0][3] == 100


# ===========================================================================
# _handle_agent_line tests
# ===========================================================================


class TestFleetMonitorHandleAgentLine(unittest.TestCase):
    """Tests for _handle_agent_line() method."""

    def setUp(self):
        self.screen = FleetMonitorScreen(members=["orchestrator", "coder"])
        self.screen._set_agent_status = Mock()

    def test_agent_line_match(self):
        """Test matching a dispatch-style agent line."""
        agent, count, handled = self.screen._handle_agent_line(
            "[euxis] dispatching Agent: orchestrator", None, 0, 2
        )
        assert handled is True
        assert agent == "orchestrator"

    def test_agent_line_no_agent_keyword(self):
        """Test line without 'Agent:' returns handled=False."""
        agent, count, handled = self.screen._handle_agent_line(
            "[euxis] some other line", None, 0, 2
        )
        assert handled is False

    def test_agent_line_agent_keyword_but_no_regex_match(self):
        """Test line with 'Agent:' but no regex match."""
        agent, count, handled = self.screen._handle_agent_line(
            "Agent: ", None, 0, 2
        )
        assert handled is False

    def test_agent_line_progress_calculation(self):
        """Test progress calculation based on completed count / total."""
        self.screen._set_active_agent = Mock(return_value=("coder", 1))
        self.screen._handle_agent_line(
            "[euxis] Agent: coder", "orchestrator", 1, 4
        )
        call_args = self.screen._set_active_agent.call_args
        # (1+1)/4 * 100 = 50
        assert call_args[0][3] == 50


# ===========================================================================
# _handle_delegate_line tests
# ===========================================================================


class TestFleetMonitorHandleDelegateLine(unittest.TestCase):
    """Tests for _handle_delegate_line() method."""

    def setUp(self):
        self.screen = FleetMonitorScreen(members=["planner", "sub-agent"])
        self.screen._set_agent_status = Mock()

    def test_delegate_line_match(self):
        """Test matching a delegation line."""
        agent, count, handled = self.screen._handle_delegate_line(
            "Delegating to sub-agent for analysis", None, 0
        )
        assert handled is True
        assert agent == "sub-agent"

    def test_delegate_line_no_keyword(self):
        """Test line without 'Delegating to' returns handled=False."""
        agent, count, handled = self.screen._handle_delegate_line(
            "Some other output line", None, 0
        )
        assert handled is False

    def test_delegate_line_keyword_but_no_regex_match(self):
        """Test line with 'Delegating to' but no regex match (edge case)."""
        agent, count, handled = self.screen._handle_delegate_line(
            "Delegating to ", None, 0
        )
        assert handled is False

    def test_delegate_line_uses_progress_50(self):
        """Test delegation uses fixed progress of 50."""
        self.screen._set_active_agent = Mock(return_value=("sub-agent", 0))
        self.screen._handle_delegate_line(
            "Delegating to sub-agent", None, 0
        )
        call_args = self.screen._set_active_agent.call_args
        assert call_args[0][3] == 50


# ===========================================================================
# _handle_error_line tests
# ===========================================================================


class TestFleetMonitorHandleErrorLine(unittest.TestCase):
    """Tests for _handle_error_line() method."""

    def setUp(self):
        self.screen = FleetMonitorScreen()
        self.screen._set_agent_status = Mock()

    def test_error_line_with_euxis_prefix_and_error(self):
        """Test error line with [euxis] prefix sets agent to error."""
        self.screen._handle_error_line("[euxis] error: something failed", "architect")
        self.screen._set_agent_status.assert_called_once_with("architect", "error")

    def test_error_line_without_euxis_prefix(self):
        """Test error line without [euxis] prefix is ignored."""
        self.screen._handle_error_line("error: something failed", "architect")
        self.screen._set_agent_status.assert_not_called()

    def test_error_line_no_current_agent(self):
        """Test error line with no current agent is ignored."""
        self.screen._handle_error_line("[euxis] error: something failed", None)
        self.screen._set_agent_status.assert_not_called()

    def test_error_line_no_error_keyword(self):
        """Test line without error keyword is ignored."""
        self.screen._handle_error_line("[euxis] running normally", "architect")
        self.screen._set_agent_status.assert_not_called()

    def test_error_line_failed_keyword(self):
        """Test 'failed' keyword also triggers error."""
        self.screen._handle_error_line("[euxis] task failed here", "architect")
        self.screen._set_agent_status.assert_called_once_with("architect", "error")

    def test_error_line_abort_keyword(self):
        """Test 'abort' keyword also triggers error."""
        self.screen._handle_error_line("[euxis] aborting operation", "architect")
        self.screen._set_agent_status.assert_called_once_with("architect", "error")


# ===========================================================================
# _handle_throttle_line tests
# ===========================================================================


class TestFleetMonitorHandleThrottleLine(unittest.TestCase):
    """Tests for _handle_throttle_line() method."""

    def setUp(self):
        self.screen = FleetMonitorScreen()
        self.screen._set_agent_status = Mock()

    def test_throttle_line_match(self):
        """Test [THROTTLE] line sets throttled status."""
        result = self.screen._handle_throttle_line("[THROTTLE] rate limited", "architect")
        assert result is True
        self.screen._set_agent_status.assert_called_with("architect", "throttled")

    def test_throttle_word_match(self):
        """Test 'throttled' word also matches."""
        result = self.screen._handle_throttle_line("request throttled", "architect")
        assert result is True
        self.screen._set_agent_status.assert_called_with("architect", "throttled")

    def test_throttle_no_current_agent(self):
        """Test throttle line without current agent does not set status."""
        result = self.screen._handle_throttle_line("[THROTTLE] rate limited", None)
        assert result is True
        self.screen._set_agent_status.assert_not_called()

    def test_cooling_line_match(self):
        """Test cooling line sets cooling status."""
        result = self.screen._handle_throttle_line("system cooling down", "architect")
        assert result is True
        self.screen._set_agent_status.assert_called_with("architect", "cooling")

    def test_overload_line_match(self):
        """Test 'overload' line matches cooling pattern."""
        result = self.screen._handle_throttle_line("system overload detected", "architect")
        assert result is True
        self.screen._set_agent_status.assert_called_with("architect", "cooling")

    def test_waiting_for_resources_match(self):
        """Test 'waiting for resources' matches cooling pattern."""
        result = self.screen._handle_throttle_line("waiting for resources to free up", "architect")
        assert result is True
        self.screen._set_agent_status.assert_called_with("architect", "cooling")

    def test_cooling_no_current_agent(self):
        """Test cooling line without current agent."""
        result = self.screen._handle_throttle_line("system cooling down", None)
        assert result is True
        self.screen._set_agent_status.assert_not_called()

    def test_no_match(self):
        """Test normal line returns False."""
        result = self.screen._handle_throttle_line("normal output", "architect")
        assert result is False
        self.screen._set_agent_status.assert_not_called()


# ===========================================================================
# _mark_remaining_complete tests
# ===========================================================================


class TestFleetMonitorMarkRemainingComplete(unittest.TestCase):
    """Tests for _mark_remaining_complete() method."""

    def test_marks_pending_agents_complete(self):
        screen = FleetMonitorScreen(members=["a", "b"])
        mock_row_a = Mock()
        mock_row_a.status = "pending"
        mock_row_b = Mock()
        mock_row_b.status = "pending"

        def query_one_side(selector, *args, **kwargs):
            if "#monitor-a" in selector:
                return mock_row_a
            if "#monitor-b" in selector:
                return mock_row_b
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side)
        screen._mark_remaining_complete()
        mock_row_a.set_status.assert_called_once_with("complete")
        mock_row_b.set_status.assert_called_once_with("complete")

    def test_skips_non_pending_agents(self):
        """Test already-complete agents are not re-marked."""
        screen = FleetMonitorScreen(members=["a"])
        mock_row = Mock()
        mock_row.status = "complete"
        screen.query_one = Mock(return_value=mock_row)
        screen._mark_remaining_complete()
        mock_row.set_status.assert_not_called()

    def test_handles_nomatches_gracefully(self):
        """Test NoMatches exceptions are caught."""
        screen = FleetMonitorScreen(members=["a"])
        screen.query_one = Mock(side_effect=NoMatches("not found"))
        # Should not raise
        screen._mark_remaining_complete()


# ===========================================================================
# _finish_success tests
# ===========================================================================


class TestFleetMonitorFinishSuccess(unittest.TestCase):
    """Tests for _finish_success() method."""

    def test_finish_success(self):
        screen = FleetMonitorScreen()
        mock_output = Mock()
        screen._show_next_actions = Mock()
        screen.notify = Mock()
        screen._finish_success(mock_output)

        mock_output.write_separator.assert_called_once()
        mock_output.write_status.assert_called_once()
        assert "complete" in mock_output.write_status.call_args[0][0].lower()
        screen.notify.assert_called_once()
        assert screen.notify.call_args[1]["severity"] == "information"
        screen._show_next_actions.assert_called_once_with(mock_output)


# ===========================================================================
# _finish_error tests
# ===========================================================================


class TestFleetMonitorFinishError(unittest.TestCase):
    """Tests for _finish_error() method."""

    def test_finish_error_with_current_agent(self):
        screen = FleetMonitorScreen()
        screen._set_agent_status = Mock()
        screen._show_next_actions = Mock()
        screen.notify = Mock()
        mock_output = Mock()
        exc = OSError("disk full")

        screen._finish_error(mock_output, exc, "architect")

        screen._set_agent_status.assert_called_once_with("architect", "error")
        mock_output.write_separator.assert_called_once()
        assert "disk full" in mock_output.write_status.call_args[0][0]
        screen.notify.assert_called_once()
        assert screen.notify.call_args[1]["severity"] == "error"
        screen._show_next_actions.assert_called_once_with(mock_output)

    def test_finish_error_without_current_agent(self):
        screen = FleetMonitorScreen()
        screen._set_agent_status = Mock()
        screen._show_next_actions = Mock()
        screen.notify = Mock()
        mock_output = Mock()
        exc = RuntimeError("timeout")

        screen._finish_error(mock_output, exc, None)

        screen._set_agent_status.assert_not_called()
        mock_output.write_separator.assert_called_once()
        assert "timeout" in mock_output.write_status.call_args[0][0]


# ===========================================================================
# _show_next_actions tests
# ===========================================================================


class TestFleetMonitorShowNextActions(unittest.TestCase):
    """Tests for _show_next_actions() method."""

    def test_show_next_actions_writes_hints(self):
        screen = FleetMonitorScreen(operation_id="dev-squad")
        mock_output = Mock()
        mock_container = Mock()
        mock_input = Mock()
        mock_bar = Mock()

        def query_one_side(selector, *args, **kwargs):
            if selector == "#fleet-monitor":
                return mock_container
            # ShortcutBar is a class selector
            return mock_bar

        screen.query_one = Mock(side_effect=query_one_side)

        with patch("tui.screens.fleet_monitor.Input", return_value=mock_input):
            screen._show_next_actions(mock_output)

        mock_output.write_separator.assert_called_once()
        mock_output.write_status.assert_called()
        mock_output.write_line.assert_called()
        mock_container.mount.assert_called_once_with(mock_input)
        mock_input.focus.assert_called_once()

    def test_show_next_actions_handles_nomatches_for_shortcut_bar(self):
        """Test NoMatches for ShortcutBar is handled gracefully."""
        screen = FleetMonitorScreen(operation_id="dev-squad")
        mock_output = Mock()
        mock_container = Mock()
        mock_input = Mock()

        call_count = [0]
        def query_one_side(selector, *args, **kwargs):
            if selector == "#fleet-monitor":
                return mock_container
            call_count[0] += 1
            raise NoMatches("not found")

        screen.query_one = Mock(side_effect=query_one_side)

        with patch("tui.screens.fleet_monitor.Input", return_value=mock_input):
            # Should not raise
            screen._show_next_actions(mock_output)


# ===========================================================================
# on_input_submitted tests
# ===========================================================================


class TestFleetMonitorOnInputSubmitted(unittest.TestCase):
    """Tests for on_input_submitted() method."""

    def _run_async(self, coro):
        loop = asyncio.new_event_loop()
        try:
            return loop.run_until_complete(coro)
        finally:
            loop.close()

    def test_wrong_input_id_ignored(self):
        """Test input submissions from non-task inputs are ignored."""
        screen = FleetMonitorScreen()
        event = Mock()
        event.input.id = "other-input"
        self._run_async(screen.on_input_submitted(event))
        # No crash, no re-run
        event.input.remove.assert_not_called()

    def test_empty_task_ignored(self):
        """Test empty task description is ignored."""
        screen = FleetMonitorScreen()
        event = Mock()
        event.input.id = "next-task-input"
        event.value = "   "
        self._run_async(screen.on_input_submitted(event))
        event.input.remove.assert_not_called()

    def test_valid_task_triggers_rerun(self):
        """Test valid task triggers re-run."""
        screen = FleetMonitorScreen(
            operation_id="dev-squad",
            members=["a", "b"],
        )
        screen._set_agent_status = Mock()
        mock_output = Mock()
        screen.query_one = Mock(return_value=mock_output)
        screen.run_worker = Mock()

        event = Mock()
        event.input.id = "next-task-input"
        event.value = "new task"

        self._run_async(screen.on_input_submitted(event))

        assert screen.task_description == "new task"
        event.input.remove.assert_called_once()
        # Agent statuses reset to pending
        assert screen._set_agent_status.call_count == 2
        for call in screen._set_agent_status.call_args_list:
            assert call[0][1] == "pending"
        mock_output.clear.assert_called_once()
        screen.run_worker.assert_called_once()


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
        screen = FleetMonitorScreen()
        screen.query_one = Mock(side_effect=NoMatches("not found"))
        # Should not raise
        screen._set_agent_status("missing-agent", "complete")

    def test_default_progress_zero(self):
        """Test default progress is 0 when not specified."""
        screen = FleetMonitorScreen()
        mock_row = Mock()
        screen.query_one = Mock(return_value=mock_row)
        screen._set_agent_status("architect", "complete")
        mock_row.set_status.assert_called_once_with("complete", 0)


# ===========================================================================
# Regex pattern tests
# ===========================================================================


class TestFleetMonitorRegexPatterns(unittest.TestCase):
    """Tests for the compiled regex patterns."""

    def test_re_step_matches(self):
        match = FleetMonitorScreen._RE_STEP.search("  Step 2/4: planner")
        assert match is not None
        assert match.group(1) == "2"
        assert match.group(2) == "4"
        assert match.group(3) == "planner"

    def test_re_step_no_match(self):
        match = FleetMonitorScreen._RE_STEP.search("no step here")
        assert match is None

    def test_re_agent_matches(self):
        match = FleetMonitorScreen._RE_AGENT.search("Agent: orchestrator")
        assert match is not None
        assert match.group(1) == "orchestrator"

    def test_re_agent_no_match(self):
        match = FleetMonitorScreen._RE_AGENT.search("no agent here")
        assert match is None

    def test_re_delegating_matches(self):
        match = FleetMonitorScreen._RE_DELEGATING.search("Delegating to sub-agent")
        assert match is not None
        assert match.group(1) == "sub-agent"

    def test_re_complete_matches(self):
        assert FleetMonitorScreen._RE_COMPLETE.search("task complete")
        assert FleetMonitorScreen._RE_COMPLETE.search("finished processing")
        assert FleetMonitorScreen._RE_COMPLETE.search("All done here")

    def test_re_error_matches(self):
        assert FleetMonitorScreen._RE_ERROR.search("error occurred")
        assert FleetMonitorScreen._RE_ERROR.search("task failed")
        assert FleetMonitorScreen._RE_ERROR.search("abort operation")

    def test_re_throttle_matches(self):
        assert FleetMonitorScreen._RE_THROTTLE.search("[THROTTLE] slow down")
        assert FleetMonitorScreen._RE_THROTTLE.search("request throttled")

    def test_re_cooling_matches(self):
        assert FleetMonitorScreen._RE_COOLING.search("cooling period")
        assert FleetMonitorScreen._RE_COOLING.search("system overload")
        assert FleetMonitorScreen._RE_COOLING.search("waiting for resources")

    def test_re_queued_matches(self):
        assert FleetMonitorScreen._RE_QUEUED.search("[QUEUE] pending")
        assert FleetMonitorScreen._RE_QUEUED.search("task queued")
        assert FleetMonitorScreen._RE_QUEUED.search("waiting for slot")


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

        mock_container = Mock()
        mock_container.mount = Mock(return_value=None)

        def query_one_side_effect(selector, *args, **kwargs):
            if selector == "#monitor-output":
                return self.mock_output
            if selector == "#fleet-monitor":
                return mock_container
            return Mock()

        self.screen.query_one = Mock(side_effect=query_one_side_effect)
        self.screen.notify = Mock()
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

        calls = self.screen._set_agent_status.call_args_list
        assert any(c[0][0] == "architect" for c in calls)
        assert any(c[0][0] == "debugger" for c in calls)

    @patch("tui.screens.fleet_monitor.run_squad")
    def test_error_handling(self, mock_run):
        async def error_stream(*a, **kw):
            msg = "connection failed"
            raise OSError(msg)
            yield  # noqa: unreachable

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
            yield  # noqa: unreachable

        mock_run.return_value = error_stream()
        self.screen._set_agent_status = Mock()
        self._run()

        self.screen.notify.assert_called_once()
        assert "error" in self.screen.notify.call_args[1].get("severity", "")

    @patch("tui.screens.fleet_monitor.run_squad")
    def test_value_error(self, mock_run):
        """Test ValueError is caught."""
        async def error_stream(*a, **kw):
            msg = "bad value"
            raise ValueError(msg)
            yield  # noqa: unreachable

        mock_run.return_value = error_stream()
        self.screen._set_agent_status = Mock()
        self._run()

        self.screen.notify.assert_called_once()
        assert "error" in self.screen.notify.call_args[1].get("severity", "")

    @patch("tui.screens.fleet_monitor.run_squad")
    def test_step_lines_handled(self, mock_run):
        """Test combo-style step lines are processed."""
        async def fake_stream(*a, **kw):
            yield "  Step 1/2: architect"
            yield "  Step 2/2: debugger"

        mock_run.return_value = fake_stream()
        self.screen._set_agent_status = Mock()
        self._run()

        calls = self.screen._set_agent_status.call_args_list
        agent_names = [c[0][0] for c in calls]
        assert "architect" in agent_names
        assert "debugger" in agent_names

    @patch("tui.screens.fleet_monitor.run_squad")
    def test_delegation_lines_handled(self, mock_run):
        """Test delegation lines are processed."""
        async def fake_stream(*a, **kw):
            yield "Delegating to architect for planning"

        mock_run.return_value = fake_stream()
        self.screen._set_agent_status = Mock()
        self._run()

        calls = self.screen._set_agent_status.call_args_list
        assert any(c[0][0] == "architect" for c in calls)

    @patch("tui.screens.fleet_monitor.run_squad")
    def test_throttle_lines_handled(self, mock_run):
        """Test throttle lines are processed."""
        async def fake_stream(*a, **kw):
            yield "[euxis] Agent: architect"
            yield "[THROTTLE] rate limited"

        mock_run.return_value = fake_stream()
        self.screen._set_agent_status = Mock()
        self._run()

        calls = self.screen._set_agent_status.call_args_list
        throttle_calls = [c for c in calls if len(c[0]) >= 2 and c[0][1] == "throttled"]
        assert len(throttle_calls) >= 1

    @patch("tui.screens.fleet_monitor.run_squad")
    def test_cooling_lines_handled(self, mock_run):
        """Test cooling lines are processed."""
        async def fake_stream(*a, **kw):
            yield "[euxis] Agent: architect"
            yield "system cooling down"

        mock_run.return_value = fake_stream()
        self.screen._set_agent_status = Mock()
        self._run()

        calls = self.screen._set_agent_status.call_args_list
        cooling_calls = [c for c in calls if len(c[0]) >= 2 and c[0][1] == "cooling"]
        assert len(cooling_calls) >= 1

    @patch("tui.screens.fleet_monitor.run_squad")
    def test_error_line_in_output(self, mock_run):
        """Test error lines in output set agent to error state."""
        async def fake_stream(*a, **kw):
            yield "[euxis] Agent: architect"
            yield "[euxis] error: compilation failed"

        mock_run.return_value = fake_stream()
        self.screen._set_agent_status = Mock()
        self._run()

        calls = self.screen._set_agent_status.call_args_list
        error_calls = [c for c in calls if len(c[0]) >= 2 and c[0][1] == "error"]
        assert len(error_calls) >= 1

    @patch("tui.screens.fleet_monitor.run_squad")
    def test_final_agent_marked_complete(self, mock_run):
        """Test the last active agent is marked complete after stream ends."""
        async def fake_stream(*a, **kw):
            yield "[euxis] Agent: architect"

        mock_run.return_value = fake_stream()
        self.screen._set_agent_status = Mock()
        self._run()

        calls = self.screen._set_agent_status.call_args_list
        # architect should be marked complete at end
        complete_calls = [c for c in calls if len(c[0]) >= 2 and c[0][1] == "complete"]
        assert len(complete_calls) >= 1

    @patch("tui.screens.fleet_monitor.run_squad")
    def test_empty_stream(self, mock_run):
        """Test empty stream completes without error."""
        async def fake_stream(*a, **kw):
            return
            yield  # noqa: unreachable

        mock_run.return_value = fake_stream()
        self.screen._set_agent_status = Mock()
        self._run()

        self.screen.notify.assert_called_once()
        assert "complete" in self.screen.notify.call_args[0][0].lower()

    @patch("tui.screens.fleet_monitor.run_squad")
    def test_mark_remaining_called(self, mock_run):
        """Test _mark_remaining_complete is called on success."""
        async def fake_stream(*a, **kw):
            yield "output"

        mock_run.return_value = fake_stream()
        self.screen._set_agent_status = Mock()
        self.screen._mark_remaining_complete = Mock()
        self._run()

        self.screen._mark_remaining_complete.assert_called_once()

    @patch("tui.screens.fleet_monitor.run_squad")
    def test_empty_members_total_fallback(self, mock_run):
        """Test total defaults to 1 when members is empty."""
        self.screen.members = []

        async def fake_stream(*a, **kw):
            yield "[euxis] Agent: architect"

        mock_run.return_value = fake_stream()
        self.screen._set_agent_status = Mock()
        self._run()

        # Should not divide by zero
        self.screen.notify.assert_called_once()


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
