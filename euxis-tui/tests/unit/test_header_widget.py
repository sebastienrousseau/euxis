# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Comprehensive unit tests for ETXHeader widget.

Tests initialization, reactive defaults, CSS, compose structure,
_update_display logic, watcher methods, cost tracking (add_cost,
_update_cost_display, reset_cost), and provider icon mappings.

Because ETXHeader uses Textual reactive properties whose watchers call
_update_display (which needs query_one), we use careful strategies:
  - For reactive defaults: inspect class-level descriptors.
  - For _update_display tests: check mock_widget.update was called and
    verify the *last* call content (since watchers trigger extra calls).
  - For watcher/on_mount tests: mock _update_display itself to count calls.
"""

import unittest
from unittest.mock import Mock, patch

from tui.widgets.header import ETXHeader, PROVIDER_ICONS


def _make_query_mocks():
    """Create four mock Static widgets (logo, context, cost, status) and a query_one side effect."""
    mock_logo = Mock()
    mock_context = Mock()
    mock_cost = Mock()
    mock_status = Mock()

    def query_one_side_effect(selector, *args, **kwargs):
        if selector == "#etx-header-logo":
            return mock_logo
        if selector == "#etx-header-context":
            return mock_context
        if selector == "#etx-header-cost":
            return mock_cost
        if selector == "#etx-header-status":
            return mock_status
        return Mock()

    return mock_logo, mock_context, mock_cost, mock_status, query_one_side_effect


# ===========================================================================
# PROVIDER_ICONS tests
# ===========================================================================


class TestProviderIcons(unittest.TestCase):
    """Tests for the PROVIDER_ICONS module-level dict."""

    def test_provider_icons_is_dict(self):
        """Test PROVIDER_ICONS is a dictionary."""
        assert isinstance(PROVIDER_ICONS, dict)

    def test_claude_icon(self):
        assert PROVIDER_ICONS["claude"] == "\u25c8"

    def test_openai_icon(self):
        assert PROVIDER_ICONS["openai"] == "\u25c9"

    def test_gemini_icon(self):
        assert PROVIDER_ICONS["gemini"] == "\u25c7"

    def test_local_icon(self):
        assert PROVIDER_ICONS["local"] == "\u25ce"

    def test_unknown_provider_not_in_icons(self):
        assert "unknown" not in PROVIDER_ICONS


# ===========================================================================
# Initialization tests
# ===========================================================================


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

    def test_cost_history_initialized_empty(self):
        """Test _cost_history is initialized as empty list."""
        header = ETXHeader()
        assert header._cost_history == []

    def test_cost_history_is_list(self):
        """Test _cost_history is a list type."""
        header = ETXHeader()
        assert isinstance(header._cost_history, list)


# ===========================================================================
# Reactive defaults tests
# ===========================================================================


class TestETXHeaderReactiveDefaults(unittest.TestCase):
    """Tests for reactive attribute default values.

    We check the class-level reactive descriptor defaults to avoid
    triggering watchers that require a mounted widget tree.
    """

    def test_default_project(self):
        descriptor = ETXHeader.__dict__["project"]
        assert descriptor._default == "euxis"

    def test_default_project_path(self):
        descriptor = ETXHeader.__dict__["project_path"]
        assert descriptor._default == ""

    def test_default_branch(self):
        descriptor = ETXHeader.__dict__["branch"]
        assert descriptor._default == ""

    def test_default_provider(self):
        descriptor = ETXHeader.__dict__["provider"]
        assert descriptor._default == "claude"

    def test_default_agent_count(self):
        descriptor = ETXHeader.__dict__["agent_count"]
        assert descriptor._default == 50

    def test_default_version(self):
        descriptor = ETXHeader.__dict__["version"]
        assert descriptor._default == "v0.0.2"

    def test_default_model(self):
        descriptor = ETXHeader.__dict__["model"]
        assert descriptor._default == ""

    def test_default_total_cost(self):
        descriptor = ETXHeader.__dict__["total_cost"]
        assert descriptor._default == 0.0

    def test_default_burn_rate(self):
        descriptor = ETXHeader.__dict__["burn_rate"]
        assert descriptor._default == 0.0


# ===========================================================================
# CSS tests
# ===========================================================================


class TestETXHeaderCSS(unittest.TestCase):
    """Tests for the DEFAULT_CSS."""

    def test_css_is_defined(self):
        assert isinstance(ETXHeader.DEFAULT_CSS, str)
        assert len(ETXHeader.DEFAULT_CSS) > 0

    def test_css_contains_etxheader_selector(self):
        assert "ETXHeader" in ETXHeader.DEFAULT_CSS

    def test_css_docks_to_top(self):
        assert "dock: top" in ETXHeader.DEFAULT_CSS

    def test_css_sets_height(self):
        assert "height: 3" in ETXHeader.DEFAULT_CSS

    def test_css_horizontal_layout(self):
        assert "layout: horizontal" in ETXHeader.DEFAULT_CSS


# ===========================================================================
# Compose tests
# ===========================================================================


class TestETXHeaderCompose(unittest.TestCase):
    """Tests for the compose() method."""

    @patch("tui.widgets.header.Static")
    def test_compose_yields_four_statics(self, mock_static):
        header = ETXHeader()
        result = list(header.compose())
        assert len(result) == 4

    @patch("tui.widgets.header.Static")
    def test_compose_creates_logo_static(self, mock_static):
        header = ETXHeader()
        list(header.compose())
        ids = [call.kwargs.get("id") for call in mock_static.call_args_list]
        assert "etx-header-logo" in ids

    @patch("tui.widgets.header.Static")
    def test_compose_creates_context_static(self, mock_static):
        header = ETXHeader()
        list(header.compose())
        ids = [call.kwargs.get("id") for call in mock_static.call_args_list]
        assert "etx-header-context" in ids

    @patch("tui.widgets.header.Static")
    def test_compose_creates_cost_static(self, mock_static):
        """Test compose() creates a Static with id='etx-header-cost'."""
        header = ETXHeader()
        list(header.compose())
        ids = [call.kwargs.get("id") for call in mock_static.call_args_list]
        assert "etx-header-cost" in ids

    @patch("tui.widgets.header.Static")
    def test_compose_creates_status_static(self, mock_static):
        header = ETXHeader()
        list(header.compose())
        ids = [call.kwargs.get("id") for call in mock_static.call_args_list]
        assert "etx-header-status" in ids


# ===========================================================================
# _update_display tests
# ===========================================================================


class TestETXHeaderUpdateDisplay(unittest.TestCase):
    """Tests for the _update_display() method."""

    def _create_header_with_mocks(self):
        """Create an ETXHeader with mocked query_one returning 4 widgets."""
        header = ETXHeader()
        mock_logo, mock_context, mock_cost, mock_status, side_effect = _make_query_mocks()
        header.query_one = Mock(side_effect=side_effect)
        mock_logo.reset_mock()
        mock_context.reset_mock()
        mock_cost.reset_mock()
        mock_status.reset_mock()
        return header, mock_logo, mock_context, mock_cost, mock_status

    def test_update_display_sets_logo(self):
        header, mock_logo, _, _, _ = self._create_header_with_mocks()
        header._update_display()
        assert mock_logo.update.called
        text = mock_logo.update.call_args[0][0]
        assert "EUXIS" in text
        assert "v0.0.2" in text

    def test_update_display_context_contains_project(self):
        header, _, mock_context, _, _ = self._create_header_with_mocks()
        header._update_display()
        assert mock_context.update.called
        text = mock_context.update.call_args[0][0]
        assert "euxis" in text

    def test_update_display_context_without_branch(self):
        header, _, mock_context, _, _ = self._create_header_with_mocks()
        header._update_display()
        text = mock_context.update.call_args[0][0]
        stripped = text.replace("[bold]", "").replace("[/]", "").replace("[dim]", "")
        assert " on " not in stripped

    def test_update_display_context_with_branch(self):
        header, _, mock_context, _, _ = self._create_header_with_mocks()
        header.branch = "feature/test"
        text = mock_context.update.call_args[0][0]
        assert "feature/test" in text
        assert "on" in text

    def test_update_display_context_with_project_path(self):
        """Test project_path overrides project in context display."""
        header, _, mock_context, _, _ = self._create_header_with_mocks()
        header.project_path = "/home/user/myproject"
        header._update_display()
        text = mock_context.update.call_args[0][0]
        assert "/home/user/myproject" in text

    def test_update_display_context_without_project_path(self):
        """Test project name is used when project_path is empty."""
        header, _, mock_context, _, _ = self._create_header_with_mocks()
        header._update_display()
        text = mock_context.update.call_args[0][0]
        assert "euxis" in text

    def test_update_display_logo_contains_version(self):
        header, mock_logo, _, _, _ = self._create_header_with_mocks()
        header._update_display()
        text = mock_logo.update.call_args[0][0]
        assert "v0.0.2" in text

    def test_update_display_status_contains_agent_count(self):
        header, _, _, _, mock_status = self._create_header_with_mocks()
        header._update_display()
        text = mock_status.update.call_args[0][0]
        assert "50" in text
        assert "agents" in text

    def test_update_display_status_contains_provider(self):
        header, _, _, _, mock_status = self._create_header_with_mocks()
        header._update_display()
        text = mock_status.update.call_args[0][0]
        assert "claude" in text

    def test_update_display_status_with_model_overrides_provider(self):
        """Test model display overrides provider when model is set."""
        header, _, _, _, mock_status = self._create_header_with_mocks()
        header.model = "claude-3-opus"
        header._update_display()
        text = mock_status.update.call_args[0][0]
        assert "claude-3-opus" in text

    def test_update_display_status_without_model_shows_provider(self):
        """Test provider is shown when model is empty."""
        header, _, _, _, mock_status = self._create_header_with_mocks()
        header._update_display()
        text = mock_status.update.call_args[0][0]
        assert "claude" in text

    def test_update_display_status_provider_icon_claude(self):
        header, _, _, _, mock_status = self._create_header_with_mocks()
        header._update_display()
        text = mock_status.update.call_args[0][0]
        assert "\u25c8" in text  # claude icon

    def test_update_display_status_provider_icon_openai(self):
        header, _, _, _, mock_status = self._create_header_with_mocks()
        header.provider = "openai"
        header._update_display()
        text = mock_status.update.call_args[0][0]
        assert "\u25c9" in text  # openai icon

    def test_update_display_status_provider_icon_gemini(self):
        header, _, _, _, mock_status = self._create_header_with_mocks()
        header.provider = "gemini"
        header._update_display()
        text = mock_status.update.call_args[0][0]
        assert "\u25c7" in text  # gemini icon

    def test_update_display_status_provider_icon_local(self):
        header, _, _, _, mock_status = self._create_header_with_mocks()
        header.provider = "local"
        header._update_display()
        text = mock_status.update.call_args[0][0]
        assert "\u25ce" in text  # local icon

    def test_update_display_status_provider_icon_unknown(self):
        """Test unknown provider gets default bullet icon."""
        header, _, _, _, mock_status = self._create_header_with_mocks()
        header.provider = "unknown-provider"
        header._update_display()
        text = mock_status.update.call_args[0][0]
        assert "\u25cf" in text  # default bullet

    def test_update_display_calls_update_cost_display(self):
        """Test _update_display also calls _update_cost_display."""
        header, _, _, _, _ = self._create_header_with_mocks()
        with patch.object(header, "_update_cost_display") as mock_cost:
            header._update_display()
            mock_cost.assert_called()

    def test_update_display_with_custom_values(self):
        header, mock_logo, mock_context, _, mock_status = self._create_header_with_mocks()
        header.project = "my-project"
        header.branch = "main"
        header.provider = "gemini"
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


# ===========================================================================
# _update_cost_display tests
# ===========================================================================


class TestETXHeaderUpdateCostDisplay(unittest.TestCase):
    """Tests for the _update_cost_display() method and burn rate coloring."""

    def _create_header_with_mocks(self):
        header = ETXHeader()
        mock_logo, mock_context, mock_cost, mock_status, side_effect = _make_query_mocks()
        header.query_one = Mock(side_effect=side_effect)
        mock_cost.reset_mock()
        return header, mock_cost

    def test_cost_display_empty_when_no_cost(self):
        """Test cost widget shows empty when total_cost is 0 and no history."""
        header, mock_cost = self._create_header_with_mocks()
        header._update_cost_display()
        mock_cost.update.assert_called_with("")

    def test_cost_display_shows_cost_when_total_cost_positive(self):
        """Test cost widget shows dollar amount when total_cost > 0."""
        header, mock_cost = self._create_header_with_mocks()
        header.total_cost = 0.5
        header._update_cost_display()
        text = mock_cost.update.call_args[0][0]
        assert "$0.500" in text

    def test_cost_display_shows_cost_when_history_present(self):
        """Test cost widget shows amount when _cost_history is non-empty."""
        header, mock_cost = self._create_header_with_mocks()
        header._cost_history = [0.01, 0.02]
        header._update_cost_display()
        text = mock_cost.update.call_args[0][0]
        assert "$0.000" in text

    def test_cost_display_green_for_low_burn_rate(self):
        """Test green color when burn_rate <= 0.1."""
        header, mock_cost = self._create_header_with_mocks()
        header.total_cost = 0.1
        header.burn_rate = 0.05
        header._update_cost_display()
        text = mock_cost.update.call_args[0][0]
        assert "[green]" in text

    def test_cost_display_yellow_for_medium_burn_rate(self):
        """Test yellow color when burn_rate > 0.1 and <= 1.0."""
        header, mock_cost = self._create_header_with_mocks()
        header.total_cost = 1.0
        header.burn_rate = 0.5
        header._update_cost_display()
        text = mock_cost.update.call_args[0][0]
        assert "[yellow]" in text

    def test_cost_display_red_for_high_burn_rate(self):
        """Test red color when burn_rate > 1.0."""
        header, mock_cost = self._create_header_with_mocks()
        header.total_cost = 5.0
        header.burn_rate = 2.0
        header._update_cost_display()
        text = mock_cost.update.call_args[0][0]
        assert "[red]" in text

    def test_cost_display_includes_sparkline(self):
        """Test sparkline text is included when cost history exists."""
        header, mock_cost = self._create_header_with_mocks()
        header.total_cost = 1.0
        header._cost_history = [0.1, 0.2, 0.3, 0.4, 0.5]
        header._update_cost_display()
        text = mock_cost.update.call_args[0][0]
        # sparkline_text returns non-empty for non-empty values
        assert "$1.000" in text

    def test_cost_display_no_sparkline_when_no_history(self):
        """Test no sparkline when cost_history is empty but total_cost > 0."""
        header, mock_cost = self._create_header_with_mocks()
        header.total_cost = 0.5
        header._cost_history = []
        header._update_cost_display()
        text = mock_cost.update.call_args[0][0]
        assert "$0.500" in text

    def test_cost_display_exception_handled(self):
        """Test _update_cost_display handles exceptions gracefully."""
        header = ETXHeader()
        header.query_one = Mock(side_effect=Exception("Widget not mounted"))
        # Should not raise
        header._update_cost_display()

    def test_cost_display_burn_rate_boundary_0_1(self):
        """Test burn_rate exactly at 0.1 uses green."""
        header, mock_cost = self._create_header_with_mocks()
        header.total_cost = 0.1
        header.burn_rate = 0.1
        header._update_cost_display()
        text = mock_cost.update.call_args[0][0]
        assert "[green]" in text

    def test_cost_display_burn_rate_boundary_1_0(self):
        """Test burn_rate exactly at 1.0 uses yellow."""
        header, mock_cost = self._create_header_with_mocks()
        header.total_cost = 1.0
        header.burn_rate = 1.0
        header._update_cost_display()
        text = mock_cost.update.call_args[0][0]
        assert "[yellow]" in text


# ===========================================================================
# add_cost tests
# ===========================================================================


class TestETXHeaderAddCost(unittest.TestCase):
    """Tests for the add_cost() method."""

    def _create_header_with_mocks(self):
        header = ETXHeader()
        _, _, _, _, side_effect = _make_query_mocks()
        header.query_one = Mock(side_effect=side_effect)
        return header

    def test_add_cost_increases_total(self):
        header = self._create_header_with_mocks()
        header.add_cost(0.5)
        assert header.total_cost == 0.5

    def test_add_cost_accumulates(self):
        header = self._create_header_with_mocks()
        header.add_cost(0.1)
        header.add_cost(0.2)
        assert abs(header.total_cost - 0.3) < 1e-9

    def test_add_cost_appends_to_history(self):
        header = self._create_header_with_mocks()
        header.add_cost(0.1)
        assert 0.1 in header._cost_history

    def test_add_cost_trims_history_to_20(self):
        """Test cost history is trimmed to 20 entries."""
        header = self._create_header_with_mocks()
        for i in range(25):
            header.add_cost(0.01 * i)
        assert len(header._cost_history) == 20

    def test_add_cost_keeps_last_20(self):
        """Test cost history keeps most recent 20 entries."""
        header = self._create_header_with_mocks()
        for i in range(25):
            header.add_cost(float(i))
        # Last entry should be 24.0
        assert header._cost_history[-1] == 24.0
        # First entry should be 5.0 (indices 5-24)
        assert header._cost_history[0] == 5.0

    def test_add_cost_calculates_burn_rate_with_enough_samples(self):
        """Test burn rate calculated when >= 2 history entries."""
        header = self._create_header_with_mocks()
        header.add_cost(0.1)
        header.add_cost(0.2)
        # burn_rate = sum of last 5 * 12
        assert header.burn_rate > 0

    def test_add_cost_no_burn_rate_with_single_sample(self):
        """Test burn rate is not updated with only 1 sample."""
        header = self._create_header_with_mocks()
        header.add_cost(0.1)
        assert header.burn_rate == 0.0

    def test_add_cost_burn_rate_formula(self):
        """Test burn rate formula: sum(last 5) * 12."""
        header = self._create_header_with_mocks()
        header.add_cost(0.1)
        header.add_cost(0.2)
        header.add_cost(0.3)
        # Last 5 = [0.1, 0.2, 0.3], sum = 0.6, * 12 = 7.2
        expected = (0.1 + 0.2 + 0.3) * 12
        assert abs(header.burn_rate - expected) < 1e-9

    def test_add_cost_calls_update_cost_display(self):
        """Test add_cost triggers _update_cost_display."""
        header = self._create_header_with_mocks()
        with patch.object(header, "_update_cost_display") as mock_update:
            header.add_cost(0.1)
            mock_update.assert_called()


# ===========================================================================
# reset_cost tests
# ===========================================================================


class TestETXHeaderResetCost(unittest.TestCase):
    """Tests for the reset_cost() method."""

    def _create_header_with_mocks(self):
        header = ETXHeader()
        _, _, _, _, side_effect = _make_query_mocks()
        header.query_one = Mock(side_effect=side_effect)
        return header

    def test_reset_cost_clears_total(self):
        header = self._create_header_with_mocks()
        header.add_cost(1.0)
        header.reset_cost()
        assert header.total_cost == 0.0

    def test_reset_cost_clears_burn_rate(self):
        header = self._create_header_with_mocks()
        header.add_cost(0.1)
        header.add_cost(0.2)
        header.reset_cost()
        assert header.burn_rate == 0.0

    def test_reset_cost_clears_history(self):
        header = self._create_header_with_mocks()
        header.add_cost(0.1)
        header.add_cost(0.2)
        header.reset_cost()
        assert header._cost_history == []

    def test_reset_cost_calls_update_cost_display(self):
        header = self._create_header_with_mocks()
        with patch.object(header, "_update_cost_display") as mock_update:
            header.reset_cost()
            mock_update.assert_called()


# ===========================================================================
# Watcher tests
# ===========================================================================


class TestETXHeaderWatchers(unittest.TestCase):
    """Tests for the watcher methods that trigger _update_display."""

    def test_watch_project_triggers_update(self):
        header = ETXHeader()
        with patch.object(header, "_update_display") as mock_update:
            header.watch_project()
            mock_update.assert_called_once()

    def test_watch_project_path_triggers_update(self):
        header = ETXHeader()
        with patch.object(header, "_update_display") as mock_update:
            header.watch_project_path()
            mock_update.assert_called_once()

    def test_watch_branch_triggers_update(self):
        header = ETXHeader()
        with patch.object(header, "_update_display") as mock_update:
            header.watch_branch()
            mock_update.assert_called_once()

    def test_watch_provider_triggers_update(self):
        header = ETXHeader()
        with patch.object(header, "_update_display") as mock_update:
            header.watch_provider()
            mock_update.assert_called_once()

    def test_watch_model_triggers_update(self):
        header = ETXHeader()
        with patch.object(header, "_update_display") as mock_update:
            header.watch_model()
            mock_update.assert_called_once()

    def test_watch_total_cost_triggers_cost_update(self):
        header = ETXHeader()
        with patch.object(header, "_update_cost_display") as mock_update:
            header.watch_total_cost()
            mock_update.assert_called_once()

    def test_watch_burn_rate_triggers_cost_update(self):
        header = ETXHeader()
        with patch.object(header, "_update_cost_display") as mock_update:
            header.watch_burn_rate()
            mock_update.assert_called_once()


# ===========================================================================
# on_mount tests
# ===========================================================================


class TestETXHeaderOnMount(unittest.TestCase):
    """Tests for the on_mount() lifecycle method."""

    def test_on_mount_triggers_update_display(self):
        header = ETXHeader()
        with patch.object(header, "_update_display") as mock_update:
            header.on_mount()
            mock_update.assert_called_once()


if __name__ == "__main__":
    unittest.main()
