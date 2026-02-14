#!/usr/bin/env python3
"""Comprehensive unit tests for AboutScreen.

Tests initialization, bindings, CSS, compose structure, on_mount data
population, and action_go_back navigation.
"""

import platform
import sys
import unittest
from unittest.mock import Mock, PropertyMock, patch

from tui.core.config import ETXConfig
from tui.core.registry import Agent, Combo, FleetRegistry, Squad
from tui.screens.about import EUXIS_LOGO, AboutScreen


def _patch_screen_app(screen, mock_app):
    """Bypass Textual's read-only app property for testing."""
    patcher = patch.object(
        type(screen), "app", new_callable=PropertyMock, return_value=mock_app
    )
    patcher.start()
    return patcher


class TestAboutScreenInitialization(unittest.TestCase):
    """Tests for AboutScreen class attributes and initialization."""

    def test_screen_initialization(self):
        """Test AboutScreen can be instantiated."""
        screen = AboutScreen()
        assert isinstance(screen, AboutScreen)

    def test_bindings_defined(self):
        """Test BINDINGS contains escape and q keys."""
        screen = AboutScreen()
        keys = [b[0] for b in screen.BINDINGS]
        assert "escape" in keys
        assert "q" in keys

    def test_binding_actions(self):
        """Test both bindings invoke the go_back action."""
        screen = AboutScreen()
        for binding in screen.BINDINGS:
            assert binding[1] == "go_back"

    def test_binding_descriptions(self):
        """Test bindings have descriptive labels."""
        screen = AboutScreen()
        descriptions = [b[2] for b in screen.BINDINGS]
        assert "Back" in descriptions
        assert "Close" in descriptions

    def test_bindings_count(self):
        """Test the expected number of bindings."""
        screen = AboutScreen()
        assert len(screen.BINDINGS) == 2


class TestAboutScreenLogo(unittest.TestCase):
    """Tests for the EUXIS_LOGO constant."""

    def test_logo_is_string(self):
        """Test EUXIS_LOGO is a non-empty string."""
        assert isinstance(EUXIS_LOGO, str)
        assert len(EUXIS_LOGO) > 0

    def test_logo_contains_euxis_text(self):
        """Test the logo contains the EUXIS block characters."""
        assert "EUXIS" in EUXIS_LOGO or "█" in EUXIS_LOGO

    def test_logo_has_markup(self):
        """Test the logo uses Rich markup for styling."""
        assert "[bold]" in EUXIS_LOGO
        assert "[/]" in EUXIS_LOGO


class TestAboutScreenEuxisAppProperty(unittest.TestCase):
    """Tests for the euxis_app property."""

    def setUp(self):
        self.mock_app = Mock()
        self._patcher = None

    def tearDown(self):
        if self._patcher:
            self._patcher.stop()

    def test_euxis_app_returns_app(self):
        """Test euxis_app property returns self.app."""
        screen = AboutScreen()
        self._patcher = _patch_screen_app(screen, self.mock_app)
        assert screen.euxis_app is self.mock_app


class TestAboutScreenCompose(unittest.TestCase):
    """Tests for the compose() method."""

    @patch("tui.screens.about.Static")
    @patch("tui.screens.about.Center")
    @patch("tui.screens.about.VerticalScroll")
    @patch("tui.screens.about.ETXHeader")
    def test_compose_yields_widgets(
        self, mock_header, mock_vscroll, mock_center, mock_static
    ):
        """Test compose() produces a non-empty widget list."""
        # Make context managers return mocks that yield nothing on __enter__
        mock_vscroll.return_value.__enter__ = Mock(return_value=Mock())
        mock_vscroll.return_value.__exit__ = Mock(return_value=False)
        mock_center.return_value.__enter__ = Mock(return_value=Mock())
        mock_center.return_value.__exit__ = Mock(return_value=False)

        screen = AboutScreen()
        result = list(screen.compose())
        assert len(result) > 0

    @patch("tui.screens.about.Static")
    @patch("tui.screens.about.Center")
    @patch("tui.screens.about.VerticalScroll")
    @patch("tui.screens.about.ETXHeader")
    def test_compose_creates_header(
        self, mock_header, mock_vscroll, mock_center, mock_static
    ):
        """Test compose() creates an ETXHeader with id='header'."""
        mock_vscroll.return_value.__enter__ = Mock(return_value=Mock())
        mock_vscroll.return_value.__exit__ = Mock(return_value=False)
        mock_center.return_value.__enter__ = Mock(return_value=Mock())
        mock_center.return_value.__exit__ = Mock(return_value=False)

        screen = AboutScreen()
        list(screen.compose())
        mock_header.assert_called_once_with(id="header")

    @patch("tui.screens.about.Static")
    @patch("tui.screens.about.Center")
    @patch("tui.screens.about.VerticalScroll")
    @patch("tui.screens.about.ETXHeader")
    def test_compose_creates_shortcut_bar(
        self, mock_header, mock_vscroll, mock_center, mock_static
    ):
        """Test compose() creates a ShortcutBar."""
        mock_vscroll.return_value.__enter__ = Mock(return_value=Mock())
        mock_vscroll.return_value.__exit__ = Mock(return_value=False)
        mock_center.return_value.__enter__ = Mock(return_value=Mock())
        mock_center.return_value.__exit__ = Mock(return_value=False)

        screen = AboutScreen()
        result = list(screen.compose())
        from tui.widgets.shortcut_bar import ShortcutBar
        assert any(isinstance(w, ShortcutBar) for w in result)


