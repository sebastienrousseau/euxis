# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Comprehensive unit tests for ProviderSelectModal.

Tests cover: compose (option list with current provider marker, all providers
listed), on_option_list_option_selected (dismiss with ID, ignore empty ID),
Selected message.
"""

from __future__ import annotations

import unittest
from unittest.mock import Mock, PropertyMock, patch

from tui.core.runner import PROVIDERS
from tui.widgets.provider_select import ProviderSelectModal


def _patch_screen_app(screen, mock_app):
    patcher = patch.object(
        type(screen), "app", new_callable=PropertyMock, return_value=mock_app
    )
    patcher.start()
    return patcher


class TestProviderSelectModalInit(unittest.TestCase):
    def test_default_current(self):
        modal = ProviderSelectModal()
        assert modal.current == "claude"

    def test_custom_current(self):
        modal = ProviderSelectModal(current="gemini")
        assert modal.current == "gemini"

    def test_bindings(self):
        modal = ProviderSelectModal()
        keys = [b[0] for b in modal.BINDINGS]
        assert "escape" in keys


class TestProviderSelectModalCompose(unittest.TestCase):
    @patch("tui.widgets.provider_select.OptionList")
    @patch("tui.widgets.provider_select.Static")
    @patch("tui.widgets.provider_select.Container")
    def test_compose_yields_widgets(self, mock_container, mock_static, mock_option_list):
        modal = ProviderSelectModal(current="claude")
        result = list(modal.compose())
        assert len(result) > 0

    @patch("tui.widgets.provider_select.Container")
    @patch("tui.widgets.provider_select.Static")
    @patch("tui.widgets.provider_select.OptionList")
    def test_all_providers_listed(self, mock_option_list_cls, mock_static, mock_container):
        instance = Mock()
        mock_option_list_cls.return_value = instance

        modal = ProviderSelectModal(current="claude")
        list(modal.compose())

        # Each provider should result in an add_option call
        assert instance.add_option.call_count == len(PROVIDERS)

    @patch("tui.widgets.provider_select.Container")
    @patch("tui.widgets.provider_select.Static")
    @patch("tui.widgets.provider_select.OptionList")
    def test_current_provider_marked(self, mock_option_list_cls, mock_static, mock_container):
        instance = Mock()
        mock_option_list_cls.return_value = instance

        modal = ProviderSelectModal(current="gemini")
        list(modal.compose())

        # Find the call that includes "(current)"
        options = [c[0][0] for c in instance.add_option.call_args_list]
        current_options = [o for o in options if "(current)" in o.prompt]
        assert len(current_options) == 1
        assert "Gemini" in current_options[0].prompt

    @patch("tui.widgets.provider_select.Container")
    @patch("tui.widgets.provider_select.Static")
    @patch("tui.widgets.provider_select.OptionList")
    def test_non_current_not_marked(self, mock_option_list_cls, mock_static, mock_container):
        instance = Mock()
        mock_option_list_cls.return_value = instance

        modal = ProviderSelectModal(current="claude")
        list(modal.compose())

        options = [c[0][0] for c in instance.add_option.call_args_list]
        non_current = [o for o in options if "(current)" not in o.prompt]
        assert len(non_current) == len(PROVIDERS) - 1


class TestProviderSelectModalOptionSelected(unittest.TestCase):
    def test_dismiss_with_id(self):
        modal = ProviderSelectModal()
        modal.dismiss = Mock()

        event = Mock()
        event.option.id = "gemini"
        modal.on_option_list_option_selected(event)

        modal.dismiss.assert_called_once_with("gemini")

    def test_ignore_empty_id(self):
        modal = ProviderSelectModal()
        modal.dismiss = Mock()

        event = Mock()
        event.option.id = ""
        modal.on_option_list_option_selected(event)

        modal.dismiss.assert_not_called()

    def test_ignore_none_id(self):
        modal = ProviderSelectModal()
        modal.dismiss = Mock()

        event = Mock()
        event.option.id = None
        modal.on_option_list_option_selected(event)

        modal.dismiss.assert_not_called()


class TestProviderSelectModalSelectedMessage(unittest.TestCase):
    def test_selected_message(self):
        msg = ProviderSelectModal.Selected("gemini")
        assert msg.provider == "gemini"

    def test_selected_message_claude(self):
        msg = ProviderSelectModal.Selected("claude")
        assert msg.provider == "claude"


if __name__ == "__main__":
    unittest.main()
