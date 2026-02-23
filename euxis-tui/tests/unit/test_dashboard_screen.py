# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

#!/usr/bin/env python3
"""Comprehensive unit tests for DashboardScreen.

Tests initialization, bindings, compose structure, on_mount header/notification,
agent card selection handling, and action_focus_search.
"""

import unittest
from unittest.mock import Mock, PropertyMock, patch

from textual.css.query import NoMatches

from tui.core.config import ETXConfig
from tui.core.registry import Agent, FleetRegistry
from tui.screens.dashboard import DashboardScreen


def _patch_screen_app(screen, mock_app):
    """Bypass Textual's read-only app property for testing."""
    patcher = patch.object(
        type(screen), "app", new_callable=PropertyMock, return_value=mock_app
    )
    patcher.start()
    return patcher


class TestDashboardScreenInitialization(unittest.TestCase):
    """Tests for DashboardScreen class attributes and initialization."""

    def test_screen_initialization(self):
        """Test DashboardScreen can be instantiated."""
        screen = DashboardScreen()
        assert isinstance(screen, DashboardScreen)

    def test_bindings_count(self):
        """Test expected number of bindings."""
        screen = DashboardScreen()
        assert len(screen.BINDINGS) == 5

    def test_bindings_keys(self):
        """Test BINDINGS contains expected keys."""
        screen = DashboardScreen()
        keys = [b[0] for b in screen.BINDINGS]
        assert "ctrl+k" in keys
        assert "slash" in keys
        assert "f1" in keys
        assert "escape" in keys

    def test_binding_ctrl_k_action(self):
        """Test Ctrl+K maps to command palette."""
        screen = DashboardScreen()
        binding_map = {b[0]: b[1] for b in screen.BINDINGS}
        assert binding_map["ctrl+k"] == "app.command_palette"

    def test_binding_slash_action(self):
        """Test slash maps to focus_search."""
        screen = DashboardScreen()
        binding_map = {b[0]: b[1] for b in screen.BINDINGS}
        assert binding_map["slash"] == "focus_search"

    def test_binding_f1_action(self):
        """Test F1 maps to help."""
        screen = DashboardScreen()
        binding_map = {b[0]: b[1] for b in screen.BINDINGS}
        assert binding_map["f1"] == "app.action_help"

    def test_binding_escape_action(self):
        """Test Escape maps to app.quit."""
        screen = DashboardScreen()
        binding_map = {b[0]: b[1] for b in screen.BINDINGS}
        assert binding_map["escape"] == "app.quit"

    def test_binding_descriptions(self):
        """Test bindings have descriptive labels."""
        screen = DashboardScreen()
        descriptions = [b[2] for b in screen.BINDINGS]
        assert "Commands" in descriptions
        assert "Search" in descriptions
        assert "Help" in descriptions
        assert "Quit" in descriptions


class TestDashboardScreenEuxisAppProperty(unittest.TestCase):
    """Tests for the euxis_app property."""

    def setUp(self):
        self.mock_app = Mock()
        self._patcher = None

    def tearDown(self):
        if self._patcher:
            self._patcher.stop()

    def test_euxis_app_returns_app(self):
        """Test euxis_app property returns self.app."""
        screen = DashboardScreen()
        self._patcher = _patch_screen_app(screen, self.mock_app)
        assert screen.euxis_app is self.mock_app


