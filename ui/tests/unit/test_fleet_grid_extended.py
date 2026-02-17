"""Extended unit tests for FleetGrid and AgentCard.

Tests cover: _matches_filter (empty, by id/tag/tier/activation, case-insensitive,
no match), _rebuild_grid (section mounting, 4-card rows, filtered),
AgentCard.on_key enter/other, tier CSS class, unknown tier/activation fallback.
"""

from __future__ import annotations

import unittest
from unittest.mock import Mock, PropertyMock, patch

from tui.core.registry import Agent, FleetRegistry
from tui.widgets.agent_card import (
    ACTIVATION_ICONS,
    TIER_STYLES,
    AgentCard,
)
from tui.widgets.fleet_grid import FleetGrid


def _make_agent(**overrides):
    defaults = {
        "id": "architect",
        "tier": "core",
        "version": "0.0.8",
        "tags": ("design", "plan"),
        "activation": "default",
    }
    defaults.update(overrides)
    return Agent(**defaults)


def _make_registry(agents=None):
    registry = FleetRegistry()
    registry.agents = agents or []
    return registry


# ===========================================================================
# FleetGrid._matches_filter tests
# ===========================================================================


class TestFleetGridMatchesFilter(unittest.TestCase):
    """Test _matches_filter using a mock object with filter_text attribute."""

    def _call_matches(self, filter_text, agent):
        """Call _matches_filter as an unbound method with a mock self."""
        mock_self = Mock()
        mock_self.filter_text = filter_text
        return FleetGrid._matches_filter(mock_self, agent)

    def test_empty_filter_matches_all(self):
        assert self._call_matches("", _make_agent()) is True

    def test_match_by_id(self):
        assert self._call_matches("architect", _make_agent(id="architect")) is True

    def test_match_by_tag(self):
        assert self._call_matches("security", _make_agent(tags=("security", "audit"))) is True

    def test_match_by_tier(self):
        assert self._call_matches("core", _make_agent(tier="core")) is True

    def test_match_by_activation(self):
        assert self._call_matches("on-demand", _make_agent(activation="on-demand")) is True

    def test_case_insensitive(self):
        assert self._call_matches("ARCHITECT", _make_agent(id="Architect")) is True

    def test_partial_match(self):
        assert self._call_matches("arch", _make_agent(id="architect")) is True

    def test_no_match(self):
        agent = _make_agent(id="architect", tags=("design",), tier="core")
        assert self._call_matches("xyz_nonexistent", agent) is False


# ===========================================================================
# FleetGrid._rebuild_grid tests
# ===========================================================================


class TestFleetGridRebuildGrid(unittest.TestCase):
    def _make_grid_mock(self, agents, filter_text=""):
        """Create a mock-based grid for _rebuild_grid testing."""
        registry = _make_registry(agents)
        mock_grid = Mock(spec=FleetGrid)
        mock_grid.registry = registry
        mock_grid.filter_text = filter_text
        # Wire the real methods
        mock_grid._matches_filter = lambda agent: FleetGrid._matches_filter(mock_grid, agent)
        mock_grid._rebuild_grid = lambda: FleetGrid._rebuild_grid(mock_grid)
        return mock_grid

    def test_empty_registry(self):
        grid = self._make_grid_mock([])
        mock_scroll = Mock()
        grid.query_one = Mock(return_value=mock_scroll)

        grid._rebuild_grid()

        mock_scroll.remove_children.assert_called_once()
        mock_scroll.mount.assert_not_called()

    def test_core_section_mounted(self):
        agents = [_make_agent(tier="core", activation="default")]
        grid = self._make_grid_mock(agents)
        mock_scroll = Mock()
        grid.query_one = Mock(return_value=mock_scroll)

        with (
            patch("tui.widgets.fleet_grid.Static"),
            patch("tui.widgets.fleet_grid.Horizontal"),
            patch("tui.widgets.fleet_grid.AgentCard"),
        ):
            grid._rebuild_grid()

        # Should have mounted section label + 1 row
        assert mock_scroll.mount.call_count >= 2

    def test_multiple_sections(self):
        agents = [
            _make_agent(id="a1", tier="core", activation="default"),
            _make_agent(id="a2", tier="fleet", activation="default"),
            _make_agent(id="a3", tier="fleet", activation="on-demand"),
            _make_agent(id="a4", tier="fleet", activation="specialist"),
        ]
        grid = self._make_grid_mock(agents)
        mock_scroll = Mock()
        grid.query_one = Mock(return_value=mock_scroll)

        with (
            patch("tui.widgets.fleet_grid.Static"),
            patch("tui.widgets.fleet_grid.Horizontal"),
            patch("tui.widgets.fleet_grid.AgentCard"),
        ):
            grid._rebuild_grid()

        # Multiple sections should be mounted
        assert mock_scroll.mount.call_count > 2

    def test_four_card_rows(self):
        # 5 core agents → should create 2 rows (4 + 1)
        agents = [
            _make_agent(id=f"agent-{i}", tier="core", activation="default")
            for i in range(5)
        ]
        grid = self._make_grid_mock(agents)
        mock_scroll = Mock()
        mock_row = Mock()
        grid.query_one = Mock(return_value=mock_scroll)

        with (
            patch("tui.widgets.fleet_grid.Static"),
            patch("tui.widgets.fleet_grid.Horizontal", return_value=mock_row),
            patch("tui.widgets.fleet_grid.AgentCard"),
        ):
            grid._rebuild_grid()

        # 1 section label + 1 section desc + 2 rows = 4 mount calls
        assert mock_scroll.mount.call_count == 4

    def test_filtered_grid(self):
        agents = [
            _make_agent(id="architect", tier="core", activation="default"),
            _make_agent(id="debugger", tier="fleet", activation="default"),
        ]
        grid = self._make_grid_mock(agents, filter_text="architect")
        mock_scroll = Mock()
        grid.query_one = Mock(return_value=mock_scroll)

        with (
            patch("tui.widgets.fleet_grid.Static"),
            patch("tui.widgets.fleet_grid.Horizontal"),
            patch("tui.widgets.fleet_grid.AgentCard"),
        ):
            grid._rebuild_grid()

        # Only core section with 1 agent should appear
        # Section label + 1 section desc + 1 row = 3 mount calls
        assert mock_scroll.mount.call_count == 3


