# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

#!/usr/bin/env python3
"""Comprehensive unit tests for WelcomeScreen.

Tests initialization, CSS, bindings, compose structure, on_mount version
display, and action_go_dashboard.

Updated to match the actual minimal welcome screen implementation which uses:
- Static widgets (no Buttons, no Footer)
- Middle/Center containers (no Container/Horizontal)
- ShortcutBar instead of Footer
- SPLASH_LOGO with [bold] markup (not [bold cyan])
- #welcome-stats and #welcome-prompt (not #welcome-version/#welcome-actions)
"""

import unittest
from unittest.mock import Mock, PropertyMock, patch

from tui.core.registry import Agent, Combo, FleetRegistry, Squad
from tui.screens.welcome import SPLASH_LOGO, WelcomeScreen


def _patch_screen_app(screen, mock_app):
    """Bypass Textual's read-only app property for testing."""
    patcher = patch.object(
        type(screen), "app", new_callable=PropertyMock, return_value=mock_app
    )
    patcher.start()
    return patcher


class TestWelcomeScreenInitialization(unittest.TestCase):
    """Tests for WelcomeScreen class attributes and initialization."""

    def test_screen_initialization(self):
        """Test WelcomeScreen can be instantiated."""
        screen = WelcomeScreen()
        assert isinstance(screen, WelcomeScreen)

    def test_bindings_count(self):
        """Test expected number of bindings."""
        screen = WelcomeScreen()
        assert len(screen.BINDINGS) == 3

    def test_bindings_keys(self):
        """Test BINDINGS contains expected keys."""
        screen = WelcomeScreen()
        keys = [b[0] for b in screen.BINDINGS]
        assert "enter" in keys
        assert "ctrl+k" in keys
        assert "escape" in keys

    def test_binding_enter_action(self):
        """Test Enter maps to go_dashboard."""
        screen = WelcomeScreen()
        binding_map = {b[0]: b[1] for b in screen.BINDINGS}
        assert binding_map["enter"] == "go_dashboard"

    def test_binding_ctrl_k_action(self):
        """Test Ctrl+K maps to command palette."""
        screen = WelcomeScreen()
        binding_map = {b[0]: b[1] for b in screen.BINDINGS}
        assert binding_map["ctrl+k"] == "app.command_palette"

    def test_binding_escape_action(self):
        """Test Escape maps to go_dashboard."""
        screen = WelcomeScreen()
        binding_map = {b[0]: b[1] for b in screen.BINDINGS}
        assert binding_map["escape"] == "go_dashboard"

    def test_binding_descriptions(self):
        """Test bindings have descriptive labels."""
        screen = WelcomeScreen()
        descriptions = [b[2] for b in screen.BINDINGS]
        assert "Dashboard" in descriptions
        assert "Commands" in descriptions
        assert "Back" in descriptions


class TestWelcomeScreenCSS(unittest.TestCase):
    """Tests for the DEFAULT_CSS constant."""

    def test_css_is_defined(self):
        """Test DEFAULT_CSS is a non-empty string."""
        screen = WelcomeScreen()
        assert isinstance(screen.DEFAULT_CSS, str)
        assert len(screen.DEFAULT_CSS) > 0

    def test_css_contains_welcome_screen_selector(self):
        """Test CSS targets WelcomeScreen."""
        screen = WelcomeScreen()
        assert "WelcomeScreen" in screen.DEFAULT_CSS

    def test_css_contains_logo_styles(self):
        """Test CSS includes logo styling."""
        screen = WelcomeScreen()
        assert "#welcome-logo" in screen.DEFAULT_CSS

    def test_css_contains_stats_styles(self):
        """Test CSS includes stats display styling."""
        screen = WelcomeScreen()
        assert "#welcome-stats" in screen.DEFAULT_CSS

    def test_css_contains_prompt_styles(self):
        """Test CSS includes prompt text styling."""
        screen = WelcomeScreen()
        assert "#welcome-prompt" in screen.DEFAULT_CSS


class TestSplashLogo(unittest.TestCase):
    """Tests for the SPLASH_LOGO constant."""

    def test_splash_logo_is_string(self):
        """Test SPLASH_LOGO is a non-empty string."""
        assert isinstance(SPLASH_LOGO, str)
        assert len(SPLASH_LOGO) > 0

    def test_splash_logo_has_markup(self):
        """Test SPLASH_LOGO uses Rich markup."""
        assert "[bold]" in SPLASH_LOGO
        assert "[/]" in SPLASH_LOGO

    def test_splash_logo_has_block_characters(self):
        """Test SPLASH_LOGO includes block art characters."""
        assert "\u2588" in SPLASH_LOGO  # Full block character


class TestWelcomeScreenEuxisAppProperty(unittest.TestCase):
    """Tests for the euxis_app property."""

    def setUp(self):
        self.mock_app = Mock()
        self._patcher = None

    def tearDown(self):
        if self._patcher:
            self._patcher.stop()

    def test_euxis_app_returns_app(self):
        """Test euxis_app property returns self.app."""
        screen = WelcomeScreen()
        self._patcher = _patch_screen_app(screen, self.mock_app)
        assert screen.euxis_app is self.mock_app