class TestDashboardScreenCompose(unittest.TestCase):
    """Tests for the compose() method."""

    def setUp(self):
        self.mock_app = Mock()
        self.registry = FleetRegistry()
        self.registry.agents = [
            Agent(id="architect", tier="core", version="0.0.2", tags=(), activation="default"),
        ]
        self.mock_app.fleet_registry = self.registry
        self._patcher = None

    def tearDown(self):
        if self._patcher:
            self._patcher.stop()

    @patch("tui.screens.dashboard.FleetGrid")
    @patch("tui.screens.dashboard.TipBar")
    @patch("tui.screens.dashboard.ETXHeader")
    def test_compose_yields_widgets(self, mock_header, mock_tipbar, mock_grid):
        """Test compose() produces a non-empty widget list."""
        screen = DashboardScreen()
        self._patcher = _patch_screen_app(screen, self.mock_app)
        result = list(screen.compose())
        assert len(result) > 0

    @patch("tui.screens.dashboard.FleetGrid")
    @patch("tui.screens.dashboard.TipBar")
    @patch("tui.screens.dashboard.ETXHeader")
    def test_compose_creates_header(self, mock_header, mock_tipbar, mock_grid):
        """Test compose() creates ETXHeader with id='header'."""
        screen = DashboardScreen()
        self._patcher = _patch_screen_app(screen, self.mock_app)
        list(screen.compose())
        mock_header.assert_called_once_with(id="header")

    @patch("tui.screens.dashboard.FleetGrid")
    @patch("tui.screens.dashboard.TipBar")
    @patch("tui.screens.dashboard.ETXHeader")
    def test_compose_creates_fleet_grid(self, mock_header, mock_tipbar, mock_grid):
        """Test compose() creates FleetGrid with registry and id."""
        screen = DashboardScreen()
        self._patcher = _patch_screen_app(screen, self.mock_app)
        list(screen.compose())
        mock_grid.assert_called_once_with(self.registry, id="fleet-grid")

    @patch("tui.screens.dashboard.FleetGrid")
    @patch("tui.screens.dashboard.TipBar")
    @patch("tui.screens.dashboard.ETXHeader")
    def test_compose_creates_shortcut_bar(self, mock_header, mock_tipbar, mock_grid):
        """Test compose() creates a ShortcutBar."""
        screen = DashboardScreen()
        self._patcher = _patch_screen_app(screen, self.mock_app)
        result = list(screen.compose())
        from tui.widgets.shortcut_bar import ShortcutBar
        assert any(isinstance(w, ShortcutBar) for w in result)


class TestDashboardScreenOnMount(unittest.TestCase):
    """Tests for the on_mount() lifecycle method."""

    def setUp(self):
        self.mock_app = Mock()
        self.mock_app.project_name = "my-project"
        self.mock_app.git_branch = "feature/test"
        self.mock_app.config = ETXConfig(default_provider="gemini")
        self.registry = FleetRegistry()
        self.registry.version = "1.0.0"
        self.registry.agents = [
            Agent(id="architect", tier="core", version="1.0.0", tags=(), activation="default"),
            Agent(id="debugger", tier="fleet", version="1.0.0", tags=(), activation="default"),
            Agent(id="tester", tier="fleet", version="1.0.0", tags=(), activation="default"),
        ]
        self.mock_app.fleet_registry = self.registry
        self._patcher = None

    def tearDown(self):
        if self._patcher:
            self._patcher.stop()

    def _create_mounted_screen(self):
        """Create a DashboardScreen with mocked query_one, notify, and app."""
        screen = DashboardScreen()
        self._patcher = _patch_screen_app(screen, self.mock_app)

        mock_header = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            from tui.widgets.header import ETXHeader as HeaderCls

            if selector is HeaderCls:
                return mock_header
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)
        screen.notify = Mock()
        return screen, mock_header

    def test_on_mount_sets_header_project(self):
        """Test on_mount sets header.project from app.project_name."""
        screen, mock_header = self._create_mounted_screen()
        screen.on_mount()
        assert mock_header.project == "my-project"

    def test_on_mount_sets_header_branch(self):
        """Test on_mount sets header.branch from app.git_branch."""
        screen, mock_header = self._create_mounted_screen()
        screen.on_mount()
        assert mock_header.branch == "feature/test"

    def test_on_mount_sets_header_provider(self):
        """Test on_mount sets header.provider from config."""
        screen, mock_header = self._create_mounted_screen()
        screen.on_mount()
        assert mock_header.provider == "gemini"

    def test_on_mount_sets_header_agent_count(self):
        """Test on_mount sets header.agent_count from registry."""
        screen, mock_header = self._create_mounted_screen()
        screen.on_mount()
        assert mock_header.agent_count == 3

    def test_on_mount_sets_header_version(self):
        """Test on_mount sets header.version from registry."""
        screen, mock_header = self._create_mounted_screen()
        screen.on_mount()
        assert mock_header.version == "1.0.0"

    def test_on_mount_sends_notification(self):
        """Test on_mount sends a notification with agent count."""
        screen, _ = self._create_mounted_screen()
        screen.on_mount()
        screen.notify.assert_called_once()
        call_args = screen.notify.call_args
        assert "3 agents ready" in call_args[0][0]
        assert call_args[1]["timeout"] == 3

    def test_on_mount_with_no_git_branch(self):
        """Test on_mount handles None git_branch gracefully."""
        self.mock_app.git_branch = None
        screen, mock_header = self._create_mounted_screen()
        screen.on_mount()
        assert mock_header.branch == ""


