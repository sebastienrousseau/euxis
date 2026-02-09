#!/usr/bin/env python3
"""Comprehensive unit tests for HelpScreen.

Tests initialization, bindings, compose structure, on_mount header
configuration, HELP_CONTENT completeness, and action_go_back.
"""

import unittest
from unittest.mock import Mock, PropertyMock, patch

from tui.screens.help import HELP_CONTENT, HelpScreen


def _patch_screen_app(screen, mock_app):
    """Bypass Textual's read-only app property for testing."""
    patcher = patch.object(
        type(screen), "app", new_callable=PropertyMock, return_value=mock_app
    )
    patcher.start()
    return patcher


class TestHelpScreenInitialization(unittest.TestCase):
    """Tests for HelpScreen class attributes and initialization."""

    def test_screen_initialization(self):
        """Test HelpScreen can be instantiated."""
        screen = HelpScreen()
        assert isinstance(screen, HelpScreen)

    def test_bindings_count(self):
        """Test expected number of bindings."""
        screen = HelpScreen()
        assert len(screen.BINDINGS) == 2

    def test_bindings_keys(self):
        """Test BINDINGS contains escape and q."""
        screen = HelpScreen()
        keys = [b[0] for b in screen.BINDINGS]
        assert "escape" in keys
        assert "q" in keys

    def test_bindings_actions(self):
        """Test both bindings map to go_back action."""
        screen = HelpScreen()
        actions = [b[1] for b in screen.BINDINGS]
        assert all(a == "go_back" for a in actions)

    def test_bindings_descriptions(self):
        """Test bindings have correct descriptions."""
        screen = HelpScreen()
        descriptions = [b[2] for b in screen.BINDINGS]
        assert "Back" in descriptions
        assert "Close" in descriptions


class TestHelpContent(unittest.TestCase):
    """Tests for the HELP_CONTENT markdown constant."""

    def test_help_content_is_string(self):
        """Test HELP_CONTENT is a non-empty string."""
        assert isinstance(HELP_CONTENT, str)
        assert len(HELP_CONTENT) > 100

    def test_help_content_has_title(self):
        """Test HELP_CONTENT starts with the main heading."""
        assert "# ETX Keyboard Reference" in HELP_CONTENT

    def test_help_content_navigation_section(self):
        """Test HELP_CONTENT includes the Navigation section."""
        assert "## Navigation" in HELP_CONTENT

    def test_help_content_screens_section(self):
        """Test HELP_CONTENT includes the Screens section."""
        assert "## Screens" in HELP_CONTENT

    def test_help_content_agent_execution_section(self):
        """Test HELP_CONTENT includes the Agent Execution section."""
        assert "## Agent Execution" in HELP_CONTENT

    def test_help_content_command_palette_section(self):
        """Test HELP_CONTENT includes the Command Palette Prefixes section."""
        assert "## Command Palette Prefixes" in HELP_CONTENT

    def test_help_content_quick_start_section(self):
        """Test HELP_CONTENT includes the Quick Start section."""
        assert "## Quick Start" in HELP_CONTENT

    def test_help_content_agent_tiers_section(self):
        """Test HELP_CONTENT includes the Agent Tiers section."""
        assert "## Agent Tiers" in HELP_CONTENT

    def test_help_content_squads_section(self):
        """Test HELP_CONTENT includes the Squads section."""
        assert "## Squads" in HELP_CONTENT

    def test_help_content_combos_section(self):
        """Test HELP_CONTENT includes the Combos section."""
        assert "## Combos" in HELP_CONTENT

    def test_help_content_lists_key_shortcuts(self):
        """Test HELP_CONTENT documents essential keyboard shortcuts."""
        assert "`Ctrl+K`" in HELP_CONTENT
        assert "`Ctrl+Q`" in HELP_CONTENT
        assert "`F1`" in HELP_CONTENT
        assert "`Escape`" in HELP_CONTENT

    def test_help_content_lists_screens(self):
        """Test HELP_CONTENT documents all major screens."""
        for screen_name in [
            "Dashboard", "Agent Execution", "Fleet Monitor",
            "Settings", "Log Viewer", "Help", "About",
        ]:
            assert screen_name in HELP_CONTENT, f"Missing screen: {screen_name}"

    def test_help_content_lists_squads(self):
        """Test HELP_CONTENT documents squad names."""
        for squad in ["Vision", "Build", "Quality", "Growth", "Experience", "Specialist"]:
            assert squad in HELP_CONTENT, f"Missing squad: {squad}"

    def test_help_content_lists_combos(self):
        """Test HELP_CONTENT documents combo names."""
        for combo in ["Steve Jobs", "Fort Knox", "Content Factory", "Jony Ive"]:
            assert combo in HELP_CONTENT, f"Missing combo: {combo}"

    def test_help_content_has_markdown_tables(self):
        """Test HELP_CONTENT uses markdown table syntax."""
        assert "|" in HELP_CONTENT
        assert "---" in HELP_CONTENT


