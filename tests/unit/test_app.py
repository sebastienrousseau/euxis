#!/usr/bin/env python3
"""Comprehensive unit tests for EuxisApp (tui/app.py).

Tests cover all action methods using mocks to avoid needing a running
Textual application. Each action method is tested for both success and
error paths.
"""

from __future__ import annotations

import unittest
from unittest.mock import MagicMock, patch

from tui.core.config import ETXConfig
from tui.core.registry import Agent, Combo, FleetRegistry, Squad


def _make_app():
    """Create an EuxisApp instance with mocked I/O dependencies.

    Patches ETXConfig.load, FleetRegistry.load, get_project_name, and
    get_git_branch so the constructor runs without touching disk.
    Returns (app, registry, config).
    """
    config = ETXConfig()
    registry = FleetRegistry()
    registry.agents = [
        Agent(id="architect", tier="core", version="0.0.7",
              tags=("design",), activation="default"),
        Agent(id="debugger", tier="fleet", version="0.0.7",
              tags=("debug",), activation="default"),
        Agent(id="tester", tier="fleet", version="0.0.7",
              tags=("test",), activation="on-demand"),
    ]
    registry.squads = [
        Squad(id="alpha", name="Alpha Squad", purpose="Testing",
              lead="architect", members=("architect", "debugger")),
    ]
    registry.combos = [
        Combo(id="review-chain", name="Review Chain",
              description="Sequential review", chain=("architect", "reviewer")),
    ]

    with patch("tui.app.ETXConfig.load", return_value=config), \
         patch("tui.app.FleetRegistry.load", return_value=registry), \
         patch("tui.app.get_project_name", return_value="test-project"), \
         patch("tui.app.get_git_branch", return_value="main"):
        from tui.app import EuxisApp
        app = EuxisApp()

    # Stub out Textual runtime methods that would fail without a running app
    app.push_screen = MagicMock()
    app.notify = MagicMock()

    return app, registry, config


class TestEuxisAppInit(unittest.TestCase):
    """Tests for EuxisApp initialization."""

    def test_app_has_title(self):
        app, _, _ = _make_app()
        assert app.TITLE == "Euxis ETX"

    def test_app_has_sub_title(self):
        app, _, _ = _make_app()
        assert app.SUB_TITLE == "Enterprise Agent Fleet"

    def test_app_stores_config(self):
        app, _, config = _make_app()
        assert app.config is config

    def test_app_stores_registry(self):
        app, registry, _ = _make_app()
        assert app.fleet_registry is registry

    def test_app_stores_project_name(self):
        app, _, _ = _make_app()
        assert app.project_name == "test-project"

    def test_app_stores_git_branch(self):
        app, _, _ = _make_app()
        assert app.git_branch == "main"

    def test_app_has_bindings(self):
        app, _, _ = _make_app()
        assert len(app.BINDINGS) > 0

    def test_app_has_command_providers(self):
        app, _, _ = _make_app()
        from tui.commands import AgentCommandProvider, SquadCommandProvider, SystemCommandProvider
        assert AgentCommandProvider in app.COMMANDS
        assert SquadCommandProvider in app.COMMANDS
        assert SystemCommandProvider in app.COMMANDS


# ============================================================================
# action_toggle_theme
# ============================================================================