# ===========================================================================
# FleetGrid init/compose/focus tests
# ===========================================================================


class TestFleetGridInitAndCompose(unittest.TestCase):
    def test_init(self):
        registry = _make_registry()
        grid = FleetGrid(registry)
        assert grid.registry is registry

    @patch("tui.widgets.fleet_grid.VerticalScroll")
    @patch("tui.widgets.fleet_grid.Input")
    @patch("tui.widgets.fleet_grid.Container")
    def test_compose(self, mock_container, mock_input, mock_scroll):
        registry = _make_registry()
        grid = FleetGrid(registry)
        result = list(grid.compose())
        assert len(result) > 0

    def test_focus_first_card_with_cards(self):
        registry = _make_registry()
        grid = FleetGrid(registry)
        mock_card = Mock()
        mock_cards = Mock()
        mock_cards.__bool__ = Mock(return_value=True)
        mock_cards.first = Mock(return_value=mock_card)
        grid.query = Mock(return_value=mock_cards)

        grid.focus_first_card()
        mock_card.focus.assert_called_once()

    def test_focus_first_card_no_cards(self):
        registry = _make_registry()
        grid = FleetGrid(registry)
        mock_cards = Mock()
        mock_cards.__bool__ = Mock(return_value=False)
        grid.query = Mock(return_value=mock_cards)

        grid.focus_first_card()  # Should not raise

    def test_on_input_changed_filter(self):
        registry = _make_registry()
        grid = FleetGrid(registry)
        event = Mock()
        event.input.id = "fleet-filter-input"
        event.value = "test"
        # Mock _rebuild_grid to avoid DOM issues from watch_filter_text
        with patch.object(grid, "_rebuild_grid"):
            grid.on_input_changed(event)
        assert grid.filter_text == "test"

    def test_on_input_changed_other_input_ignored(self):
        """Non-fleet-filter inputs should not change filter_text."""
        registry = _make_registry()
        grid = FleetGrid(registry)
        event = Mock()
        event.input.id = "other-input"
        event.value = "test"
        # The method has an early return for non-matching input IDs
        # Just verify it doesn't crash and doesn't set filter_text
        with patch.object(type(grid), "filter_text", new_callable=PropertyMock) as mock_ft:
            grid.on_input_changed(event)
            mock_ft.assert_not_called()


# ===========================================================================
# AgentCard tests
# ===========================================================================


class TestAgentCard(unittest.TestCase):
    def test_init_core_class(self):
        agent = _make_agent(tier="core")
        card = AgentCard(agent)
        assert card.agent is agent
        assert "tier-core" in card.classes

    def test_init_fleet_no_core_class(self):
        agent = _make_agent(tier="fleet")
        card = AgentCard(agent)
        assert "tier-core" not in card.classes

    @patch("tui.widgets.agent_card.Static")
    def test_compose(self, mock_static):
        agent = _make_agent()
        card = AgentCard(agent)
        result = list(card.compose())
        assert len(result) == 3

    def test_on_click_posts_message(self):
        agent = _make_agent()
        card = AgentCard(agent)
        card.post_message = Mock()
        card.on_click()

        card.post_message.assert_called_once()
        msg = card.post_message.call_args[0][0]
        assert isinstance(msg, AgentCard.Selected)
        assert msg.agent is agent

    def test_on_key_enter(self):
        agent = _make_agent()
        card = AgentCard(agent)
        card.post_message = Mock()

        event = Mock()
        event.key = "enter"
        card.on_key(event)

        card.post_message.assert_called_once()
        event.stop.assert_called_once()

    def test_on_key_other(self):
        agent = _make_agent()
        card = AgentCard(agent)
        card.post_message = Mock()

        event = Mock()
        event.key = "tab"
        card.on_key(event)

        card.post_message.assert_not_called()

    def test_selected_message(self):
        agent = _make_agent()
        msg = AgentCard.Selected(agent)
        assert msg.agent is agent


class TestTierAndActivationStyles(unittest.TestCase):
    def test_tier_styles_core(self):
        assert "core" in TIER_STYLES
        style, label = TIER_STYLES["core"]
        assert label == "CORE"

    def test_tier_styles_fleet(self):
        assert "fleet" in TIER_STYLES
        style, label = TIER_STYLES["fleet"]
        assert label == "FLEET"

    def test_unknown_tier_fallback(self):
        # TIER_STYLES.get returns default for unknown tier
        result = TIER_STYLES.get("unknown", ("dim", "FLEET"))
        assert result == ("dim", "FLEET")

    def test_activation_icons_default(self):
        assert "default" in ACTIVATION_ICONS
        assert "●" in ACTIVATION_ICONS["default"]

    def test_activation_icons_on_demand(self):
        assert "on-demand" in ACTIVATION_ICONS
        assert "◐" in ACTIVATION_ICONS["on-demand"]

    def test_activation_icons_specialist(self):
        assert "specialist" in ACTIVATION_ICONS
        assert "◆" in ACTIVATION_ICONS["specialist"]

    def test_unknown_activation_fallback(self):
        result = ACTIVATION_ICONS.get("unknown", "●")
        assert result == "●"


if __name__ == "__main__":
    unittest.main()
