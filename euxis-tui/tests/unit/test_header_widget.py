# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

#!/usr/bin/env python3
"""Comprehensive unit tests for ETXHeader widget.

Tests initialization, reactive defaults, CSS, compose structure,
_update_display logic, and watcher methods.

Because ETXHeader uses Textual reactive properties whose watchers call
_update_display (which needs query_one), we use careful strategies:
  - For reactive defaults: inspect class-level descriptors.
  - For _update_display tests: check mock_widget.update was called and
    verify the *last* call content (since watchers trigger extra calls).
  - For watcher/on_mount tests: mock _update_display itself to count calls.
"""

import unittest
from unittest.mock import Mock, patch

from tui.widgets.header import ETXHeader


def _make_query_mocks():
    """Create three mock Static widgets and a query_one side effect."""
    mock_logo = Mock()
    mock_context = Mock()
    mock_status = Mock()

    def query_one_side_effect(selector, *args, **kwargs):
        if selector == "#etx-header-logo":
            return mock_logo
        if selector == "#etx-header-context":
            return mock_context
        if selector == "#etx-header-status":
            return mock_status
        return Mock()

    return mock_logo, mock_context, mock_status, query_one_side_effect


class TestETXHeaderInitialization(unittest.TestCase):
    """Tests for ETXHeader class attributes and initialization."""

    def test_widget_initialization(self):
        """Test ETXHeader can be instantiated."""
        header = ETXHeader()
        assert isinstance(header, ETXHeader)

    def test_widget_initialization_with_id(self):
        """Test ETXHeader can be instantiated with an id."""
        header = ETXHeader(id="header")
        assert header.id == "header"


class TestETXHeaderReactiveDefaults(unittest.TestCase):
    """Tests for reactive attribute default values.

    We check the class-level reactive descriptor defaults to avoid
    triggering watchers that require a mounted widget tree.
    """

    def test_default_project(self):
        """Test default project reactive value is 'euxis'."""
        descriptor = ETXHeader.__dict__["project"]
        assert descriptor._default == "euxis"

    def test_default_branch(self):
        """Test default branch reactive value is empty string."""
        descriptor = ETXHeader.__dict__["branch"]
        assert descriptor._default == ""

    def test_default_provider(self):
        """Test default provider reactive value is 'claude'."""
        descriptor = ETXHeader.__dict__["provider"]
        assert descriptor._default == "claude"

    def test_default_agent_count(self):
        """Test default agent_count reactive value is 50."""
        descriptor = ETXHeader.__dict__["agent_count"]
        assert descriptor._default == 50

    def test_default_version(self):
        """Test default version reactive value is '0.1.0'."""
        descriptor = ETXHeader.__dict__["version"]
        assert descriptor._default == "0.1.0"


class TestETXHeaderCSS(unittest.TestCase):
    """Tests for the DEFAULT_CSS."""

    def test_css_is_defined(self):
        """Test DEFAULT_CSS is a non-empty string."""
        assert isinstance(ETXHeader.DEFAULT_CSS, str)
        assert len(ETXHeader.DEFAULT_CSS) > 0

    def test_css_contains_etxheader_selector(self):
        """Test CSS targets ETXHeader widget."""
        assert "ETXHeader" in ETXHeader.DEFAULT_CSS

    def test_css_docks_to_top(self):
        """Test CSS docks header to top."""
        assert "dock: top" in ETXHeader.DEFAULT_CSS

    def test_css_sets_height(self):
        """Test CSS sets height to 3."""
        assert "height: 3" in ETXHeader.DEFAULT_CSS

    def test_css_horizontal_layout(self):
        """Test CSS uses horizontal layout."""
        assert "layout: horizontal" in ETXHeader.DEFAULT_CSS


class TestETXHeaderCompose(unittest.TestCase):
    """Tests for the compose() method."""

    @patch("tui.widgets.header.Static")
    def test_compose_yields_four_statics(self, mock_static):
        """Test compose() yields four Static widgets (logo, context, burn rate, status)."""
        header = ETXHeader()
        result = list(header.compose())
        assert len(result) == 4

    @patch("tui.widgets.header.Static")
    def test_compose_creates_logo_static(self, mock_static):
        """Test compose() creates a Static with id='etx-header-logo'."""
        header = ETXHeader()
        list(header.compose())
        ids = [call.kwargs.get("id") for call in mock_static.call_args_list]
        assert "etx-header-logo" in ids

    @patch("tui.widgets.header.Static")
    def test_compose_creates_context_static(self, mock_static):
        """Test compose() creates a Static with id='etx-header-context'."""
        header = ETXHeader()
        list(header.compose())
        ids = [call.kwargs.get("id") for call in mock_static.call_args_list]
        assert "etx-header-context" in ids

    @patch("tui.widgets.header.Static")
    def test_compose_creates_status_static(self, mock_static):
        """Test compose() creates a Static with id='etx-header-status'."""
        header = ETXHeader()
        list(header.compose())
        ids = [call.kwargs.get("id") for call in mock_static.call_args_list]
        assert "etx-header-status" in ids