class TestActionToggleTheme(unittest.TestCase):
    """Tests for the toggle_theme action cycling through themes."""

    def test_dark_to_light(self):
        app, _, _ = _make_app()
        app.theme = "textual-dark"
        app.action_toggle_theme()
        assert app.theme == "textual-light"
        assert app.config.theme == "textual-light"

    def test_light_to_ansi(self):
        app, _, _ = _make_app()
        app.theme = "textual-light"
        app.action_toggle_theme()
        assert app.theme == "textual-ansi"
        assert app.config.theme == "textual-ansi"

    def test_ansi_to_dark(self):
        app, _, _ = _make_app()
        app.theme = "textual-ansi"
        app.action_toggle_theme()
        assert app.theme == "textual-dark"
        assert app.config.theme == "textual-dark"

    def test_unknown_theme_falls_to_dark(self):
        """Any unrecognised theme should cycle back to dark.

        We set the internal reactive storage directly to bypass Textual's
        theme validation, so the string comparison in action_toggle_theme
        hits the else branch.
        """
        app, _, _ = _make_app()
        # Bypass Textual theme validation by writing directly to reactive storage
        app._reactive_theme = "some-other-theme"
        app.action_toggle_theme()
        assert app.config.theme == "textual-dark"

    def test_toggle_saves_config(self):
        app, _, _ = _make_app()
        app.theme = "textual-dark"
        app.config.save = MagicMock()
        app.action_toggle_theme()
        app.config.save.assert_called_once()

    def test_toggle_notifies(self):
        app, _, _ = _make_app()
        app.theme = "textual-dark"
        app.action_toggle_theme()
        app.notify.assert_called_once()
        assert "textual-light" in app.notify.call_args[0][0]


# ============================================================================
# action_deploy_agent
# ============================================================================


class TestActionDeployAgent(unittest.TestCase):
    """Tests for deploying an agent via action_deploy_agent."""

    def test_known_agent_pushes_screen(self):
        app, _, _ = _make_app()
        mock_screen_cls = MagicMock()
        mock_module = MagicMock()
        mock_module.AgentScreen = mock_screen_cls
        with patch.dict("sys.modules", {"tui.screens.agent": mock_module}):
            app.action_deploy_agent("architect")
        app.push_screen.assert_called_once()

    def test_unknown_agent_notifies_error(self):
        app, _, _ = _make_app()
        app.action_deploy_agent("nonexistent")
        app.notify.assert_called_once()
        args, kwargs = app.notify.call_args
        assert "Unknown agent" in args[0]
        assert kwargs.get("severity") == "error"

    def test_unknown_agent_does_not_push_screen(self):
        app, _, _ = _make_app()
        app.action_deploy_agent("nonexistent")
        app.push_screen.assert_not_called()


# ============================================================================
# action_deploy_squad
# ============================================================================


class TestActionDeploySquad(unittest.TestCase):
    """Tests for deploying a squad via action_deploy_squad."""

    def test_known_squad_pushes_screen(self):
        app, _, _ = _make_app()
        mock_module = MagicMock()
        with patch.dict("sys.modules", {"tui.screens.fleet_monitor": mock_module}):
            app.action_deploy_squad("alpha")
        app.push_screen.assert_called_once()

    def test_unknown_squad_notifies_error(self):
        app, _, _ = _make_app()
        app.action_deploy_squad("nonexistent")
        app.notify.assert_called_once()
        args, kwargs = app.notify.call_args
        assert "Unknown squad" in args[0]
        assert kwargs.get("severity") == "error"

    def test_unknown_squad_does_not_push_screen(self):
        app, _, _ = _make_app()
        app.action_deploy_squad("nonexistent")
        app.push_screen.assert_not_called()


# ============================================================================
# action_deploy_combo
# ============================================================================


class TestActionDeployCombo(unittest.TestCase):
    """Tests for deploying a combo via action_deploy_combo."""

    def test_known_combo_pushes_screen(self):
        app, _, _ = _make_app()
        mock_module = MagicMock()
        with patch.dict("sys.modules", {"tui.screens.fleet_monitor": mock_module}):
            app.action_deploy_combo("review-chain")
        app.push_screen.assert_called_once()

    def test_unknown_combo_notifies_error(self):
        app, _, _ = _make_app()
        app.action_deploy_combo("nonexistent")
        app.notify.assert_called_once()
        args, kwargs = app.notify.call_args
        assert "Unknown combo" in args[0]
        assert kwargs.get("severity") == "error"

    def test_unknown_combo_does_not_push_screen(self):
        app, _, _ = _make_app()
        app.action_deploy_combo("nonexistent")
        app.push_screen.assert_not_called()


# ============================================================================
# Simple action methods that push screens
# ============================================================================