class TestDashboardScreenAgentCardSelected(unittest.TestCase):
    """Tests for the on_agent_card_selected event handler."""

    def setUp(self):
        self.mock_app = Mock()
        self._patcher = None

    def tearDown(self):
        if self._patcher:
            self._patcher.stop()

    def test_agent_card_selected_deploys_agent(self):
        """Test on_agent_card_selected calls action_deploy_agent."""
        screen = DashboardScreen()
        self._patcher = _patch_screen_app(screen, self.mock_app)

        mock_event = Mock()
        mock_event.agent.id = "architect"
        screen.on_agent_card_selected(mock_event)

        self.mock_app.action_deploy_agent.assert_called_once_with("architect")

    def test_agent_card_selected_with_different_agent(self):
        """Test on_agent_card_selected passes correct agent ID."""
        screen = DashboardScreen()
        self._patcher = _patch_screen_app(screen, self.mock_app)

        mock_event = Mock()
        mock_event.agent.id = "pentester"
        screen.on_agent_card_selected(mock_event)

        self.mock_app.action_deploy_agent.assert_called_once_with("pentester")


class TestDashboardScreenActionFocusSearch(unittest.TestCase):
    """Tests for the action_focus_search() method."""

    def setUp(self):
        self._patcher = None

    def tearDown(self):
        if self._patcher:
            self._patcher.stop()

    def test_focus_search_focuses_input(self):
        """Test action_focus_search focuses the search input."""
        screen = DashboardScreen()
        mock_grid = Mock()
        mock_search_input = Mock()
        mock_grid.query_one.return_value = mock_search_input

        def query_one_side_effect(selector, *args, **kwargs):
            from tui.widgets.fleet_grid import FleetGrid as GridCls

            if selector is GridCls:
                return mock_grid
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)
        screen.action_focus_search()
        mock_search_input.focus.assert_called_once()

    def test_focus_search_handles_no_matches(self):
        """Test action_focus_search gracefully handles NoMatches."""
        screen = DashboardScreen()
        mock_grid = Mock()
        mock_grid.query_one.side_effect = NoMatches()

        def query_one_side_effect(selector, *args, **kwargs):
            from tui.widgets.fleet_grid import FleetGrid as GridCls

            if selector is GridCls:
                return mock_grid
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)
        # Should not raise
        screen.action_focus_search()


