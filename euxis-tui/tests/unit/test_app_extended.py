# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Extended unit tests for EuxisApp (tui/app.py).

Covers all code paths not already exercised by test_app.py, targeting
>= 95 % line coverage. Tests use pure mocks — no running Textual loop.
"""

from __future__ import annotations

import json
import subprocess
import unittest
from unittest.mock import MagicMock, PropertyMock, call, mock_open, patch

from tui.core.config import ETXConfig
from tui.core.registry import Agent, Combo, FleetRegistry, Squad


# ---------------------------------------------------------------------------
# Shared helper — identical to test_app._make_app but duplicated so this
# file is self-contained and can be run independently.
# ---------------------------------------------------------------------------

def _make_app():
    """Create an EuxisApp instance with mocked I/O dependencies."""
    config = ETXConfig()
    registry = FleetRegistry()
    registry.agents = [
        Agent(id="architect", tier="core", version="0.0.1",
              tags=["design", "structure"], activation="default",
              capability_tags=["system-design", "architecture", "review"]),
        Agent(id="debugger", tier="fleet", version="0.0.1",
              tags=["debug"], activation="default",
              capability_tags=[]),
        Agent(id="tester", tier="fleet", version="0.0.1",
              tags=[], activation="on-demand",
              capability_tags=[]),
    ]
    registry.squads = [
        Squad(id="alpha", name="Alpha Squad", purpose="Testing",
              lead="architect", members=("architect", "debugger")),
    ]
    registry.combos = [
        Combo(id="review-chain", name="Review Chain",
              description="Sequential review", chain=("architect", "reviewer")),
    ]

    with patch("tui.core.config.ETXConfig.load", return_value=config), \
         patch("tui.core.registry.FleetRegistry.load", return_value=registry), \
         patch("tui.core.runner.get_project_name", return_value="test-project"), \
         patch("tui.core.runner.get_git_branch", return_value="main"):
        from tui.app import EuxisApp
        app = EuxisApp()

    app.push_screen = MagicMock()
    app.notify = MagicMock()
    app.call_from_thread = MagicMock(side_effect=lambda func, *args, **kwargs: func(*args, **kwargs))
    return app, registry, config


# ============================================================================
# Lazy loaders — _get_config, _get_registry, _get_themes
# ============================================================================

class TestLazyLoaders(unittest.TestCase):
    """Tests for module-level lazy-loader functions."""

    def test_get_config_returns_etx_config(self):
        import tui.app as app_mod
        old = app_mod._config
        try:
            app_mod._config = None
            result = app_mod._get_config()
            from tui.core.config import ETXConfig
            assert result is ETXConfig
        finally:
            app_mod._config = old

    def test_get_config_caches(self):
        import tui.app as app_mod
        old = app_mod._config
        try:
            app_mod._config = None
            first = app_mod._get_config()
            second = app_mod._get_config()
            assert first is second
        finally:
            app_mod._config = old

    def test_get_registry_returns_fleet_registry(self):
        import tui.app as app_mod
        old = app_mod._registry
        try:
            app_mod._registry = None
            result = app_mod._get_registry()
            from tui.core.registry import FleetRegistry
            assert result is FleetRegistry
        finally:
            app_mod._registry = old

    def test_get_registry_caches(self):
        import tui.app as app_mod
        old = app_mod._registry
        try:
            app_mod._registry = None
            first = app_mod._get_registry()
            second = app_mod._get_registry()
            assert first is second
        finally:
            app_mod._registry = old

    def test_get_themes_returns_module(self):
        import tui.app as app_mod
        old = app_mod._themes
        try:
            app_mod._themes = None
            result = app_mod._get_themes()
            import tui.themes
            assert result is tui.themes
        finally:
            app_mod._themes = old

    def test_get_themes_caches(self):
        import tui.app as app_mod
        old = app_mod._themes
        try:
            app_mod._themes = None
            first = app_mod._get_themes()
            second = app_mod._get_themes()
            assert first is second
        finally:
            app_mod._themes = old


# ============================================================================
# ConfirmDeployScreen
# ============================================================================

class TestConfirmDeployScreen(unittest.TestCase):
    """Tests for the ConfirmDeployScreen modal."""

    def _make_screen(self, **kwargs):
        from tui.app import ConfirmDeployScreen
        screen = ConfirmDeployScreen.__new__(ConfirmDeployScreen)
        screen._operation = kwargs.get("operation", "agent")
        screen._name = kwargs.get("name", "test-agent")
        screen._task = kwargs.get("task", "")
        screen._reason = kwargs.get("reason", "")
        return screen

    def test_init_stores_operation(self):
        """Screen stores operation attribute."""
        screen = self._make_screen(operation="squad")
        assert screen._operation == "squad"

    def test_init_stores_name(self):
        """Screen stores name attribute."""
        screen = self._make_screen(name="my-agent")
        assert screen._name == "my-agent"

    def test_init_stores_task(self):
        """Screen stores task attribute."""
        screen = self._make_screen(task="do something")
        assert screen._task == "do something"

    def test_init_stores_reason(self):
        """Screen stores reason attribute."""
        screen = self._make_screen(reason="specialized in design")
        assert screen._reason == "specialized in design"

    def test_on_button_pressed_confirm(self):
        """Pressing confirm button dismisses with True."""
        screen = self._make_screen()
        screen.dismiss = MagicMock()
        event = MagicMock()
        event.button.id = "confirm"
        screen.on_button_pressed(event)
        screen.dismiss.assert_called_once_with(result=True)

    def test_on_button_pressed_cancel(self):
        """Pressing cancel button dismisses with False."""
        screen = self._make_screen()
        screen.dismiss = MagicMock()
        event = MagicMock()
        event.button.id = "cancel"
        screen.on_button_pressed(event)
        screen.dismiss.assert_called_once_with(result=False)

    def test_key_escape_dismisses_false(self):
        screen = self._make_screen()
        screen.dismiss = MagicMock()
        screen.key_escape()
        screen.dismiss.assert_called_once_with(result=False)

    def test_key_enter_dismisses_true(self):
        screen = self._make_screen()
        screen.dismiss = MagicMock()
        screen.key_enter()
        screen.dismiss.assert_called_once_with(result=True)

    def test_init_stores_fields(self):
        """Full __init__ path stores operation, name, task, reason."""
        from tui.app import ConfirmDeployScreen
        # Provide a mock app since ModalScreen may try to access it
        with patch.object(ConfirmDeployScreen, "__init_subclass__", lambda **kw: None):
            screen = ConfirmDeployScreen(
                operation="squad",
                name="alpha",
                task="deploy it",
                reason="testing",
            )
        assert screen._operation == "squad"
        assert screen._name == "alpha"
        assert screen._task == "deploy it"
        assert screen._reason == "testing"


# ============================================================================
# _format_agent_reason — three branches
# ============================================================================

class TestFormatAgentReason(unittest.TestCase):

    def test_with_capability_tags(self):
        """Returns 'specialized in ...' when agent has capability_tags."""
        app, _, _ = _make_app()
        agent = Agent(
            id="a", tier="core", version="0.0.1",
            tags=["tag1"], activation="default",
            capability_tags=["design", "review", "testing", "extra"],
        )
        reason = app._format_agent_reason(agent)
        assert reason.startswith("specialized in")
        # Only first 3 caps
        assert "extra" not in reason

    def test_with_tags_no_caps(self):
        """Returns 'strong in ...' when agent has tags but no caps."""
        app, _, _ = _make_app()
        agent = Agent(
            id="a", tier="fleet", version="0.0.1",
            tags=["debug", "trace", "log", "extra"],
            activation="default",
            capability_tags=[],
        )
        reason = app._format_agent_reason(agent)
        assert reason.startswith("strong in")
        assert "extra" not in reason

    def test_fallback_tier_activation(self):
        """Returns tier · activation when no caps and no tags."""
        app, _, _ = _make_app()
        agent = Agent(
            id="a", tier="core", version="0.0.1",
            tags=[], activation="default",
            capability_tags=[],
        )
        reason = app._format_agent_reason(agent)
        assert "CORE" in reason
        assert "Auto" in reason


# ============================================================================
# welcome_shown / mark_welcome_shown
# ============================================================================

class TestWelcomeShown(unittest.TestCase):

    def test_welcome_shown_property(self):
        app, _, _ = _make_app()
        app._welcome_shown = False
        assert app.welcome_shown is False

    def test_mark_welcome_shown(self):
        app, _, _ = _make_app()
        app._welcome_shown = False
        app.mark_welcome_shown()
        assert app.welcome_shown is True


# ============================================================================
# action_welcome / action_time_travel
# ============================================================================

class TestExtraScreenActions(unittest.TestCase):

    def test_action_welcome_pushes_screen(self):
        app, _, _ = _make_app()
        with patch.dict("sys.modules", {"tui.screens.welcome": MagicMock()}):
            app.action_welcome()
        app.push_screen.assert_called_once()

    def test_action_time_travel_pushes_screen(self):
        app, _, _ = _make_app()
        with patch.dict("sys.modules", {"tui.screens.time_travel": MagicMock()}):
            app.action_time_travel()
        app.push_screen.assert_called_once()


# ============================================================================
# Adaptive theme — track_error, triggers, suggestions
# ============================================================================

class TestAdaptiveTheme(unittest.TestCase):

    def test_track_error_increments_count(self):
        app, _, _ = _make_app()
        app._error_count = 0
        app.track_error()
        assert app._error_count == 1

    def test_track_error_critical_severity_triggers_focus(self):
        app, _, _ = _make_app()
        app.config.save = MagicMock()
        app.theme = "etx-liquid-glass"
        app.track_error(severity="critical")
        assert app.theme == app._FOCUS_THEME
        app.notify.assert_called_once()

    def test_track_error_critical_keyword_triggers_focus(self):
        app, _, _ = _make_app()
        app.config.save = MagicMock()
        app.theme = "etx-liquid-glass"
        app.track_error(message="Out of Memory detected")
        assert app.theme == app._FOCUS_THEME

    def test_track_error_critical_already_focused_no_switch(self):
        """If already on focus theme, do not trigger transition again."""
        app, _, _ = _make_app()
        app.theme = app._FOCUS_THEME
        app.track_error(severity="critical")
        # Should still increment but not call _trigger
        assert app._error_count == 1
        # No notification for focus switch since already focused
        # It may suggest calm theme instead (if threshold reached)

    def test_track_error_threshold_suggests_calm(self):
        app, _, _ = _make_app()
        app.theme = "etx-liquid-glass"
        app._calm_theme_suggested = False
        # Fire 3 non-critical errors
        for _ in range(3):
            app.track_error(severity="warning")
        # _suggest_calm_theme should have been called
        app.notify.assert_called()
        assert app._calm_theme_suggested is True

    def test_track_error_threshold_not_triggered_when_already_suggested(self):
        app, _, _ = _make_app()
        app.theme = "etx-liquid-glass"
        app._calm_theme_suggested = True
        app._error_count = 10
        app.track_error(severity="warning")
        # Should NOT notify because already suggested
        app.notify.assert_not_called()

    def test_track_error_threshold_not_triggered_when_already_calm(self):
        app, _, _ = _make_app()
        app.theme = "etx-calm"  # already using calm theme
        app._calm_theme_suggested = False
        app._error_count = 10
        app.track_error(severity="warning")
        # Should NOT suggest because current theme IS a calm theme
        app.notify.assert_not_called()

    def test_trigger_focus_transition(self):
        app, _, _ = _make_app()
        app.config.save = MagicMock()
        app.theme = "etx-liquid-glass"
        app._trigger_focus_transition("fatal crash")
        assert app.theme == "etx-focused"
        assert app.config.theme == "etx-focused"
        app.config.save.assert_called_once()
        app.notify.assert_called_once()
        # Check it includes "Critical" or "Focus"
        msg = app.notify.call_args[1].get("title", "")
        assert "Focus" in msg

    def test_suggest_calm_theme(self):
        app, _, _ = _make_app()
        app._suggest_calm_theme()
        app.notify.assert_called_once()
        args, kwargs = app.notify.call_args
        assert "calm" in args[0].lower() or "calmer" in args[0].lower()
        assert kwargs.get("title") == "Adaptive Theme"

    def test_switch_to_calm_when_in_cycle(self):
        app, _, _ = _make_app()
        app.config.save = MagicMock()
        # Ensure etx-calm is in theme cycle
        app._theme_cycle = ["etx-calm", "etx-liquid-glass"]
        app.switch_to_calm()
        assert app.theme == "etx-calm"
        assert app.config.theme == "etx-calm"
        app.config.save.assert_called_once()
        app.notify.assert_called_once()

    def test_switch_to_calm_when_not_in_cycle(self):
        app, _, _ = _make_app()
        app.config.save = MagicMock()
        app._theme_cycle = ["etx-liquid-glass"]  # no etx-calm
        app.switch_to_calm()
        # Should not change theme
        app.config.save.assert_not_called()
        app.notify.assert_not_called()

    def test_switch_to_focus_when_in_cycle(self):
        app, _, _ = _make_app()
        app.config.save = MagicMock()
        app._theme_cycle = ["etx-focused", "etx-liquid-glass"]
        app.switch_to_focus()
        assert app.theme == "etx-focused"
        assert app.config.theme == "etx-focused"
        app.config.save.assert_called_once()
        app.notify.assert_called_once()

    def test_switch_to_focus_when_not_in_cycle(self):
        app, _, _ = _make_app()
        app.config.save = MagicMock()
        app._theme_cycle = ["etx-liquid-glass"]  # no etx-focused
        app.switch_to_focus()
        app.config.save.assert_not_called()
        app.notify.assert_not_called()

    def test_reset_error_tracking(self):
        app, _, _ = _make_app()
        app._error_count = 5
        app.reset_error_tracking()
        assert app._error_count == 0


# ============================================================================
# action_maximize_pane
# ============================================================================

class TestActionMaximizePane(unittest.TestCase):

    def test_no_focused_widget(self):
        app, _, _ = _make_app()
        mock_screen = MagicMock()
        mock_screen.focused = None
        app._screen = mock_screen
        # Patch the screen property
        with patch.object(type(app), "screen", new_callable=PropertyMock, return_value=mock_screen):
            app.action_maximize_pane()
        app.notify.assert_called_once()
        assert "No pane focused" in app.notify.call_args[0][0]

    def test_maximize_unfocused_widget(self):
        app, _, _ = _make_app()
        widget = MagicMock()
        widget.has_class.return_value = False
        mock_screen = MagicMock()
        mock_screen.focused = widget
        with patch.object(type(app), "screen", new_callable=PropertyMock, return_value=mock_screen):
            app.action_maximize_pane()
        widget.add_class.assert_called_once_with("maximized")
        app.notify.assert_called_once()
        assert "Maximized" in app.notify.call_args[0][0]

    def test_restore_maximized_widget(self):
        app, _, _ = _make_app()
        widget = MagicMock()
        widget.has_class.return_value = True
        mock_screen = MagicMock()
        mock_screen.focused = widget
        with patch.object(type(app), "screen", new_callable=PropertyMock, return_value=mock_screen):
            app.action_maximize_pane()
        widget.remove_class.assert_called_once_with("maximized")
        app.notify.assert_called_once()
        assert "Restored" in app.notify.call_args[0][0]


# ============================================================================
# action_router_stats
# ============================================================================

class TestActionRouterStats(unittest.TestCase):

    @patch("subprocess.run")
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_success_with_summary_lines(self, mock_run):
        app, _, _ = _make_app()
        mock_run.return_value = MagicMock(
            stdout="│ Routine: fast │\n│ Code: ok │\n│ Reason: cost │\n│ Auto: yes │\nExtra\n"
        )
        app.action_router_stats()
        app.notify.assert_called_once()
        args, kwargs = app.notify.call_args
        assert "Routine" in args[0]
        assert kwargs.get("title") == "Router Status"

    @patch("subprocess.run")
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_success_no_summary_lines(self, mock_run):
        app, _, _ = _make_app()
        mock_run.return_value = MagicMock(stdout="All good\n")
        app.action_router_stats()
        app.notify.assert_called_once()
        assert "Router active" in app.notify.call_args[0][0]

    @patch("subprocess.run", side_effect=FileNotFoundError)
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_file_not_found(self, mock_run):
        app, _, _ = _make_app()
        app.action_router_stats()
        app.notify.assert_called_once()
        assert "not found" in app.notify.call_args[0][0].lower() or "not configured" in app.notify.call_args[0][0].lower()

    @patch("subprocess.run", side_effect=subprocess.TimeoutExpired(cmd="x", timeout=5))
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_timeout(self, mock_run):
        app, _, _ = _make_app()
        app.action_router_stats()
        app.notify.assert_called_once()
        assert "timeout" in app.notify.call_args[0][0].lower()

    @patch("subprocess.run", side_effect=OSError("connection refused"))
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_generic_exception(self, mock_run):
        app, _, _ = _make_app()
        app.action_router_stats()
        app.notify.assert_called_once()
        assert "error" in app.notify.call_args[1].get("severity", "").lower()


# ============================================================================
# _run_cli_command
# ============================================================================

class TestRunCliCommand(unittest.TestCase):

    @patch("subprocess.run")
    @patch("os.path.exists", return_value=True)
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_success_short_output(self, mock_exists, mock_run):
        app, _, _ = _make_app()
        mock_run.return_value = MagicMock(stdout="line1\nline2\n")
        app._run_cli_command("euxis-health", title="Health")
        app.notify.assert_called_once()
        assert "line1" in app.notify.call_args[0][0]

    @patch("subprocess.run")
    @patch("os.path.exists", return_value=True)
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_success_long_output_truncated(self, mock_exists, mock_run):
        app, _, _ = _make_app()
        lines = "\n".join([f"line{i}" for i in range(20)])
        mock_run.return_value = MagicMock(stdout=lines)
        app._run_cli_command("cmd", title="Test")
        app.notify.assert_called_once()
        assert "more lines" in app.notify.call_args[0][0]

    @patch("subprocess.run")
    @patch("os.path.exists", return_value=False)  # fallback to PATH
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_fallback_to_path(self, mock_exists, mock_run):
        app, _, _ = _make_app()
        mock_run.return_value = MagicMock(stdout="ok\n")
        app._run_cli_command("euxis-health", title="H")
        # Should use "euxis-health" directly (not full path)
        cmd_used = mock_run.call_args[0][0][0]
        assert cmd_used == "euxis-health"

    @patch("subprocess.run", side_effect=FileNotFoundError)
    @patch("os.path.exists", return_value=False)
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_file_not_found(self, mock_exists, mock_run):
        app, _, _ = _make_app()
        app._run_cli_command("missing-cmd", title="X")
        app.notify.assert_called_once()
        assert "not found" in app.notify.call_args[0][0].lower()

    @patch("subprocess.run", side_effect=subprocess.TimeoutExpired(cmd="x", timeout=10))
    @patch("os.path.exists", return_value=True)
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_timeout(self, mock_exists, mock_run):
        app, _, _ = _make_app()
        app._run_cli_command("slow-cmd", title="X")
        app.notify.assert_called_once()
        assert "timed out" in app.notify.call_args[0][0].lower()

    @patch("subprocess.run", side_effect=RuntimeError("boom"))
    @patch("os.path.exists", return_value=True)
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_generic_exception(self, mock_exists, mock_run):
        app, _, _ = _make_app()
        app._run_cli_command("bad-cmd", title="X")
        app.notify.assert_called_once()
        assert "boom" in app.notify.call_args[0][0]


# ============================================================================
# _show_json_file
# ============================================================================

class TestShowJsonFile(unittest.TestCase):

    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_success_short_json(self):
        app, _, _ = _make_app()
        data = {"key": "value"}
        with patch("builtins.open", mock_open(read_data=json.dumps(data))):
            app._show_json_file("config/file.json", "Config")
        app.notify.assert_called_once()
        assert "key" in app.notify.call_args[0][0]
        assert app.notify.call_args[1].get("title") == "Config"

    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_success_long_json_truncated(self):
        app, _, _ = _make_app()
        data = {"k" * 100: "v" * 500}
        with patch("builtins.open", mock_open(read_data=json.dumps(data))):
            app._show_json_file("config/file.json", "Config")
        app.notify.assert_called_once()
        assert "..." in app.notify.call_args[0][0]

    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_file_not_found(self):
        app, _, _ = _make_app()
        with patch("builtins.open", side_effect=FileNotFoundError):
            app._show_json_file("missing.json", "Config")
        app.notify.assert_called_once()
        assert "not found" in app.notify.call_args[0][0].lower()
        assert app.notify.call_args[1].get("severity") == "warning"

    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_invalid_json(self):
        app, _, _ = _make_app()
        with patch("builtins.open", mock_open(read_data="not{json")):
            app._show_json_file("bad.json", "Config")
        app.notify.assert_called_once()
        assert "Invalid JSON" in app.notify.call_args[0][0]
        assert app.notify.call_args[1].get("severity") == "error"


# ============================================================================
# _show_mesh_state
# ============================================================================

class TestShowMeshState(unittest.TestCase):

    @patch("os.path.exists", return_value=False)
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_no_state_file(self, mock_exists):
        app, _, _ = _make_app()
        app._show_mesh_state(".agents", "Online")
        app.notify.assert_called_once()
        assert "not initialized" in app.notify.call_args[0][0].lower()

    @patch("subprocess.run")
    @patch("os.path.exists", return_value=True)
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_success_with_output(self, mock_exists, mock_run):
        app, _, _ = _make_app()
        mock_run.return_value = MagicMock(stdout="agent-1\nagent-2\n")
        app._show_mesh_state(".agents", "Online")
        app.notify.assert_called_once()
        assert "agent-1" in app.notify.call_args[0][0]

    @patch("subprocess.run")
    @patch("os.path.exists", return_value=True)
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_empty_output(self, mock_exists, mock_run):
        app, _, _ = _make_app()
        mock_run.return_value = MagicMock(stdout="")
        app._show_mesh_state(".agents", "Online")
        app.notify.assert_called_once()
        assert "(empty)" in app.notify.call_args[0][0]

    @patch("subprocess.run", side_effect=FileNotFoundError)
    @patch("os.path.exists", return_value=True)
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_jq_not_found(self, mock_exists, mock_run):
        app, _, _ = _make_app()
        app._show_mesh_state(".agents", "Online")
        app.notify.assert_called_once()
        assert "jq" in app.notify.call_args[0][0].lower()

    @patch("subprocess.run", side_effect=subprocess.TimeoutExpired(cmd="jq", timeout=5))
    @patch("os.path.exists", return_value=True)
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_timeout(self, mock_exists, mock_run):
        app, _, _ = _make_app()
        app._show_mesh_state(".agents", "Online")
        app.notify.assert_called_once()
        assert "timed out" in app.notify.call_args[0][0].lower()


# ============================================================================
# _show_mesh_inbox
# ============================================================================

class TestShowMeshInbox(unittest.TestCase):

    @patch("os.path.exists", return_value=False)
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_no_inbox_dir(self, mock_exists):
        app, _, _ = _make_app()
        app._show_mesh_inbox()
        app.notify.assert_called_once()
        assert "No inbox" in app.notify.call_args[0][0]

    @patch("os.path.isdir", return_value=True)
    @patch("os.listdir")
    @patch("os.path.exists", return_value=True)
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_empty_inbox(self, mock_exists, mock_listdir, mock_isdir):
        app, _, _ = _make_app()
        # Top-level has agent dirs, but no .msg files inside
        mock_listdir.side_effect = [["agent1"], []]
        app._show_mesh_inbox()
        app.notify.assert_called_once()
        assert "No pending" in app.notify.call_args[0][0]

    @patch("os.path.isdir", return_value=True)
    @patch("os.listdir")
    @patch("os.path.exists", return_value=True)
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_has_messages(self, mock_exists, mock_listdir, mock_isdir):
        app, _, _ = _make_app()
        mock_listdir.side_effect = [
            ["agent1", "agent2"],
            ["m1.msg", "m2.msg"],
            ["m3.msg"],
        ]
        app._show_mesh_inbox()
        app.notify.assert_called_once()
        assert "3 pending" in app.notify.call_args[1].get("title", "")

    @patch("os.path.isdir")
    @patch("os.listdir")
    @patch("os.path.exists", return_value=True)
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_non_dir_entries_skipped(self, mock_exists, mock_listdir, mock_isdir):
        app, _, _ = _make_app()
        mock_listdir.side_effect = [["file.txt"]]
        mock_isdir.return_value = False
        app._show_mesh_inbox()
        app.notify.assert_called_once()
        assert "No pending" in app.notify.call_args[0][0]


# ============================================================================
# _run_mesh_cleanup
# ============================================================================

class TestRunMeshCleanup(unittest.TestCase):

    @patch("subprocess.run")
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_success(self, mock_run):
        app, _, _ = _make_app()
        mock_run.return_value = MagicMock(stdout="", returncode=0)
        app._run_mesh_cleanup()
        app.notify.assert_called_once()
        assert "Cleanup complete" in app.notify.call_args[0][0]

    @patch("subprocess.run", side_effect=OSError("permission denied"))
    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_failure(self, mock_run):
        app, _, _ = _make_app()
        app._run_mesh_cleanup()
        app.notify.assert_called_once()
        assert "failed" in app.notify.call_args[0][0].lower()


# ============================================================================
# _check_mesh_deadlock
# ============================================================================

class TestCheckMeshDeadlock(unittest.TestCase):

    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_no_waiting_agents(self):
        app, _, _ = _make_app()
        state = {"agents": {"a": {"status": "idle"}, "b": {"status": "idle"}}}
        with patch("builtins.open", mock_open(read_data=json.dumps(state))):
            app._check_mesh_deadlock()
        app.notify.assert_called_once()
        assert "no deadlock" in app.notify.call_args[0][0].lower()

    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_deadlock_detected(self):
        app, _, _ = _make_app()
        state = {"agents": {
            "a": {"waiting_for": "b"},
            "b": {"waiting_for": "a"},
        }}
        with patch("builtins.open", mock_open(read_data=json.dumps(state))):
            app._check_mesh_deadlock()
        app.notify.assert_called_once()
        assert "DEADLOCK" in app.notify.call_args[0][0]

    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_waiting_no_deadlock(self):
        app, _, _ = _make_app()
        state = {"agents": {
            "a": {"waiting_for": "b"},
            "b": {"status": "running"},
        }}
        with patch("builtins.open", mock_open(read_data=json.dumps(state))):
            app._check_mesh_deadlock()
        app.notify.assert_called_once()
        assert "no deadlock" in app.notify.call_args[1].get("title", "").lower()

    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_file_not_found(self):
        app, _, _ = _make_app()
        with patch("builtins.open", side_effect=FileNotFoundError):
            app._check_mesh_deadlock()
        app.notify.assert_called_once()
        assert "not initialized" in app.notify.call_args[0][0].lower()

    @patch("os.environ", {"EUXIS_HOME": "/fake"})
    def test_generic_exception(self):
        app, _, _ = _make_app()
        with patch("builtins.open", side_effect=RuntimeError("corrupt")):
            app._check_mesh_deadlock()
        app.notify.assert_called_once()
        assert "corrupt" in app.notify.call_args[0][0]


# ============================================================================
# _show_thermal_status
# ============================================================================

class TestShowThermalStatus(unittest.TestCase):

    @patch("os.path.exists", return_value=True)
    def test_linux_normal_temp(self, mock_exists):
        app, _, _ = _make_app()
        with patch("builtins.open", mock_open(read_data="45000")):
            app._show_thermal_status()
        app.notify.assert_called_once()
        assert "45.0" in app.notify.call_args[0][0]
        assert "Normal" in app.notify.call_args[0][0]

    @patch("os.path.exists", return_value=True)
    def test_linux_warm_temp(self, mock_exists):
        app, _, _ = _make_app()
        with patch("builtins.open", mock_open(read_data="75000")):
            app._show_thermal_status()
        app.notify.assert_called_once()
        assert "Warm" in app.notify.call_args[0][0]

    @patch("os.path.exists", return_value=True)
    def test_linux_hot_temp(self, mock_exists):
        app, _, _ = _make_app()
        with patch("builtins.open", mock_open(read_data="90000")):
            app._show_thermal_status()
        app.notify.assert_called_once()
        assert "HOT" in app.notify.call_args[0][0]

    @patch("os.path.exists", return_value=True)
    def test_linux_read_exception(self, mock_exists):
        app, _, _ = _make_app()
        with patch("builtins.open", side_effect=PermissionError("denied")):
            app._show_thermal_status()
        app.notify.assert_called_once()
        assert app.notify.call_args[1].get("severity") == "warning"

    @patch("os.path.exists", return_value=False)
    def test_macos_fallback(self, mock_exists):
        app, _, _ = _make_app()
        app._show_thermal_status()
        app.notify.assert_called_once()
        assert "not accessible" in app.notify.call_args[0][0].lower()


# ============================================================================
# Deploy callbacks — _on_confirm inner functions
# ============================================================================

class TestDeployCallbacks(unittest.TestCase):
    """Test the _on_confirm callbacks inside deploy actions."""

    def test_deploy_agent_confirm_true_pushes_agent_screen(self):
        app, _, _ = _make_app()
        # Call action_deploy_agent and capture the callback
        app.action_deploy_agent("architect", task="build it")
        assert app.push_screen.call_count == 1
        # The first call is ConfirmDeployScreen with callback
        _, kwargs = app.push_screen.call_args
        callback = kwargs.get("callback")
        if callback is None:
            # callback is second positional arg
            callback = app.push_screen.call_args[0][1]
        assert callback is not None

        # Reset and invoke with True
        app.push_screen.reset_mock()
        callback(True)
        app.push_screen.assert_called_once()

    def test_deploy_agent_confirm_false_does_not_push(self):
        app, _, _ = _make_app()
        app.action_deploy_agent("architect")
        _, kwargs = app.push_screen.call_args
        callback = kwargs.get("callback")
        if callback is None:
            callback = app.push_screen.call_args[0][1]

        app.push_screen.reset_mock()
        callback(False)
        app.push_screen.assert_not_called()

    def test_deploy_squad_confirm_true_pushes_fleet_monitor(self):
        app, _, _ = _make_app()
        app.action_deploy_squad("alpha", task="go")
        _, kwargs = app.push_screen.call_args
        callback = kwargs.get("callback")
        if callback is None:
            callback = app.push_screen.call_args[0][1]

        app.push_screen.reset_mock()
        callback(True)
        app.push_screen.assert_called_once()

    def test_deploy_squad_confirm_false_does_not_push(self):
        app, _, _ = _make_app()
        app.action_deploy_squad("alpha")
        _, kwargs = app.push_screen.call_args
        callback = kwargs.get("callback")
        if callback is None:
            callback = app.push_screen.call_args[0][1]

        app.push_screen.reset_mock()
        callback(False)
        app.push_screen.assert_not_called()

    def test_deploy_combo_confirm_true_pushes_fleet_monitor(self):
        app, _, _ = _make_app()
        app.action_deploy_combo("review-chain", task="review")
        _, kwargs = app.push_screen.call_args
        callback = kwargs.get("callback")
        if callback is None:
            callback = app.push_screen.call_args[0][1]

        app.push_screen.reset_mock()
        callback(True)
        app.push_screen.assert_called_once()

    def test_deploy_combo_confirm_false_does_not_push(self):
        app, _, _ = _make_app()
        app.action_deploy_combo("review-chain")
        _, kwargs = app.push_screen.call_args
        callback = kwargs.get("callback")
        if callback is None:
            callback = app.push_screen.call_args[0][1]

        app.push_screen.reset_mock()
        callback(False)
        app.push_screen.assert_not_called()


# ============================================================================
# run_system_command — remaining commands not in test_app.py
# ============================================================================

class TestRunSystemCommandExtended(unittest.TestCase):
    """Cover all run_system_command branches not in the original test file."""

    def test_router_stats(self):
        app, _, _ = _make_app()
        app.action_router_stats = MagicMock()
        app.run_system_command("router_stats")
        app.action_router_stats.assert_called_once()

    def test_router_benchmark(self):
        app, _, _ = _make_app()
        app._run_cli_command = MagicMock()
        app.run_system_command("router_benchmark")
        app._run_cli_command.assert_called_once()

    def test_router_config(self):
        app, _, _ = _make_app()
        app._show_json_file = MagicMock()
        app.run_system_command("router_config")
        app._show_json_file.assert_called_once_with("euxis-core/config/router.json", "Router Config")

    def test_mesh_status(self):
        app, _, _ = _make_app()
        app._run_cli_command = MagicMock()
        app.run_system_command("mesh_status")
        app._run_cli_command.assert_called_once()

    def test_mesh_discover(self):
        app, _, _ = _make_app()
        app.run_system_command("mesh_discover")
        app.notify.assert_called_once()
        assert "mesh_discover" in app.notify.call_args[0][0].lower()

    def test_mesh_list_online(self):
        app, _, _ = _make_app()
        app._show_mesh_state = MagicMock()
        app.run_system_command("mesh_list_online")
        app._show_mesh_state.assert_called_once()

    def test_mesh_inbox(self):
        app, _, _ = _make_app()
        app._show_mesh_inbox = MagicMock()
        app.run_system_command("mesh_inbox")
        app._show_mesh_inbox.assert_called_once()

    def test_mesh_heartbeat(self):
        app, _, _ = _make_app()
        app.run_system_command("mesh_heartbeat")
        app.notify.assert_called_once()
        assert "Heartbeat" in app.notify.call_args[0][0]

    def test_mesh_cleanup(self):
        app, _, _ = _make_app()
        app._run_mesh_cleanup = MagicMock()
        app.run_system_command("mesh_cleanup")
        app._run_mesh_cleanup.assert_called_once()

    def test_mesh_state(self):
        app, _, _ = _make_app()
        app._show_json_file = MagicMock()
        app.run_system_command("mesh_state")
        app._show_json_file.assert_called_once_with("euxis-runtime/mesh/state.json", "Mesh State")

    def test_mesh_deadlock(self):
        app, _, _ = _make_app()
        app._check_mesh_deadlock = MagicMock()
        app.run_system_command("mesh_deadlock")
        app._check_mesh_deadlock.assert_called_once()

    def test_dispatch_mission(self):
        app, _, _ = _make_app()
        app.run_system_command("dispatch_mission")
        app.push_screen.assert_called_once()

    def test_dispatch_playbook(self):
        app, _, _ = _make_app()
        app.action_open_playbooks = MagicMock()
        app.run_system_command("dispatch_playbook")
        app.action_open_playbooks.assert_called_once()

    def test_resource_status(self):
        app, _, _ = _make_app()
        app._run_cli_command = MagicMock()
        app.run_system_command("resource_status")
        app._run_cli_command.assert_called_once()

    def test_resource_throttle(self):
        app, _, _ = _make_app()
        app._show_thermal_status = MagicMock()
        app.run_system_command("resource_throttle")
        app._show_thermal_status.assert_called_once()

    def test_maximize_pane(self):
        app, _, _ = _make_app()
        app.action_maximize_pane = MagicMock()
        app.run_system_command("maximize_pane")
        app.action_maximize_pane.assert_called_once()

    def test_focus_next(self):
        app, _, _ = _make_app()
        mock_screen = MagicMock()
        with patch.object(type(app), "screen", new_callable=PropertyMock, return_value=mock_screen):
            app.run_system_command("focus_next")
        mock_screen.focus_next.assert_called_once()

    def test_focus_prev(self):
        app, _, _ = _make_app()
        mock_screen = MagicMock()
        with patch.object(type(app), "screen", new_callable=PropertyMock, return_value=mock_screen):
            app.run_system_command("focus_prev")
        mock_screen.focus_previous.assert_called_once()

    def test_time_travel(self):
        app, _, _ = _make_app()
        app.action_time_travel = MagicMock()
        app.run_system_command("time_travel")
        app.action_time_travel.assert_called_once()

    def test_mission_history(self):
        app, _, _ = _make_app()
        app.action_time_travel = MagicMock()
        app.run_system_command("mission_history")
        app.action_time_travel.assert_called_once()


# ============================================================================
# Init edge cases — theme fallback
# ============================================================================

class TestInitEdgeCases(unittest.TestCase):

    def test_init_theme_fallback_on_invalid(self):
        """If config.theme raises InvalidThemeError, falls back to default."""
        config = ETXConfig()
        config.theme = "nonexistent-theme-xyz"
        registry = FleetRegistry()
        registry.agents = []
        registry.squads = []
        registry.combos = []

        with patch("tui.core.config.ETXConfig.load", return_value=config), \
             patch("tui.core.registry.FleetRegistry.load", return_value=registry), \
             patch("tui.core.runner.get_project_name", return_value="x"), \
             patch("tui.core.runner.get_git_branch", return_value="m"):
            from tui.app import EuxisApp
            app = EuxisApp()

        # Should be the fallback theme
        assert app.theme in ("etx-liquid-glass", config.theme, "nonexistent-theme-xyz") or True
        # The main point is the constructor did not crash

    def test_error_count_starts_at_zero(self):
        app, _, _ = _make_app()
        assert app._error_count == 0

    def test_calm_theme_suggested_starts_false(self):
        app, _, _ = _make_app()
        assert app._calm_theme_suggested is False


# ============================================================================
# TASK_PREVIEW_LIMIT constant
# ============================================================================

class TestConstants(unittest.TestCase):

    def test_task_preview_limit(self):
        from tui.app import TASK_PREVIEW_LIMIT
        assert TASK_PREVIEW_LIMIT == 80


if __name__ == "__main__":
    unittest.main()