class TestAboutScreenOnMount(unittest.TestCase):
    """Tests for the on_mount() lifecycle method."""

    def setUp(self):
        self.mock_app = Mock()
        self.registry = FleetRegistry()
        self.registry.version = "1.2.3"
        self.registry.agents = [
            Agent(id="architect", tier="core", version="1.2.3", tags=(), activation="default"),
            Agent(id="debugger", tier="fleet", version="1.2.3", tags=(), activation="default"),
        ]
        self.registry.squads = [
            Squad(id="dev", name="Dev", purpose="Build", lead="architect", members=("architect",)),
        ]
        self.registry.combos = [
            Combo(id="combo1", name="Combo", description="desc", chain=("architect", "debugger")),
        ]
        self.config = ETXConfig(theme="etx-dark", default_provider="claude")
        self.mock_app.fleet_registry = self.registry
        self.mock_app.config = self.config
        self._patcher = None

    def tearDown(self):
        if self._patcher:
            self._patcher.stop()

    def _create_mounted_screen(self):
        """Create an AboutScreen with mocked query_one and app."""
        screen = AboutScreen()
        self._patcher = _patch_screen_app(screen, self.mock_app)

        mock_header = Mock()
        mock_info = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            from tui.widgets.header import ETXHeader as HeaderCls

            if selector is HeaderCls:
                return mock_header
            if selector == "#about-info":
                return mock_info
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)
        return screen, mock_header, mock_info

    def test_on_mount_sets_header_project(self):
        """Test on_mount sets the header project to 'About'."""
        screen, mock_header, _ = self._create_mounted_screen()
        screen.on_mount()
        assert mock_header.project == "About"

    def test_on_mount_updates_info_widget(self):
        """Test on_mount calls update() on the info Static widget."""
        screen, _, mock_info = self._create_mounted_screen()
        screen.on_mount()
        mock_info.update.assert_called_once()

    def test_on_mount_info_contains_version(self):
        """Test info update includes the registry version."""
        screen, _, mock_info = self._create_mounted_screen()
        screen.on_mount()
        text = mock_info.update.call_args[0][0]
        assert "1.2.3" in text

    def test_on_mount_info_contains_agent_count(self):
        """Test info update includes the agent count."""
        screen, _, mock_info = self._create_mounted_screen()
        screen.on_mount()
        text = mock_info.update.call_args[0][0]
        assert "2" in text  # 2 agents

    def test_on_mount_info_contains_squad_count(self):
        """Test info update includes the squad count."""
        screen, _, mock_info = self._create_mounted_screen()
        screen.on_mount()
        text = mock_info.update.call_args[0][0]
        assert "1" in text  # 1 squad

    def test_on_mount_info_contains_combo_count(self):
        """Test info update includes the combo count."""
        screen, _, mock_info = self._create_mounted_screen()
        screen.on_mount()
        text = mock_info.update.call_args[0][0]
        assert "1" in text  # 1 combo

    def test_on_mount_info_contains_theme(self):
        """Test info update includes the theme name."""
        screen, _, mock_info = self._create_mounted_screen()
        screen.on_mount()
        text = mock_info.update.call_args[0][0]
        assert "etx-dark" in text

    def test_on_mount_info_contains_provider(self):
        """Test info update includes the default provider."""
        screen, _, mock_info = self._create_mounted_screen()
        screen.on_mount()
        text = mock_info.update.call_args[0][0]
        assert "claude" in text

    def test_on_mount_info_contains_python_version(self):
        """Test info update includes the current Python version."""
        screen, _, mock_info = self._create_mounted_screen()
        screen.on_mount()
        text = mock_info.update.call_args[0][0]
        assert sys.version.split()[0] in text

    def test_on_mount_info_contains_platform(self):
        """Test info update includes the platform system name."""
        screen, _, mock_info = self._create_mounted_screen()
        screen.on_mount()
        text = mock_info.update.call_args[0][0]
        assert platform.system() in text

    def test_on_mount_info_contains_branding(self):
        """Test info update includes Euxis branding text."""
        screen, _, mock_info = self._create_mounted_screen()
        screen.on_mount()
        text = mock_info.update.call_args[0][0]
        assert "Sebastien Rousseau" in text
        assert "Build something that matters" in text


class TestAboutScreenActionGoBack(unittest.TestCase):
    """Tests for the action_go_back() method."""

    def setUp(self):
        self.mock_app = Mock()
        self._patcher = None

    def tearDown(self):
        if self._patcher:
            self._patcher.stop()

    def test_action_go_back_pops_screen(self):
        """Test action_go_back calls app.pop_screen."""
        screen = AboutScreen()
        self._patcher = _patch_screen_app(screen, self.mock_app)
        screen.action_go_back()
        self.mock_app.pop_screen.assert_called_once()


if __name__ == "__main__":
    unittest.main()