class TestTipBar(unittest.TestCase):
    """Tests for the TipBar widget."""

    @patch("tui.screens.dashboard.random")
    def test_tip_bar_initialization(self, mock_random):
        """Test TipBar stores tips and selects random initial index."""
        mock_random.randint.return_value = 2
        from tui.screens.dashboard import TipBar
        tips = ["Tip A", "Tip B", "Tip C"]
        bar = TipBar(tips)
        assert bar._tips == tips
        assert bar._index == 2

    def test_tip_bar_empty_tips(self):
        """Test TipBar handles empty tips list."""
        from tui.screens.dashboard import TipBar
        bar = TipBar([])
        bar.update = Mock()
        bar._show_tip()
        bar.update.assert_called_once()

    def test_tip_bar_rotate(self):
        """Test _rotate advances index cyclically."""
        from tui.screens.dashboard import TipBar
        bar = TipBar(["A", "B", "C"])
        bar._index = 0
        bar.update = Mock()
        bar._rotate()
        assert bar._index == 1
        bar._index = 2
        bar._rotate()
        assert bar._index == 0

    def test_tip_bar_show_tip(self):
        """Test _show_tip updates content with current tip."""
        from tui.screens.dashboard import TipBar
        bar = TipBar(["Hello"])
        bar._index = 0
        bar.update = Mock()
        bar._show_tip()
        call_content = bar.update.call_args[0][0]
        assert "Hello" in call_content

    def test_tip_bar_on_click_rotates(self):
        """Test clicking advances to next tip."""
        from tui.screens.dashboard import TipBar
        bar = TipBar(["A", "B"])
        bar._index = 0
        bar.update = Mock()
        bar.on_click()
        assert bar._index == 1