class TestHelpScreenCompose(unittest.TestCase):
    """Tests for the compose() method."""

    @patch("tui.screens.help.Footer")
    @patch("tui.screens.help.Markdown")
    @patch("tui.screens.help.VerticalScroll")
    @patch("tui.screens.help.ETXHeader")
    def test_compose_yields_widgets(
        self, mock_header, mock_vscroll, mock_markdown, mock_footer
    ):
        """Test compose() produces widget output."""
        mock_vscroll.return_value.__enter__ = Mock(return_value=Mock())
        mock_vscroll.return_value.__exit__ = Mock(return_value=False)

        screen = HelpScreen()
        result = list(screen.compose())
        assert len(result) > 0

    @patch("tui.screens.help.Footer")
    @patch("tui.screens.help.Markdown")
    @patch("tui.screens.help.VerticalScroll")
    @patch("tui.screens.help.ETXHeader")
    def test_compose_creates_header(
        self, mock_header, mock_vscroll, mock_markdown, mock_footer
    ):
        """Test compose() creates ETXHeader with id='header'."""
        mock_vscroll.return_value.__enter__ = Mock(return_value=Mock())
        mock_vscroll.return_value.__exit__ = Mock(return_value=False)

        screen = HelpScreen()
        list(screen.compose())
        mock_header.assert_called_once_with(id="header")

    @patch("tui.screens.help.Footer")
    @patch("tui.screens.help.Markdown")
    @patch("tui.screens.help.VerticalScroll")
    @patch("tui.screens.help.ETXHeader")
    def test_compose_creates_markdown_with_content(
        self, mock_header, mock_vscroll, mock_markdown, mock_footer
    ):
        """Test compose() creates Markdown widget with HELP_CONTENT."""
        mock_vscroll.return_value.__enter__ = Mock(return_value=Mock())
        mock_vscroll.return_value.__exit__ = Mock(return_value=False)

        screen = HelpScreen()
        list(screen.compose())
        mock_markdown.assert_called_once_with(HELP_CONTENT, id="help-content")

    @patch("tui.screens.help.Footer")
    @patch("tui.screens.help.Markdown")
    @patch("tui.screens.help.VerticalScroll")
    @patch("tui.screens.help.ETXHeader")
    def test_compose_creates_footer(
        self, mock_header, mock_vscroll, mock_markdown, mock_footer
    ):
        """Test compose() creates a Footer."""
        mock_vscroll.return_value.__enter__ = Mock(return_value=Mock())
        mock_vscroll.return_value.__exit__ = Mock(return_value=False)

        screen = HelpScreen()
        list(screen.compose())
        mock_footer.assert_called_once()


class TestHelpScreenOnMount(unittest.TestCase):
    """Tests for the on_mount() lifecycle method."""

    def setUp(self):
        self._patcher = None

    def tearDown(self):
        if self._patcher:
            self._patcher.stop()

    def test_on_mount_sets_header_project(self):
        """Test on_mount sets the header project to 'Help'."""
        screen = HelpScreen()
        mock_header = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            from tui.widgets.header import ETXHeader as HeaderCls

            if selector is HeaderCls:
                return mock_header
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)
        screen.on_mount()
        assert mock_header.project == "Help"


class TestHelpScreenActionGoBack(unittest.TestCase):
    """Tests for the action_go_back() method."""

    def setUp(self):
        self.mock_app = Mock()
        self._patcher = None

    def tearDown(self):
        if self._patcher:
            self._patcher.stop()

    def test_action_go_back_pops_screen(self):
        """Test action_go_back calls app.pop_screen."""
        screen = HelpScreen()
        self._patcher = _patch_screen_app(screen, self.mock_app)
        screen.action_go_back()
        self.mock_app.pop_screen.assert_called_once()


if __name__ == "__main__":
    unittest.main()
