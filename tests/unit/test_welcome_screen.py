#!/usr/bin/env python3
"""Comprehensive unit tests for WelcomeScreen.

Tests initialization, CSS, bindings, compose structure, on_mount version
display, button press handling, and action_go_dashboard.
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
        """Test Escape maps to quit."""
        screen = WelcomeScreen()
        binding_map = {b[0]: b[1] for b in screen.BINDINGS}
        assert binding_map["escape"] == "app.quit"

    def test_binding_descriptions(self):
        """Test bindings have descriptive labels."""
        screen = WelcomeScreen()
        descriptions = [b[2] for b in screen.BINDINGS]
        assert "Dashboard" in descriptions
        assert "Commands" in descriptions
        assert "Quit" in descriptions


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

    def test_css_contains_container_styles(self):
        """Test CSS includes welcome container styles."""
        screen = WelcomeScreen()
        assert "#welcome-container" in screen.DEFAULT_CSS

    def test_css_contains_logo_styles(self):
        """Test CSS includes logo styling."""
        screen = WelcomeScreen()
        assert "#welcome-logo" in screen.DEFAULT_CSS

    def test_css_contains_version_styles(self):
        """Test CSS includes version display styling."""
        screen = WelcomeScreen()
        assert "#welcome-version" in screen.DEFAULT_CSS

    def test_css_contains_actions_styles(self):
        """Test CSS includes action button area styling."""
        screen = WelcomeScreen()
        assert "#welcome-actions" in screen.DEFAULT_CSS

    def test_css_contains_hint_styles(self):
        """Test CSS includes hint text styling."""
        screen = WelcomeScreen()
        assert "#welcome-hint" in screen.DEFAULT_CSS


class TestSplashLogo(unittest.TestCase):
    """Tests for the SPLASH_LOGO constant."""

    def test_splash_logo_is_string(self):
        """Test SPLASH_LOGO is a non-empty string."""
        assert isinstance(SPLASH_LOGO, str)
        assert len(SPLASH_LOGO) > 0

    def test_splash_logo_has_markup(self):
        """Test SPLASH_LOGO uses Rich markup."""
        assert "[bold cyan]" in SPLASH_LOGO
        assert "[/]" in SPLASH_LOGO

    def test_splash_logo_has_branding(self):
        """Test SPLASH_LOGO includes branding text."""
        assert "Enterprise Unified eXecution Intelligence System" in SPLASH_LOGO

    def test_splash_logo_has_tagline(self):
        """Test SPLASH_LOGO includes the tagline."""
        assert "41 AI specialists" in SPLASH_LOGO


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

    @patch("tui.screens.welcome.Footer")
    @patch("tui.screens.welcome.Static")
    @patch("tui.screens.welcome.Button")
    @patch("tui.screens.welcome.Horizontal")
    @patch("tui.screens.welcome.Container")
    @patch("tui.screens.welcome.Center")
    def test_compose_yields_widgets(
        self, mock_center, mock_container, mock_horiz, mock_button, mock_static, mock_footer
    ):
        """Test compose() produces a non-empty widget list."""
        mock_center.return_value.__enter__ = Mock(return_value=Mock())
        mock_center.return_value.__exit__ = Mock(return_value=False)
        mock_container.return_value.__enter__ = Mock(return_value=Mock())
        mock_container.return_value.__exit__ = Mock(return_value=False)
        mock_horiz.return_value.__enter__ = Mock(return_value=Mock())
        mock_horiz.return_value.__exit__ = Mock(return_value=False)

        screen = WelcomeScreen()
        result = list(screen.compose())
        assert len(result) > 0

    @patch("tui.screens.welcome.Footer")
    @patch("tui.screens.welcome.Static")
    @patch("tui.screens.welcome.Button")
    @patch("tui.screens.welcome.Horizontal")
    @patch("tui.screens.welcome.Container")
    @patch("tui.screens.welcome.Center")
    def test_compose_creates_three_buttons(
        self, mock_center, mock_container, mock_horiz, mock_button, mock_static, mock_footer
    ):
        """Test compose() creates three Button widgets."""
        mock_center.return_value.__enter__ = Mock(return_value=Mock())
        mock_center.return_value.__exit__ = Mock(return_value=False)
        mock_container.return_value.__enter__ = Mock(return_value=Mock())
        mock_container.return_value.__exit__ = Mock(return_value=False)
        mock_horiz.return_value.__enter__ = Mock(return_value=Mock())
        mock_horiz.return_value.__exit__ = Mock(return_value=False)

        screen = WelcomeScreen()
        list(screen.compose())
        assert mock_button.call_count == 3

    @patch("tui.screens.welcome.Footer")
    @patch("tui.screens.welcome.Static")
    @patch("tui.screens.welcome.Button")
    @patch("tui.screens.welcome.Horizontal")
    @patch("tui.screens.welcome.Container")
    @patch("tui.screens.welcome.Center")
    def test_compose_button_ids(
        self, mock_center, mock_container, mock_horiz, mock_button, mock_static, mock_footer
    ):
        """Test compose() creates buttons with correct IDs."""
        mock_center.return_value.__enter__ = Mock(return_value=Mock())
        mock_center.return_value.__exit__ = Mock(return_value=False)
        mock_container.return_value.__enter__ = Mock(return_value=Mock())
        mock_container.return_value.__exit__ = Mock(return_value=False)
        mock_horiz.return_value.__enter__ = Mock(return_value=Mock())
        mock_horiz.return_value.__exit__ = Mock(return_value=False)

        screen = WelcomeScreen()
        list(screen.compose())

        button_ids = [call.kwargs.get("id") for call in mock_button.call_args_list]
        assert "btn-dashboard" in button_ids
        assert "btn-agent" in button_ids
        assert "btn-help" in button_ids

    @patch("tui.screens.welcome.Footer")
    @patch("tui.screens.welcome.Static")
    @patch("tui.screens.welcome.Button")
    @patch("tui.screens.welcome.Horizontal")
    @patch("tui.screens.welcome.Container")
    @patch("tui.screens.welcome.Center")
    def test_compose_creates_footer(
        self, mock_center, mock_container, mock_horiz, mock_button, mock_static, mock_footer
    ):
        """Test compose() creates a Footer."""
        mock_center.return_value.__enter__ = Mock(return_value=Mock())
        mock_center.return_value.__exit__ = Mock(return_value=False)
        mock_container.return_value.__enter__ = Mock(return_value=Mock())
        mock_container.return_value.__exit__ = Mock(return_value=False)
        mock_horiz.return_value.__enter__ = Mock(return_value=Mock())
        mock_horiz.return_value.__exit__ = Mock(return_value=False)

        screen = WelcomeScreen()
        list(screen.compose())
        mock_footer.assert_called_once()


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

        mock_version = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            if selector == "#welcome-version":
                return mock_version
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)
        return screen, mock_version

    def test_on_mount_updates_version_widget(self):
        """Test on_mount calls update() on the version Static widget."""
        screen, mock_version = self._create_mounted_screen()
        screen.on_mount()
        mock_version.update.assert_called_once()

    def test_on_mount_version_contains_version(self):
        """Test version update includes the registry version."""
        screen, mock_version = self._create_mounted_screen()
        screen.on_mount()
        text = mock_version.update.call_args[0][0]
        assert "2.0.0" in text

    def test_on_mount_version_contains_agent_count(self):
        """Test version update includes the agent count."""
        screen, mock_version = self._create_mounted_screen()
        screen.on_mount()
        text = mock_version.update.call_args[0][0]
        assert "3 agents" in text

    def test_on_mount_version_contains_squad_count(self):
        """Test version update includes the squad count."""
        screen, mock_version = self._create_mounted_screen()
        screen.on_mount()
        text = mock_version.update.call_args[0][0]
        assert "2 squads" in text

    def test_on_mount_version_contains_combo_count(self):
        """Test version update includes the combo count."""
        screen, mock_version = self._create_mounted_screen()
        screen.on_mount()
        text = mock_version.update.call_args[0][0]
        assert "1 combos" in text


class TestWelcomeScreenButtonPressed(unittest.TestCase):
    """Tests for the on_button_pressed event handler."""

    def setUp(self):
        self.mock_app = Mock()
        self._patcher = None

    def tearDown(self):
        if self._patcher:
            self._patcher.stop()

    def _create_screen(self):
        screen = WelcomeScreen()
        self._patcher = _patch_screen_app(screen, self.mock_app)
        screen.dismiss = Mock()
        return screen

    def test_btn_dashboard_calls_action_go_dashboard(self):
        """Test btn-dashboard button triggers action_go_dashboard."""
        screen = self._create_screen()
        mock_event = Mock()
        mock_event.button.id = "btn-dashboard"

        with patch.object(screen, "action_go_dashboard") as mock_action:
            screen.on_button_pressed(mock_event)
            mock_action.assert_called_once()

    def test_btn_agent_deploys_architect(self):
        """Test btn-agent button dismisses and deploys architect."""
        screen = self._create_screen()
        mock_event = Mock()
        mock_event.button.id = "btn-agent"

        screen.on_button_pressed(mock_event)
        screen.dismiss.assert_called_once()
        self.mock_app.action_deploy_agent.assert_called_once_with("architect")

    def test_btn_help_opens_help(self):
        """Test btn-help button dismisses and opens help."""
        screen = self._create_screen()
        mock_event = Mock()
        mock_event.button.id = "btn-help"

        screen.on_button_pressed(mock_event)
        screen.dismiss.assert_called_once()
        self.mock_app.action_help.assert_called_once()

    def test_unknown_button_does_nothing(self):
        """Test unknown button ID does not trigger any action."""
        screen = self._create_screen()
        mock_event = Mock()
        mock_event.button.id = "btn-unknown"

        screen.on_button_pressed(mock_event)
        screen.dismiss.assert_not_called()
        self.mock_app.action_deploy_agent.assert_not_called()
        self.mock_app.action_help.assert_not_called()


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
