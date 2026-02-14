"""Comprehensive unit tests for SettingsScreen.

Tests cover: compose with form widgets, on_button_pressed (save-btn/cancel-btn
branching), action_save_settings (Select value extraction, BLANK skip, Switch
values, config.save(), theme application, notification, screen pop), actions.
"""

from __future__ import annotations

import unittest
from unittest.mock import Mock, PropertyMock, patch

from tui.screens.settings import THEME_OPTIONS, SettingsScreen


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
    mock_app.config.theme = "textual-dark"
    mock_app.config.default_provider = "claude"
    mock_app.config.show_agent_tags = True
    mock_app.config.reduced_motion = False
    mock_app.config.accessible_mode = False
    mock_app.config.save = Mock()
    mock_app.theme = "textual-dark"
    return mock_app


class TestSettingsScreenInit(unittest.TestCase):
    def test_bindings(self):
        screen = SettingsScreen()
        keys = [b[0] for b in screen.BINDINGS]
        assert "escape" in keys
        assert "ctrl+s" in keys
        assert "ctrl+k" in keys

    def test_euxis_app_property(self):
        screen = SettingsScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)
        try:
            assert screen.euxis_app is mock_app
        finally:
            patcher.stop()


class TestSettingsScreenCompose(unittest.TestCase):
    @patch("tui.screens.settings.Button")
    @patch("tui.screens.settings.Switch")
    @patch("tui.screens.settings.Select")
    @patch("tui.screens.settings.Label")
    @patch("tui.screens.settings.Static")
    @patch("tui.screens.settings.Horizontal")
    @patch("tui.screens.settings.Container")
    @patch("tui.screens.settings.ETXHeader")
    def test_compose_yields_widgets(self, *mocks):
        screen = SettingsScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)
        try:
            result = list(screen.compose())
            assert len(result) > 0
        finally:
            patcher.stop()


class TestSettingsScreenOnMount(unittest.TestCase):
    def test_on_mount_header(self):
        screen = SettingsScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)

        mock_header = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            if hasattr(selector, "__name__"):
                return mock_header
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)
        screen.on_mount()

        assert mock_header.project == "test-project"
        assert mock_header.branch == "main"
        assert mock_header.provider == "claude"
        patcher.stop()


class TestSettingsScreenOnButtonPressed(unittest.TestCase):
    def setUp(self):
        self.screen = SettingsScreen()
        self.mock_app = _make_mock_app()
        self._patcher = _patch_screen_app(self.screen, self.mock_app)

    def tearDown(self):
        self._patcher.stop()

    def test_save_button(self):
        with patch.object(self.screen, "action_save_settings") as mock_save:
            event = Mock()
            event.button.id = "save-btn"
            self.screen.on_button_pressed(event)
            mock_save.assert_called_once()

    def test_cancel_button(self):
        with patch.object(self.screen, "action_go_back") as mock_back:
            event = Mock()
            event.button.id = "cancel-btn"
            self.screen.on_button_pressed(event)
            mock_back.assert_called_once()

    def test_unknown_button_does_nothing(self):
        with patch.object(self.screen, "action_save_settings") as mock_save:
            with patch.object(self.screen, "action_go_back") as mock_back:
                event = Mock()
                event.button.id = "other-btn"
                self.screen.on_button_pressed(event)
                mock_save.assert_not_called()
                mock_back.assert_not_called()


class TestSettingsScreenSaveSettings(unittest.TestCase):
    def setUp(self):
        self.screen = SettingsScreen()
        self.mock_app = _make_mock_app()
        self._patcher = _patch_screen_app(self.screen, self.mock_app)
        self.screen.notify = Mock()

        # Create mock widgets
        self.mock_theme_select = Mock()
        self.mock_theme_select.value = "textual-light"

        self.mock_provider_select = Mock()
        self.mock_provider_select.value = "gemini"

        self.mock_tags_switch = Mock()
        self.mock_tags_switch.value = False

        self.mock_motion_switch = Mock()
        self.mock_motion_switch.value = True

        self.mock_accessible_switch = Mock()
        self.mock_accessible_switch.value = True

        def query_one_side_effect(selector, *args, **kwargs):
            mapping = {
                "#theme-select": self.mock_theme_select,
                "#provider-select": self.mock_provider_select,
                "#tags-switch": self.mock_tags_switch,
                "#motion-switch": self.mock_motion_switch,
                "#accessible-switch": self.mock_accessible_switch,
            }
            return mapping.get(selector, Mock())

        self.screen.query_one = Mock(side_effect=query_one_side_effect)

    def tearDown(self):
        self._patcher.stop()

    def test_saves_theme(self):
        # Import Select to get BLANK sentinel
        from textual.widgets import Select

        self.mock_theme_select.value = "textual-light"
        self.screen.action_save_settings()

        assert self.mock_app.config.theme == "textual-light"
        assert self.mock_app.theme == "textual-light"

    def test_blank_theme_skipped(self):
        from textual.widgets import Select

        self.mock_theme_select.value = Select.BLANK
        original_theme = self.mock_app.config.theme
        self.screen.action_save_settings()

        assert self.mock_app.config.theme == original_theme

    def test_saves_provider(self):
        self.screen.action_save_settings()
        assert self.mock_app.config.default_provider == "gemini"

    def test_blank_provider_skipped(self):
        from textual.widgets import Select

        self.mock_provider_select.value = Select.BLANK
        original = self.mock_app.config.default_provider
        self.screen.action_save_settings()
        assert self.mock_app.config.default_provider == original

    def test_saves_switch_values(self):
        self.screen.action_save_settings()
        assert self.mock_app.config.show_agent_tags is False
        assert self.mock_app.config.reduced_motion is True
        assert self.mock_app.config.accessible_mode is True

    def test_calls_config_save(self):
        self.screen.action_save_settings()
        self.mock_app.config.save.assert_called_once()

    def test_sends_notification(self):
        self.screen.action_save_settings()
        self.screen.notify.assert_called_once()
        assert "saved" in self.screen.notify.call_args[0][0].lower()

    def test_pops_screen(self):
        self.screen.action_save_settings()
        self.mock_app.pop_screen.assert_called_once()


class TestSettingsScreenActions(unittest.TestCase):
    def test_action_go_back(self):
        screen = SettingsScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)
        try:
            screen.action_go_back()
            mock_app.pop_screen.assert_called_once()
        finally:
            patcher.stop()


class TestThemeOptions(unittest.TestCase):
    def test_theme_options_defined(self):
        assert len(THEME_OPTIONS) >= 3
        for name, value in THEME_OPTIONS:
            assert isinstance(name, str)
            assert isinstance(value, str)


if __name__ == "__main__":
    unittest.main()
