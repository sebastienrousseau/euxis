# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Extended unit tests for FleetGrid widget — targeting >= 95% line coverage.

Covers: all message classes, _ClickableRow, compose/mount lifecycle,
on_input_submitted (all branches), _parse_search_command, _find_exact_entity,
_dispatch_selection, watch_filter_text/watch_view_mode, action_toggle_view,
_rebuild_grid/_rebuild_activation_view/_rebuild_hierarchy_view,
_mount_agent_cards (with active/error IDs), _get_active_agent_ids,
_get_error_agent_ids, _mount_agent_rows, _matches_squad, _matches_combo,
_mount_squads_and_combos, navigation actions, update_active_indicator,
action_goto_active, action_restart_all_failed, _dispatch_first_filtered,
and module-level constants.
"""

from __future__ import annotations

import json
import unittest
from unittest.mock import Mock, MagicMock, PropertyMock, call, mock_open, patch

from tui.core.registry import Agent, Combo, FleetRegistry, Squad
from tui.widgets.agent_card import (
    ACTIVATION_ICONS,
    STATUS_ACTIVE as AC_STATUS_ACTIVE,
    STATUS_ERROR as AC_STATUS_ERROR,
    SUPERVISION_TIERS,
    TIER_STYLES,
    AgentCard,
)
from tui.widgets.fleet_grid import (
    HIERARCHY_DESCRIPTIONS,
    MIN_QUOTED_LENGTH,
    SECTION_DESCRIPTIONS,
    STATUS_ACTIVE,
    STATUS_IDLE,
    SYSTEM_COMMANDS,
    AgentSelected,
    BulkRestartRequested,
    ComboSelected,
    FleetGrid,
    SquadSelected,
    SystemCommandRequested,
    _ClickableRow,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_agent(**overrides):
    defaults = {
        "id": "architect",
        "tier": "core",
        "version": "v0.0.2",
        "tags": ["design", "plan"],
        "activation": "default",
        "capability_tags": [],
    }
    defaults.update(overrides)
    return Agent(**defaults)


def _make_squad(**overrides):
    defaults = {
        "id": "alpha",
        "name": "Alpha Squad",
        "purpose": "Main engineering",
        "lead": "architect",
        "members": ("architect", "debugger"),
    }
    defaults.update(overrides)
    return Squad(**defaults)


def _make_combo(**overrides):
    defaults = {
        "id": "envision",
        "name": "Envision Combo",
        "description": "Full analysis pipeline",
        "chain": ("analyst", "architect", "documenter"),
    }
    defaults.update(overrides)
    return Combo(**defaults)


def _make_registry(agents=None, squads=None, combos=None):
    registry = FleetRegistry()
    registry.agents = agents or []
    registry.squads = squads or []
    registry.combos = combos or []
    registry._rebuild_index()
    return registry


def _make_fleet_grid(agents=None, squads=None, combos=None):
    registry = _make_registry(agents or [], squads or [], combos or [])
    grid = FleetGrid(registry)
    return grid


# ===========================================================================
# Module-level constants
# ===========================================================================



class TestClickableRow(unittest.TestCase):
    def test_init_stores_message(self):
        inner_msg = AgentSelected("architect")
        row = _ClickableRow("label text", inner_msg)
        assert row._message is inner_msg

    def test_on_click_posts_message(self):
        inner_msg = AgentSelected("architect")
        row = _ClickableRow("label text", inner_msg)
        row.post_message = Mock()

        row.on_click()

        row.post_message.assert_called_once_with(inner_msg)

    def test_action_select_posts_message(self):
        inner_msg = SquadSelected("alpha")
        row = _ClickableRow("label text", inner_msg)
        row.post_message = Mock()

        row.action_select()

        row.post_message.assert_called_once_with(inner_msg)

    def test_bindings(self):
        assert any(b[0] == "enter" for b in _ClickableRow.BINDINGS)


# ===========================================================================
# FleetGrid.__init__
# ===========================================================================

class TestRebuildGrid(unittest.TestCase):
    def _make_grid_mock(self, agents=None, squads=None, combos=None, filter_text="", view_mode="activation"):
        registry = _make_registry(agents or [], squads or [], combos or [])
        mock_grid = Mock(spec=FleetGrid)
        mock_grid.registry = registry
        mock_grid.filter_text = filter_text
        mock_grid._rebuilding = False
        mock_grid.view_mode = view_mode
        mock_grid._CARDS_PER_ROW = 4
        mock_grid._get_active_agent_ids = Mock(return_value=set())
        mock_grid._get_error_agent_ids = Mock(return_value=set())
        mock_grid._mount_squads_and_combos = Mock()
        mock_grid.call_later = Mock()
        mock_grid.update_active_indicator = Mock()
        mock_grid._matches_filter = lambda agent: FleetGrid._matches_filter(mock_grid, agent)
        mock_grid._rebuild_grid = lambda: FleetGrid._rebuild_grid(mock_grid)
        mock_grid._rebuild_activation_view = lambda scroll, active, error: FleetGrid._rebuild_activation_view(mock_grid, scroll, active, error)
        mock_grid._rebuild_hierarchy_view = lambda scroll, active, error: FleetGrid._rebuild_hierarchy_view(mock_grid, scroll, active, error)
        mock_grid._mount_agent_cards = lambda scroll, agents_list, active, error: FleetGrid._mount_agent_cards(mock_grid, scroll, agents_list, active, error)
        return mock_grid

    def test_guard_prevents_concurrent_rebuild(self):
        grid = self._make_grid_mock()
        grid._rebuilding = True
        mock_scroll = Mock()
        grid.query_one = Mock(return_value=mock_scroll)

        FleetGrid._rebuild_grid(grid)

        mock_scroll.remove_children.assert_not_called()

    def test_rebuilding_flag_reset_after_rebuild(self):
        grid = self._make_grid_mock()
        mock_scroll = Mock()
        grid.query_one = Mock(return_value=mock_scroll)

        with patch("tui.widgets.fleet_grid.Static"), \
             patch("tui.widgets.fleet_grid.Horizontal"), \
             patch("tui.widgets.fleet_grid.AgentCard"):
            grid._rebuild_grid()

        assert grid._rebuilding is False

    def test_rebuilding_flag_reset_on_exception(self):
        """Ensure _rebuilding is reset even if an exception occurs during rebuild."""
        grid = self._make_grid_mock()
        grid.query_one = Mock(side_effect=Exception("test"))

        try:
            grid._rebuild_grid()
        except Exception:
            pass

        assert grid._rebuilding is False

    def test_activation_view_mode(self):
        agents = [_make_agent(id="a1", tier="core")]
        grid = self._make_grid_mock(agents=agents, view_mode="activation")
        mock_scroll = Mock()
        grid.query_one = Mock(return_value=mock_scroll)

        with patch("tui.widgets.fleet_grid.Static"), \
             patch("tui.widgets.fleet_grid.Horizontal"), \
             patch("tui.widgets.fleet_grid.AgentCard"):
            grid._rebuild_grid()

        # Should have mounted section elements
        assert mock_scroll.mount.call_count >= 2

    def test_hierarchy_view_mode(self):
        agents = [_make_agent(id="arbiter", tier="core")]
        grid = self._make_grid_mock(agents=agents, view_mode="hierarchy")
        mock_scroll = Mock()
        grid.query_one = Mock(return_value=mock_scroll)

        with patch("tui.widgets.fleet_grid.Static"), \
             patch("tui.widgets.fleet_grid.Horizontal"), \
             patch("tui.widgets.fleet_grid.AgentCard"):
            grid._rebuild_grid()

        # Should have view indicator + section elements
        assert mock_scroll.mount.call_count >= 1

    def test_calls_mount_squads_and_combos(self):
        grid = self._make_grid_mock()
        mock_scroll = Mock()
        grid.query_one = Mock(return_value=mock_scroll)

        with patch("tui.widgets.fleet_grid.Static"), \
             patch("tui.widgets.fleet_grid.Horizontal"), \
             patch("tui.widgets.fleet_grid.AgentCard"):
            grid._rebuild_grid()

        grid._mount_squads_and_combos.assert_called_once_with(mock_scroll)

    def test_calls_update_active_indicator(self):
        grid = self._make_grid_mock()
        mock_scroll = Mock()
        grid.query_one = Mock(return_value=mock_scroll)

        with patch("tui.widgets.fleet_grid.Static"), \
             patch("tui.widgets.fleet_grid.Horizontal"), \
             patch("tui.widgets.fleet_grid.AgentCard"):
            grid._rebuild_grid()

        grid.call_later.assert_called_once_with(grid.update_active_indicator)


# ===========================================================================
# FleetGrid._rebuild_activation_view
# ===========================================================================

class TestRebuildActivationView(unittest.TestCase):
    def test_core_section(self):
        agents = [_make_agent(id="a1", tier="core", activation="default")]
        registry = _make_registry(agents)
        grid = Mock(spec=FleetGrid)
        grid.registry = registry
        grid.filter_text = ""
        grid._CARDS_PER_ROW = 4
        grid._matches_filter = lambda a: FleetGrid._matches_filter(grid, a)
        grid._mount_agent_cards = Mock()

        scroll = Mock()
        with patch("tui.widgets.fleet_grid.Static"):
            FleetGrid._rebuild_activation_view(grid, scroll, set(), set())

        # section label + section desc + mount_agent_cards
        grid._mount_agent_cards.assert_called_once()

    def test_all_four_sections(self):
        agents = [
            _make_agent(id="a1", tier="core", activation="default"),
            _make_agent(id="a2", tier="fleet", activation="default"),
            _make_agent(id="a3", tier="fleet", activation="on-demand"),
            _make_agent(id="a4", tier="fleet", activation="specialist"),
        ]
        registry = _make_registry(agents)
        grid = Mock(spec=FleetGrid)
        grid.registry = registry
        grid.filter_text = ""
        grid._CARDS_PER_ROW = 4
        grid._matches_filter = lambda a: FleetGrid._matches_filter(grid, a)
        grid._mount_agent_cards = Mock()

        scroll = Mock()
        with patch("tui.widgets.fleet_grid.Static"):
            FleetGrid._rebuild_activation_view(grid, scroll, set(), set())

        assert grid._mount_agent_cards.call_count == 4

    def test_empty_section_skipped(self):
        agents = [_make_agent(id="a1", tier="fleet", activation="default")]
        registry = _make_registry(agents)
        grid = Mock(spec=FleetGrid)
        grid.registry = registry
        grid.filter_text = ""
        grid._CARDS_PER_ROW = 4
        grid._matches_filter = lambda a: FleetGrid._matches_filter(grid, a)
        grid._mount_agent_cards = Mock()

        scroll = Mock()
        with patch("tui.widgets.fleet_grid.Static"):
            FleetGrid._rebuild_activation_view(grid, scroll, set(), set())

        # Only default section has agents
        grid._mount_agent_cards.assert_called_once()

    def test_filter_removes_agents(self):
        agents = [
            _make_agent(id="architect", tier="core", activation="default"),
            _make_agent(id="debugger", tier="fleet", activation="default"),
        ]
        registry = _make_registry(agents)
        grid = Mock(spec=FleetGrid)
        grid.registry = registry
        grid.filter_text = "architect"
        grid._CARDS_PER_ROW = 4
        grid._matches_filter = lambda a: FleetGrid._matches_filter(grid, a)
        grid._mount_agent_cards = Mock()

        scroll = Mock()
        with patch("tui.widgets.fleet_grid.Static"):
            FleetGrid._rebuild_activation_view(grid, scroll, set(), set())

        # Only core section should have match
        grid._mount_agent_cards.assert_called_once()


# ===========================================================================
# FleetGrid._rebuild_hierarchy_view
# ===========================================================================

class TestRebuildHierarchyView(unittest.TestCase):
    def _make_mock_grid(self, agents, filter_text=""):
        registry = _make_registry(agents)
        grid = Mock(spec=FleetGrid)
        grid.registry = registry
        grid.filter_text = filter_text
        grid._CARDS_PER_ROW = 4
        grid._matches_filter = lambda a: FleetGrid._matches_filter(grid, a)
        grid._mount_agent_cards = Mock()
        return grid

    def test_supervisors_section(self):
        agents = [_make_agent(id="arbiter", tier="core")]
        grid = self._make_mock_grid(agents)
        scroll = Mock()

        with patch("tui.widgets.fleet_grid.Static"):
            FleetGrid._rebuild_hierarchy_view(grid, scroll, set(), set())

        # View indicator + supervisor label + desc + mount cards
        grid._mount_agent_cards.assert_called_once()

    def test_coordinators_section(self):
        agents = [_make_agent(id="architect", tier="core")]
        grid = self._make_mock_grid(agents)
        scroll = Mock()

        with patch("tui.widgets.fleet_grid.Static"):
            FleetGrid._rebuild_hierarchy_view(grid, scroll, set(), set())

        grid._mount_agent_cards.assert_called_once()

    def test_specialists_section(self):
        agents = [_make_agent(id="debugger", tier="fleet")]
        grid = self._make_mock_grid(agents)
        scroll = Mock()

        with patch("tui.widgets.fleet_grid.Static"):
            FleetGrid._rebuild_hierarchy_view(grid, scroll, set(), set())

        grid._mount_agent_cards.assert_called_once()

    def test_all_three_hierarchy_sections(self):
        agents = [
            _make_agent(id="arbiter", tier="core"),       # supervisor
            _make_agent(id="architect", tier="core"),      # coordinator
            _make_agent(id="debugger", tier="fleet"),      # specialist
        ]
        grid = self._make_mock_grid(agents)
        scroll = Mock()

        with patch("tui.widgets.fleet_grid.Static"):
            FleetGrid._rebuild_hierarchy_view(grid, scroll, set(), set())

        assert grid._mount_agent_cards.call_count == 3

    def test_filter_removes_from_hierarchy(self):
        agents = [
            _make_agent(id="arbiter", tier="core"),
            _make_agent(id="debugger", tier="fleet"),
        ]
        grid = self._make_mock_grid(agents, filter_text="arbiter")
        scroll = Mock()

        with patch("tui.widgets.fleet_grid.Static"):
            FleetGrid._rebuild_hierarchy_view(grid, scroll, set(), set())

        # Only supervisor section should have content
        grid._mount_agent_cards.assert_called_once()

    def test_view_indicator_always_mounted(self):
        """The hierarchy view indicator is always mounted regardless of agents."""
        grid = self._make_mock_grid([])
        scroll = Mock()

        with patch("tui.widgets.fleet_grid.Static") as MockStatic:
            FleetGrid._rebuild_hierarchy_view(grid, scroll, set(), set())

        # View indicator should be mounted even with no agents
        scroll.mount.assert_called_once()


# ===========================================================================
# FleetGrid._mount_agent_cards
# ===========================================================================

class TestMountAgentCards(unittest.TestCase):
    def _make_mock_grid(self):
        grid = Mock(spec=FleetGrid)
        grid._CARDS_PER_ROW = 4
        return grid

    def test_single_row(self):
        agents = [_make_agent(id=f"a{i}") for i in range(3)]
        grid = self._make_mock_grid()
        scroll = Mock()
        mock_row = Mock()

        with patch("tui.widgets.fleet_grid.Horizontal", return_value=mock_row), \
             patch("tui.widgets.fleet_grid.AgentCard") as MockCard:
            MockCard.side_effect = lambda a, id: Mock()
            FleetGrid._mount_agent_cards(grid, scroll, agents, set(), set())

        # 1 row mounted to scroll
        scroll.mount.assert_called_once_with(mock_row)

    def test_two_rows_for_five_agents(self):
        agents = [_make_agent(id=f"a{i}") for i in range(5)]
        grid = self._make_mock_grid()
        scroll = Mock()

        with patch("tui.widgets.fleet_grid.Horizontal") as MockH, \
             patch("tui.widgets.fleet_grid.AgentCard") as MockCard:
            mock_rows = [Mock(), Mock()]
            MockH.side_effect = mock_rows
            MockCard.side_effect = lambda a, id: Mock()
            FleetGrid._mount_agent_cards(grid, scroll, agents, set(), set())

        assert scroll.mount.call_count == 2

    def test_active_status_set(self):
        agents = [_make_agent(id="arbiter")]
        grid = self._make_mock_grid()
        scroll = Mock()
        mock_card = Mock()

        with patch("tui.widgets.fleet_grid.Horizontal") as MockH, \
             patch("tui.widgets.fleet_grid.AgentCard", return_value=mock_card):
            mock_row = Mock()
            MockH.return_value = mock_row
            FleetGrid._mount_agent_cards(grid, scroll, agents, {"arbiter"}, set())

        assert mock_card.status == AC_STATUS_ACTIVE

    def test_error_status_set(self):
        agents = [_make_agent(id="guard")]
        grid = self._make_mock_grid()
        scroll = Mock()
        mock_card = Mock()

        with patch("tui.widgets.fleet_grid.Horizontal") as MockH, \
             patch("tui.widgets.fleet_grid.AgentCard", return_value=mock_card):
            mock_row = Mock()
            MockH.return_value = mock_row
            FleetGrid._mount_agent_cards(grid, scroll, agents, set(), {"guard"})

        assert mock_card.status == AC_STATUS_ERROR

    def test_error_takes_priority_over_active(self):
        """Error state takes priority when agent is in both active and error sets."""
        agents = [_make_agent(id="guard")]
        grid = self._make_mock_grid()
        scroll = Mock()
        mock_card = Mock()

        with patch("tui.widgets.fleet_grid.Horizontal") as MockH, \
             patch("tui.widgets.fleet_grid.AgentCard", return_value=mock_card):
            mock_row = Mock()
            MockH.return_value = mock_row
            FleetGrid._mount_agent_cards(grid, scroll, agents, {"guard"}, {"guard"})

        assert mock_card.status == AC_STATUS_ERROR

    def test_no_status_for_idle_agent(self):
        agents = [_make_agent(id="idle_agent")]
        grid = self._make_mock_grid()
        scroll = Mock()
        mock_card = MagicMock()
        # Remove status attribute to track if it gets set
        del mock_card.status

        with patch("tui.widgets.fleet_grid.Horizontal") as MockH, \
             patch("tui.widgets.fleet_grid.AgentCard", return_value=mock_card):
            mock_row = Mock()
            MockH.return_value = mock_row
            FleetGrid._mount_agent_cards(grid, scroll, agents, set(), set())

        # status should NOT have been set since agent is not active or error
        assert not hasattr(mock_card, "status")


# ===========================================================================
# FleetGrid._get_active_agent_ids
# ===========================================================================

class TestGetActiveAgentIds(unittest.TestCase):
    def test_reads_state_file(self):
        state = {
            "agents": {
                "arbiter": {"status": "online"},
                "debugger": {"status": "offline"},
                "orchestrator": {"status": "online"},
            }
        }
        grid = _make_fleet_grid()
        m = mock_open(read_data=json.dumps(state))
        with patch("builtins.open", m):
            result = grid._get_active_agent_ids()
        assert result == {"arbiter", "orchestrator"}

    def test_fallback_on_file_not_found(self):
        grid = _make_fleet_grid()
        with patch("builtins.open", side_effect=FileNotFoundError):
            result = grid._get_active_agent_ids()
        assert result == {"arbiter", "orchestrator"}

    def test_fallback_on_json_decode_error(self):
        grid = _make_fleet_grid()
        m = mock_open(read_data="not json")
        with patch("builtins.open", m):
            result = grid._get_active_agent_ids()
        assert result == {"arbiter", "orchestrator"}

    def test_empty_agents_in_state(self):
        state = {"agents": {}}
        grid = _make_fleet_grid()
        m = mock_open(read_data=json.dumps(state))
        with patch("builtins.open", m):
            result = grid._get_active_agent_ids()
        assert result == set()

    def test_respects_euxis_home_env(self):
        state = {"agents": {"x": {"status": "online"}}}
        grid = _make_fleet_grid()
        m = mock_open(read_data=json.dumps(state))
        with patch.dict("os.environ", {"EUXIS_HOME": "/custom/home"}), \
             patch("builtins.open", m) as mock_f:
            result = grid._get_active_agent_ids()
        # Verify the path includes custom home
        call_path = mock_f.call_args[0][0]
        assert "/custom/home/" in call_path

    def test_missing_agents_key(self):
        state = {}
        grid = _make_fleet_grid()
        m = mock_open(read_data=json.dumps(state))
        with patch("builtins.open", m):
            result = grid._get_active_agent_ids()
        assert result == set()


# ===========================================================================
# FleetGrid._get_error_agent_ids
# ===========================================================================

class TestGetErrorAgentIds(unittest.TestCase):
    def test_reads_state_file(self):
        state = {
            "agents": {
                "guard": {"status": "error"},
                "arbiter": {"status": "online"},
            }
        }
        grid = _make_fleet_grid()
        m = mock_open(read_data=json.dumps(state))
        with patch("builtins.open", m):
            result = grid._get_error_agent_ids()
        assert result == {"guard"}

    def test_fallback_on_file_not_found(self):
        grid = _make_fleet_grid()
        with patch("builtins.open", side_effect=FileNotFoundError):
            result = grid._get_error_agent_ids()
        assert result == {"guard"}

    def test_fallback_on_json_decode_error(self):
        grid = _make_fleet_grid()
        m = mock_open(read_data="{{bad json")
        with patch("builtins.open", m):
            result = grid._get_error_agent_ids()
        assert result == {"guard"}

    def test_empty_agents_in_state(self):
        state = {"agents": {}}
        grid = _make_fleet_grid()
        m = mock_open(read_data=json.dumps(state))
        with patch("builtins.open", m):
            result = grid._get_error_agent_ids()
        assert result == set()

    def test_respects_euxis_home_env(self):
        state = {"agents": {"x": {"status": "error"}}}
        grid = _make_fleet_grid()
        m = mock_open(read_data=json.dumps(state))
        with patch.dict("os.environ", {"EUXIS_HOME": "/custom/home"}), \
             patch("builtins.open", m) as mock_f:
            result = grid._get_error_agent_ids()
        call_path = mock_f.call_args[0][0]
        assert "/custom/home/" in call_path

    def test_missing_agents_key(self):
        state = {}
        grid = _make_fleet_grid()
        m = mock_open(read_data=json.dumps(state))
        with patch("builtins.open", m):
            result = grid._get_error_agent_ids()
        assert result == set()


# ===========================================================================
# FleetGrid._mount_agent_rows
# ===========================================================================

class TestMountAgentRows(unittest.TestCase):
    def _make_mock_grid(self):
        grid = Mock(spec=FleetGrid)
        grid._CARDS_PER_ROW = 4
        return grid

    def test_active_rows(self):
        agents = [_make_agent(id="a1")]
        grid = self._make_mock_grid()
        scroll = Mock()
        mock_card = Mock()

        with patch("tui.widgets.fleet_grid.Horizontal") as MockH, \
             patch("tui.widgets.fleet_grid.AgentCard", return_value=mock_card):
            mock_row = Mock()
            MockH.return_value = mock_row
            FleetGrid._mount_agent_rows(grid, scroll, agents, is_active=True)

        MockH.assert_called_once_with(classes="fleet-row fleet-row-active")
        mock_card.add_class.assert_called_with("status-active")

    def test_standby_rows(self):
        agents = [_make_agent(id="a1")]
        grid = self._make_mock_grid()
        scroll = Mock()
        mock_card = Mock()

        with patch("tui.widgets.fleet_grid.Horizontal") as MockH, \
             patch("tui.widgets.fleet_grid.AgentCard", return_value=mock_card):
            mock_row = Mock()
            MockH.return_value = mock_row
            FleetGrid._mount_agent_rows(grid, scroll, agents, is_active=False)

        MockH.assert_called_once_with(classes="fleet-row fleet-row-standby")
        mock_card.add_class.assert_called_with("status-standby")

    def test_multiple_rows(self):
        agents = [_make_agent(id=f"a{i}") for i in range(5)]
        grid = self._make_mock_grid()
        scroll = Mock()

        with patch("tui.widgets.fleet_grid.Horizontal") as MockH, \
             patch("tui.widgets.fleet_grid.AgentCard"):
            MockH.side_effect = [Mock(), Mock()]
            FleetGrid._mount_agent_rows(grid, scroll, agents, is_active=True)

        assert scroll.mount.call_count == 2


# ===========================================================================
# FleetGrid._mount_squads_and_combos
# ===========================================================================

class TestMountSquadsAndCombos(unittest.TestCase):
    def _make_mock_grid(self, squads=None, combos=None, filter_text=""):
        registry = _make_registry(squads=squads or [], combos=combos or [])
        grid = Mock(spec=FleetGrid)
        grid.registry = registry
        grid.filter_text = filter_text
        grid._matches_squad = lambda s: FleetGrid._matches_squad(grid, s)
        grid._matches_combo = lambda c: FleetGrid._matches_combo(grid, c)
        return grid

    def test_no_squads_or_combos(self):
        grid = self._make_mock_grid()
        scroll = Mock()

        FleetGrid._mount_squads_and_combos(grid, scroll)

        scroll.mount.assert_not_called()

    def test_squads_only(self):
        squads = [_make_squad(id="alpha")]
        grid = self._make_mock_grid(squads=squads)
        scroll = Mock()

        with patch("tui.widgets.fleet_grid.Static"), \
             patch("tui.widgets.fleet_grid._ClickableRow"):
            FleetGrid._mount_squads_and_combos(grid, scroll)

        # divider + section label + section desc + 1 squad row = 4
        assert scroll.mount.call_count == 4

    def test_combos_only(self):
        combos = [_make_combo(id="envision")]
        grid = self._make_mock_grid(combos=combos)
        scroll = Mock()

        with patch("tui.widgets.fleet_grid.Static"), \
             patch("tui.widgets.fleet_grid._ClickableRow"):
            FleetGrid._mount_squads_and_combos(grid, scroll)

        # divider + section label + section desc + 1 combo row = 4
        assert scroll.mount.call_count == 4

    def test_both_squads_and_combos(self):
        squads = [_make_squad(id="alpha")]
        combos = [_make_combo(id="envision")]
        grid = self._make_mock_grid(squads=squads, combos=combos)
        scroll = Mock()

        with patch("tui.widgets.fleet_grid.Static"), \
             patch("tui.widgets.fleet_grid._ClickableRow"):
            FleetGrid._mount_squads_and_combos(grid, scroll)

        # divider + squad label + squad desc + 1 squad row + combo label + combo desc + 1 combo row = 7
        assert scroll.mount.call_count == 7

    def test_multiple_squads(self):
        squads = [_make_squad(id=f"s{i}") for i in range(3)]
        grid = self._make_mock_grid(squads=squads)
        scroll = Mock()

        with patch("tui.widgets.fleet_grid.Static"), \
             patch("tui.widgets.fleet_grid._ClickableRow"):
            FleetGrid._mount_squads_and_combos(grid, scroll)

        # divider + label + desc + 3 rows = 6
        assert scroll.mount.call_count == 6

    def test_filtered_squads(self):
        squads = [
            _make_squad(id="alpha", name="Alpha"),
            _make_squad(id="beta", name="Beta"),
        ]
        grid = self._make_mock_grid(squads=squads, filter_text="alpha")
        scroll = Mock()

        with patch("tui.widgets.fleet_grid.Static"), \
             patch("tui.widgets.fleet_grid._ClickableRow"):
            FleetGrid._mount_squads_and_combos(grid, scroll)

        # divider + label + desc + 1 row = 4
        assert scroll.mount.call_count == 4

    def test_all_filtered_out(self):
        squads = [_make_squad(id="alpha")]
        grid = self._make_mock_grid(squads=squads, filter_text="zzz_nonexistent")
        scroll = Mock()

        FleetGrid._mount_squads_and_combos(grid, scroll)

        scroll.mount.assert_not_called()


# ===========================================================================
# FleetGrid navigation methods
# ===========================================================================

class TestUpdateActiveIndicator(unittest.TestCase):
    def test_with_active_cards(self):
        grid = _make_fleet_grid()
        mock_indicator = Mock()
        mock_card1 = Mock()
        mock_card1.status = STATUS_ACTIVE
        mock_card1.agent = Mock(id="arbiter")
        mock_card2 = Mock()
        mock_card2.status = STATUS_ACTIVE
        mock_card2.agent = Mock(id="orchestrator")

        def fake_query_one(selector, cls=None):
            return mock_indicator

        def fake_query(cls):
            return [mock_card1, mock_card2]

        grid.query_one = fake_query_one
        grid.query = fake_query

        grid.update_active_indicator()

        mock_indicator.update.assert_called_once()
        call_text = mock_indicator.update.call_args[0][0]
        assert "2 Active" in call_text
        assert "arbiter" in call_text
        mock_indicator.remove_class.assert_called_with("hidden")

    def test_with_more_than_three_active(self):
        grid = _make_fleet_grid()
        mock_indicator = Mock()
        cards = []
        for i in range(5):
            c = Mock()
            c.status = STATUS_ACTIVE
            c.agent = Mock(id=f"agent{i}")
            cards.append(c)

        grid.query_one = Mock(return_value=mock_indicator)
        grid.query = Mock(return_value=cards)

        grid.update_active_indicator()

        call_text = mock_indicator.update.call_args[0][0]
        assert "+2" in call_text

    def test_no_active_cards(self):
        grid = _make_fleet_grid()
        mock_indicator = Mock()
        mock_card = Mock()
        mock_card.status = "idle"

        grid.query_one = Mock(return_value=mock_indicator)
        grid.query = Mock(return_value=[mock_card])

        grid.update_active_indicator()

        mock_indicator.add_class.assert_called_with("hidden")

    def test_exception_handling(self):
        grid = _make_fleet_grid()
        grid.query_one = Mock(side_effect=Exception("not mounted"))
        grid.update_active_indicator()  # Should not raise


# ===========================================================================
# FleetGrid.action_goto_active
# ===========================================================================

class TestStripPrefixesAndCategories(unittest.TestCase):
    def test_strip_prefixes(self):
        assert FleetGrid._STRIP_PREFIXES == {"euxis"}

    def test_strip_categories(self):
        expected = {
            "combo", "combos", "squad", "squads", "agent", "agents",
            "run", "deploy", "execute", "start", "launch",
        }
        assert FleetGrid._STRIP_CATEGORIES == expected


# ===========================================================================
# FleetGrid reactive properties
# ===========================================================================