class TestSimpleScreenActions(unittest.TestCase):
    """Tests for actions that simply push a known screen."""

    def test_action_open_settings(self):
        app, _, _ = _make_app()
        with patch.dict("sys.modules", {"tui.screens.settings": MagicMock()}):
            app.action_open_settings()
        app.push_screen.assert_called_once()

    def test_action_fleet_monitor_screen(self):
        app, _, _ = _make_app()
        with patch.dict("sys.modules", {"tui.screens.fleet_monitor": MagicMock()}):
            app.action_fleet_monitor_screen()
        app.push_screen.assert_called_once()

    def test_action_open_logs(self):
        app, _, _ = _make_app()
        with patch.dict("sys.modules", {"tui.screens.logs": MagicMock()}):
            app.action_open_logs()
        app.push_screen.assert_called_once()

    def test_action_open_playbooks(self):
        app, _, _ = _make_app()
        with patch.dict("sys.modules", {"tui.screens.playbooks": MagicMock()}):
            app.action_open_playbooks()
        app.push_screen.assert_called_once()

    def test_action_open_cortex(self):
        app, _, _ = _make_app()
        with patch.dict("sys.modules", {"tui.screens.cortex": MagicMock()}):
            app.action_open_cortex()
        app.push_screen.assert_called_once()

    def test_action_open_metrics(self):
        app, _, _ = _make_app()
        with patch.dict("sys.modules", {"tui.screens.metrics": MagicMock()}):
            app.action_open_metrics()
        app.push_screen.assert_called_once()

    def test_action_open_squad_detail(self):
        app, _, _ = _make_app()
        with patch.dict("sys.modules", {"tui.screens.squad_detail": MagicMock()}):
            app.action_open_squad_detail()
        app.push_screen.assert_called_once()

    def test_action_help(self):
        app, _, _ = _make_app()
        with patch.dict("sys.modules", {"tui.screens.help": MagicMock()}):
            app.action_help()
        app.push_screen.assert_called_once()

    def test_action_about(self):
        app, _, _ = _make_app()
        with patch.dict("sys.modules", {"tui.screens.about": MagicMock()}):
            app.action_about()
        app.push_screen.assert_called_once()


# ============================================================================
# action_refresh
# ============================================================================


class TestActionRefresh(unittest.TestCase):
    """Tests for the refresh action."""

    def test_refresh_reloads_registry(self):
        app, original_reg, _ = _make_app()
        new_reg = FleetRegistry()
        with patch("tui.app.FleetRegistry.load", return_value=new_reg), \
             patch("tui.app.get_project_name", return_value="refreshed-proj"), \
             patch("tui.app.get_git_branch", return_value="develop"):
            app.action_refresh()
        assert app.fleet_registry is new_reg
        assert app.project_name == "refreshed-proj"
        assert app.git_branch == "develop"

    def test_refresh_notifies(self):
        app, _, _ = _make_app()
        with patch("tui.app.FleetRegistry.load", return_value=FleetRegistry()), \
             patch("tui.app.get_project_name", return_value="x"), \
             patch("tui.app.get_git_branch", return_value=None):
            app.action_refresh()
        app.notify.assert_called_once()
        assert "refreshed" in app.notify.call_args[0][0].lower()


# ============================================================================
# run_system_command dispatcher
# ============================================================================