class TestWelcomeScreenCompose(unittest.TestCase):
    """Tests for the compose() method."""

    @patch("tui.screens.welcome.Static")
    @patch("tui.screens.welcome.Center")
    @patch("tui.screens.welcome.Middle")
    def test_compose_yields_widgets(
        self, mock_middle, mock_center, mock_static
    ):
        """Test compose() produces a non-empty widget list."""
        mock_middle.return_value.__enter__ = Mock(return_value=Mock())
        mock_middle.return_value.__exit__ = Mock(return_value=False)
        mock_center.return_value.__enter__ = Mock(return_value=Mock())
        mock_center.return_value.__exit__ = Mock(return_value=False)

        screen = WelcomeScreen()
        result = list(screen.compose())
        assert len(result) > 0

    @patch("tui.screens.welcome.Static")
    @patch("tui.screens.welcome.Center")
    @patch("tui.screens.welcome.Middle")
    def test_compose_creates_statics(
        self, mock_middle, mock_center, mock_static
    ):
        """Test compose() creates Static widgets for logo, stats, and prompt."""
        mock_middle.return_value.__enter__ = Mock(return_value=Mock())
        mock_middle.return_value.__exit__ = Mock(return_value=False)
        mock_center.return_value.__enter__ = Mock(return_value=Mock())
        mock_center.return_value.__exit__ = Mock(return_value=False)

        screen = WelcomeScreen()
        list(screen.compose())
        # 3 Static widgets: logo, stats, prompt
        assert mock_static.call_count == 3

    @patch("tui.screens.welcome.Static")
    @patch("tui.screens.welcome.Center")
    @patch("tui.screens.welcome.Middle")
    def test_compose_creates_shortcut_bar(
        self, mock_middle, mock_center, mock_static
    ):
        """Test compose() creates a ShortcutBar."""
        mock_middle.return_value.__enter__ = Mock(return_value=Mock())
        mock_middle.return_value.__exit__ = Mock(return_value=False)
        mock_center.return_value.__enter__ = Mock(return_value=Mock())
        mock_center.return_value.__exit__ = Mock(return_value=False)

        screen = WelcomeScreen()
        result = list(screen.compose())
        # The last yielded item should be the ShortcutBar
        from tui.widgets.shortcut_bar import ShortcutBar
        assert any(isinstance(w, ShortcutBar) for w in result)


class TestWelcomeScreenOnMount(unittest.TestCase):
    """Tests for the on_mount() lifecycle method."""

    def setUp(self):
        self.mock_app = Mock()
        self.registry = FleetRegistry()
        self.registry.version = "2.0.0"
        self.registry.agents = [
            Agent(id="a1", tier="core", version="2.0.0", tags=(), activation="default"),
            Agent(id="a2", tier="fleet", version="2.0.0", tags=(), activation="default"),
            Agent(id="a3", tier="fleet", version="2.0.0", tags=(), activation="on-demand"),
        ]
        self.registry.squads = [
            Squad(id="s1", name="S1", purpose="p", lead="a1", members=("a1", "a2")),
            Squad(id="s2", name="S2", purpose="p", lead="a2", members=("a2",)),
        ]
        self.registry.combos = [
            Combo(id="c1", name="C1", description="d", chain=("a1", "a2")),
        ]
        self.mock_app.fleet_registry = self.registry
        self._patcher = None

    def tearDown(self):
        if self._patcher:
            self._patcher.stop()

    def _create_mounted_screen(self):
        """Create a WelcomeScreen with mocked query_one and app."""
        screen = WelcomeScreen()
        self._patcher = _patch_screen_app(screen, self.mock_app)

        mock_stats = Mock()
        mock_prompt = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            if selector == "#welcome-stats":
                return mock_stats
            if selector == "#welcome-prompt":
                return mock_prompt
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)
        return screen, mock_stats

    def test_on_mount_updates_version_widget(self):
        """Test on_mount calls update() on the stats Static widget."""
        screen, mock_stats = self._create_mounted_screen()
        screen.on_mount()
        mock_stats.update.assert_called_once()

    def test_on_mount_version_contains_version(self):
        """Test stats update includes the registry version."""
        screen, mock_stats = self._create_mounted_screen()
        screen.on_mount()
        text = mock_stats.update.call_args[0][0]
        assert "2.0.0" in text

    def test_on_mount_version_contains_agent_count(self):
        """Test stats update includes the agent count."""
        screen, mock_stats = self._create_mounted_screen()
        screen.on_mount()
        text = mock_stats.update.call_args[0][0]
        assert "3 agents" in text

    def test_on_mount_version_contains_squad_count(self):
        """Test stats update includes the squad count."""
        screen, mock_stats = self._create_mounted_screen()
        screen.on_mount()
        text = mock_stats.update.call_args[0][0]
        assert "2 squads" in text

    def test_on_mount_version_contains_combo_count(self):
        """Test stats update includes the combo count."""
        screen, mock_stats = self._create_mounted_screen()
        screen.on_mount()
        text = mock_stats.update.call_args[0][0]
        assert "1 combos" in text


class TestWelcomeScreenActionGoDashboard(unittest.TestCase):
    """Tests for the action_go_dashboard() method."""

    def setUp(self):
        self.mock_app = Mock()
        self._patcher = None

    def tearDown(self):
        if self._patcher:
            self._patcher.stop()

    def test_action_go_dashboard_dismisses(self):
        """Test action_go_dashboard calls self.dismiss()."""
        screen = WelcomeScreen()
        self._patcher = _patch_screen_app(screen, self.mock_app)
        screen.dismiss = Mock()
        screen.action_go_dashboard()
        screen.dismiss.assert_called_once()


if __name__ == "__main__":
    unittest.main()