class TestETXHeaderUpdateDisplay(unittest.TestCase):
    """Tests for the _update_display() method.

    After creating the header and installing query_one mocks, we check the
    most recent call to each mock widget's update() method rather than
    asserting exact call count (watchers cause additional calls).
    """

    def _create_header_with_mocks(self):
        """Create an ETXHeader with mocked query_one."""
        header = ETXHeader()
        mock_logo, mock_context, mock_status, side_effect = _make_query_mocks()
        header.query_one = Mock(side_effect=side_effect)
        # Clear all prior calls so we can track new ones
        mock_logo.reset_mock()
        mock_context.reset_mock()
        mock_status.reset_mock()
        return header, mock_logo, mock_context, mock_status

    def test_update_display_sets_logo(self):
        """Test _update_display sets logo with EUXIS branding and version."""
        header, mock_logo, _, _ = self._create_header_with_mocks()
        header._update_display()
        assert mock_logo.update.called
        text = mock_logo.update.call_args[0][0]
        assert "EUXIS" in text
        assert "0.1.0" in text

    def test_update_display_context_contains_project(self):
        """Test _update_display includes project name in context."""
        header, _, mock_context, _ = self._create_header_with_mocks()
        header._update_display()
        assert mock_context.update.called
        text = mock_context.update.call_args[0][0]
        assert "euxis" in text

    def test_update_display_context_without_branch(self):
        """Test _update_display omits branch when empty."""
        header, _, mock_context, _ = self._create_header_with_mocks()
        header._update_display()
        text = mock_context.update.call_args[0][0]
        # No branch means no "on" keyword in plain text
        stripped = text.replace("[bold]", "").replace("[/]", "").replace("[dim]", "")
        assert " on " not in stripped

    def test_update_display_context_with_branch(self):
        """Test _update_display includes branch when set."""
        header, _, mock_context, _ = self._create_header_with_mocks()
        header.branch = "feature/test"
        # watcher already called _update_display, check last call
        text = mock_context.update.call_args[0][0]
        assert "feature/test" in text
        assert "on" in text

    def test_update_display_logo_contains_version(self):
        """Test _update_display includes version in logo."""
        header, mock_logo, _, _ = self._create_header_with_mocks()
        header._update_display()
        text = mock_logo.update.call_args[0][0]
        assert "0.1.0" in text

    def test_update_display_status_contains_agent_count(self):
        """Test _update_display includes agent count in status."""
        header, _, _, mock_status = self._create_header_with_mocks()
        header._update_display()
        text = mock_status.update.call_args[0][0]
        assert "50" in text
        assert "agents" in text

    def test_update_display_status_contains_provider(self):
        """Test _update_display includes provider in status."""
        header, _, _, mock_status = self._create_header_with_mocks()
        header._update_display()
        text = mock_status.update.call_args[0][0]
        assert "claude" in text

    def test_update_display_with_custom_values(self):
        """Test _update_display uses current reactive values."""
        header, mock_logo, mock_context, mock_status = self._create_header_with_mocks()
        header.project = "my-project"
        header.branch = "main"
        header.provider = "gemini"
        # agent_count and version have no watch_ methods, so set them
        # before calling _update_display explicitly
        header.agent_count = 10
        header.version = "3.0.0"
        mock_logo.reset_mock()
        mock_context.reset_mock()
        mock_status.reset_mock()
        header._update_display()

        logo_text = mock_logo.update.call_args[0][0]
        assert "3.0.0" in logo_text

        context_text = mock_context.update.call_args[0][0]
        assert "my-project" in context_text
        assert "main" in context_text

        status_text = mock_status.update.call_args[0][0]
        assert "gemini" in status_text
        assert "10" in status_text


class TestETXHeaderWatchers(unittest.TestCase):
    """Tests for the watcher methods that trigger _update_display.

    We mock _update_display itself to verify watchers invoke it.
    """

    def test_watch_project_triggers_update(self):
        """Test watch_project calls _update_display."""
        header = ETXHeader()
        with patch.object(header, "_update_display") as mock_update:
            header.watch_project()
            mock_update.assert_called_once()

    def test_watch_branch_triggers_update(self):
        """Test watch_branch calls _update_display."""
        header = ETXHeader()
        with patch.object(header, "_update_display") as mock_update:
            header.watch_branch()
            mock_update.assert_called_once()

    def test_watch_provider_triggers_update(self):
        """Test watch_provider calls _update_display."""
        header = ETXHeader()
        with patch.object(header, "_update_display") as mock_update:
            header.watch_provider()
            mock_update.assert_called_once()


class TestETXHeaderOnMount(unittest.TestCase):
    """Tests for the on_mount() lifecycle method."""

    def test_on_mount_triggers_update_display(self):
        """Test on_mount calls _update_display."""
        header = ETXHeader()
        with patch.object(header, "_update_display") as mock_update:
            header.on_mount()
            mock_update.assert_called_once()


if __name__ == "__main__":
    unittest.main()