class TestDashboardEventHandlers(unittest.TestCase):
    """Tests for dashboard event handler methods."""

    def setUp(self):
        self.mock_app = Mock()
        self._patcher = None

    def tearDown(self):
        if self._patcher:
            self._patcher.stop()

    def _make_screen(self):
        screen = DashboardScreen()
        self._patcher = _patch_screen_app(screen, self.mock_app)
        screen.notify = Mock()
        screen.query_one = Mock()
        screen.set_timer = Mock()
        return screen

    def test_on_agent_selected(self):
        """Test agent selection from search deploys agent with task."""
        screen = self._make_screen()
        event = Mock()
        event.agent_id = "debugger"
        event.task = "fix the bug"
        screen.on_agent_selected(event)
        self.mock_app.action_deploy_agent.assert_called_once_with("debugger", task="fix the bug")

    def test_on_squad_selected(self):
        """Test squad selection deploys squad with task."""
        screen = self._make_screen()
        event = Mock()
        event.squad_id = "quality"
        event.task = "review code"
        screen.on_squad_selected(event)
        self.mock_app.action_deploy_squad.assert_called_once_with("quality", task="review code")

    def test_on_combo_selected(self):
        """Test combo selection deploys combo with task."""
        screen = self._make_screen()
        event = Mock()
        event.combo_id = "review-chain"
        event.task = "audit module"
        screen.on_combo_selected(event)
        self.mock_app.action_deploy_combo.assert_called_once_with("review-chain", task="audit module")

    def test_on_system_command_requested(self):
        """Test system command dispatched to app."""
        screen = self._make_screen()
        event = Mock()
        event.command = "health"
        screen.on_system_command_requested(event)
        self.mock_app.run_system_command.assert_called_once_with("health")

    def test_on_agent_card_restart_requested(self):
        """Test restart request calls _restart_agent."""
        screen = self._make_screen()
        event = Mock()
        event.agent.id = "coder"
        mock_card = Mock()
        screen.query_one.return_value = mock_card
        screen.on_agent_card_restart_requested(event)
        screen.notify.assert_called()

    def test_on_agent_card_logs_requested(self):
        """Test logs request pushes LogViewerScreen."""
        screen = self._make_screen()
        event = Mock()
        event.agent.id = "tester"
        screen.on_agent_card_logs_requested(event)
        self.mock_app.push_screen.assert_called_once()

    def test_on_agent_card_error_dismissed(self):
        """Test error dismissal stores undo state and notifies."""
        screen = self._make_screen()
        event = Mock()
        event.agent.id = "debugger"
        screen.on_agent_card_error_dismissed(event)
        assert screen._last_dismissed_agent == "debugger"
        screen.notify.assert_called_once()
        screen.set_timer.assert_called_once()

    def test_clear_undo_state(self):
        """Test _clear_undo_state resets dismissed agent."""
        screen = self._make_screen()
        screen._last_dismissed_agent = "debugger"
        screen._clear_undo_state()
        assert screen._last_dismissed_agent is None

    def test_action_undo_dismiss_with_agent(self):
        """Test undo dismiss restores error state."""
        screen = self._make_screen()
        screen._last_dismissed_agent = "debugger"
        mock_card = Mock()
        screen.query_one.return_value = mock_card
        screen.action_undo_dismiss()
        assert mock_card.status == "error"
        screen.notify.assert_called()
        assert screen._last_dismissed_agent is None

    def test_action_undo_dismiss_no_agent(self):
        """Test undo dismiss with no dismissed agent does nothing."""
        screen = self._make_screen()
        screen._last_dismissed_agent = None
        screen.action_undo_dismiss()
        screen.notify.assert_not_called()

    def test_action_undo_dismiss_agent_not_found(self):
        """Test undo dismiss handles missing card gracefully."""
        screen = self._make_screen()
        screen._last_dismissed_agent = "missing"
        screen.query_one.side_effect = Exception("not found")
        screen.action_undo_dismiss()
        assert screen._last_dismissed_agent is None

    def test_restart_agent(self):
        """Test _restart_agent sets card status to idle."""
        screen = self._make_screen()
        mock_card = Mock()
        screen.query_one.return_value = mock_card
        screen._restart_agent("coder")
        assert mock_card.status == "idle"
        screen.notify.assert_called()

    def test_restart_agent_card_not_found(self):
        """Test _restart_agent handles missing card."""
        screen = self._make_screen()
        screen.query_one.side_effect = Exception("not found")
        screen._restart_agent("missing")  # Should not raise

    def test_simulate_agent(self):
        """Test _simulate_agent sends notification."""
        screen = self._make_screen()
        screen._simulate_agent("coder")
        screen.notify.assert_called()
        screen.set_timer.assert_called()

    def test_open_agent_logs(self):
        """Test _open_agent_logs pushes log screen."""
        screen = self._make_screen()
        screen._open_agent_logs("coder")
        self.mock_app.push_screen.assert_called_once()

    @patch("builtins.open", side_effect=FileNotFoundError)
    def test_get_agent_error_message_file_not_found(self, _):
        """Test _get_agent_error_message returns empty on missing file."""
        screen = self._make_screen()
        assert screen._get_agent_error_message("test") == ""

    @patch("builtins.open")
    def test_get_agent_error_message_with_state(self, mock_open):
        """Test _get_agent_error_message returns error from state."""
        import json
        mock_open.return_value.__enter__ = lambda s: s
        mock_open.return_value.__exit__ = Mock(return_value=False)
        mock_open.return_value.read = Mock(return_value=json.dumps({
            "agents": {"test": {"last_error": "connection refused"}}
        }))
        # Use json.load which calls read()
        with patch("json.load", return_value={"agents": {"test": {"last_error": "connection refused"}}}):
            screen = self._make_screen()
            result = screen._get_agent_error_message("test")
            assert result == "connection refused"

    def test_on_agent_card_error_details_requested(self):
        """Test error details request pushes modal."""
        screen = self._make_screen()
        screen._get_agent_error_message = Mock(return_value="test error")
        self.mock_app.track_error = Mock()
        event = Mock()
        event.agent = Agent(id="test", tier="core", version="0.0.2", tags=(), activation="default")
        screen.on_agent_card_error_details_requested(event)
        self.mock_app.push_screen.assert_called_once()

    def test_on_bulk_restart_no_agents(self):
        """Test bulk restart with empty list warns."""
        screen = self._make_screen()
        event = Mock()
        event.agent_ids = []
        screen.on_bulk_restart_requested(event)
        call_args = screen.notify.call_args
        assert call_args[1].get("severity") == "warning"

    def test_on_bulk_restart_with_agents(self):
        """Test bulk restart restarts all failed agents."""
        screen = self._make_screen()
        mock_card = Mock()
        screen.query_one.return_value = mock_card
        event = Mock()
        event.agent_ids = ["a1", "a2", "a3"]
        screen.on_bulk_restart_requested(event)
        # Should notify for each restart plus one summary
        assert screen.notify.call_count >= 1


if __name__ == "__main__":
    unittest.main()