class TestRunSystemCommand(unittest.TestCase):
    """Tests for the run_system_command dispatcher."""

    def test_cortex_status_opens_cortex(self):
        app, _, _ = _make_app()
        app.action_open_cortex = MagicMock()
        app.run_system_command("cortex_status")
        app.action_open_cortex.assert_called_once()
        app.push_screen.assert_not_called()

    def test_health_pushes_tool_runner(self):
        app, _, _ = _make_app()
        with patch.dict("sys.modules", {"tui.screens.tool_runner": MagicMock()}):
            app.run_system_command("health")
        app.push_screen.assert_called_once()

    def test_certify_pushes_tool_runner(self):
        app, _, _ = _make_app()
        with patch.dict("sys.modules", {"tui.screens.tool_runner": MagicMock()}):
            app.run_system_command("certify")
        app.push_screen.assert_called_once()

    def test_lint_pushes_tool_runner(self):
        app, _, _ = _make_app()
        with patch.dict("sys.modules", {"tui.screens.tool_runner": MagicMock()}):
            app.run_system_command("lint")
        app.push_screen.assert_called_once()

    def test_sync_docs_pushes_tool_runner(self):
        app, _, _ = _make_app()
        with patch.dict("sys.modules", {"tui.screens.tool_runner": MagicMock()}):
            app.run_system_command("sync_docs")
        app.push_screen.assert_called_once()

    def test_toggle_theme_delegates(self):
        app, _, _ = _make_app()
        app.action_toggle_theme = MagicMock()
        app.run_system_command("toggle_theme")
        app.action_toggle_theme.assert_called_once()

    def test_settings_delegates(self):
        app, _, _ = _make_app()
        app.action_open_settings = MagicMock()
        app.run_system_command("settings")
        app.action_open_settings.assert_called_once()

    def test_fleet_monitor_delegates(self):
        app, _, _ = _make_app()
        app.action_fleet_monitor_screen = MagicMock()
        app.run_system_command("fleet_monitor")
        app.action_fleet_monitor_screen.assert_called_once()

    def test_view_logs_delegates(self):
        app, _, _ = _make_app()
        app.action_open_logs = MagicMock()
        app.run_system_command("view_logs")
        app.action_open_logs.assert_called_once()

    def test_help_delegates(self):
        app, _, _ = _make_app()
        app.action_help = MagicMock()
        app.run_system_command("help")
        app.action_help.assert_called_once()

    def test_about_delegates(self):
        app, _, _ = _make_app()
        app.action_about = MagicMock()
        app.run_system_command("about")
        app.action_about.assert_called_once()

    def test_playbooks_delegates(self):
        app, _, _ = _make_app()
        app.action_open_playbooks = MagicMock()
        app.run_system_command("playbooks")
        app.action_open_playbooks.assert_called_once()

    def test_metrics_delegates(self):
        app, _, _ = _make_app()
        app.action_open_metrics = MagicMock()
        app.run_system_command("metrics")
        app.action_open_metrics.assert_called_once()

    def test_cortex_delegates(self):
        app, _, _ = _make_app()
        app.action_open_cortex = MagicMock()
        app.run_system_command("cortex")
        app.action_open_cortex.assert_called_once()

    def test_squad_detail_delegates(self):
        app, _, _ = _make_app()
        app.action_open_squad_detail = MagicMock()
        app.run_system_command("squad_detail")
        app.action_open_squad_detail.assert_called_once()

    def test_refresh_delegates(self):
        app, _, _ = _make_app()
        app.action_refresh = MagicMock()
        app.run_system_command("refresh")
        app.action_refresh.assert_called_once()

    def test_unknown_command_does_nothing(self):
        """Unrecognized commands should not push screens or crash."""
        app, _, _ = _make_app()
        app.run_system_command("totally_unknown")
        app.push_screen.assert_not_called()


# ============================================================================
# on_mount
# ============================================================================


class TestOnMount(unittest.TestCase):
    """Tests for the on_mount lifecycle method."""

    def test_on_mount_applies_saved_theme(self):
        app, _, _ = _make_app()
        app.config.theme = "textual-light"
        app.on_mount()
        assert app.theme == "textual-light"
        app.push_screen.assert_called_once()

    def test_on_mount_falls_back_on_invalid_theme(self):
        """Invalid saved theme should fall back to textual-dark."""
        app, _, _ = _make_app()
        app.config.theme = "nonexistent-theme-xyz"
        # on_mount catches InvalidThemeError and falls back to textual-dark
        app.on_mount()
        assert app.theme == "textual-dark"
        # Should have pushed DashboardScreen despite theme error
        app.push_screen.assert_called_once()

    def test_on_mount_pushes_dashboard(self):
        app, _, _ = _make_app()
        app.on_mount()
        app.push_screen.assert_called_once()
        # Verify a DashboardScreen was passed
        from tui.screens.dashboard import DashboardScreen
        screen_arg = app.push_screen.call_args[0][0]
        assert isinstance(screen_arg, DashboardScreen)


if __name__ == "__main__":
    unittest.main()
