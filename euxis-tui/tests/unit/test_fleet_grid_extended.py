# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

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
        "version": "0.1.0",
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


class TestModuleConstants(unittest.TestCase):
    """Validate exported constants are accessible and correct."""

    def test_section_descriptions_keys(self):
        assert set(SECTION_DESCRIPTIONS) == {"core", "default", "ondemand", "specialist"}

    def test_hierarchy_descriptions_keys(self):
        assert set(HIERARCHY_DESCRIPTIONS) == {"supervisor", "coordinator", "specialist"}

    def test_status_active(self):
        assert STATUS_ACTIVE == "active"

    def test_status_idle(self):
        assert STATUS_IDLE == "idle"

    def test_min_quoted_length(self):
        assert MIN_QUOTED_LENGTH == 2

    def test_system_commands(self):
        expected = {"health", "certify", "lint", "cortex", "metrics",
                    "settings", "help", "about", "playbooks", "refresh"}
        assert SYSTEM_COMMANDS == expected


# ===========================================================================
# Message classes
# ===========================================================================


class TestAgentSelectedMessage(unittest.TestCase):
    def test_defaults(self):
        msg = AgentSelected("architect")
        assert msg.agent_id == "architect"
        assert msg.task == ""

    def test_with_task(self):
        msg = AgentSelected("architect", task="Review code")
        assert msg.agent_id == "architect"
        assert msg.task == "Review code"


class TestSquadSelectedMessage(unittest.TestCase):
    def test_defaults(self):
        msg = SquadSelected("alpha")
        assert msg.squad_id == "alpha"
        assert msg.task == ""

    def test_with_task(self):
        msg = SquadSelected("alpha", task="Deploy all")
        assert msg.squad_id == "alpha"
        assert msg.task == "Deploy all"


class TestComboSelectedMessage(unittest.TestCase):
    def test_defaults(self):
        msg = ComboSelected("envision")
        assert msg.combo_id == "envision"
        assert msg.task == ""

    def test_with_task(self):
        msg = ComboSelected("envision", task="Analyse repo")
        assert msg.combo_id == "envision"
        assert msg.task == "Analyse repo"


class TestSystemCommandRequestedMessage(unittest.TestCase):
    def test_message(self):
        msg = SystemCommandRequested("health")
        assert msg.command == "health"


class TestBulkRestartRequestedMessage(unittest.TestCase):
    def test_message(self):
        msg = BulkRestartRequested(["guard", "debugger"])
        assert msg.agent_ids == ["guard", "debugger"]


# ===========================================================================
# _ClickableRow
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


class TestFleetGridInit(unittest.TestCase):
    def test_init_attributes(self):
        registry = _make_registry()
        grid = FleetGrid(registry)
        assert grid.registry is registry
        assert grid._mounted is False
        assert grid._rebuilding is False


# ===========================================================================
# FleetGrid.compose
# ===========================================================================


class TestFleetGridCompose(unittest.TestCase):
    @patch("tui.widgets.fleet_grid.VerticalScroll")
    @patch("tui.widgets.fleet_grid.Input")
    @patch("tui.widgets.fleet_grid.Container")
    def test_compose_yields_widgets(self, _cont, _inp, _scroll):
        grid = _make_fleet_grid()
        widgets = list(grid.compose())
        assert len(widgets) > 0


# ===========================================================================
# FleetGrid.on_mount / _initial_build
# ===========================================================================


class TestFleetGridOnMount(unittest.TestCase):
    def test_on_mount_first_time(self):
        grid = _make_fleet_grid()
        grid.call_later = Mock()

        grid.on_mount()

        assert grid._mounted is True
        grid.call_later.assert_called_once_with(grid._initial_build)

    def test_on_mount_already_mounted(self):
        grid = _make_fleet_grid()
        grid._mounted = True
        grid.call_later = Mock()

        grid.on_mount()

        grid.call_later.assert_not_called()

    def test_initial_build_calls_rebuild(self):
        grid = _make_fleet_grid()
        grid._rebuild_grid = Mock()
        grid.focus_first_card = Mock()
        grid._rebuilding = False

        grid._initial_build()

        grid._rebuild_grid.assert_called_once()
        grid.focus_first_card.assert_called_once()

    def test_initial_build_skips_when_rebuilding(self):
        grid = _make_fleet_grid()
        grid._rebuild_grid = Mock()
        grid.focus_first_card = Mock()
        grid._rebuilding = True

        grid._initial_build()

        grid._rebuild_grid.assert_not_called()
        grid.focus_first_card.assert_not_called()


# ===========================================================================
# FleetGrid.on_input_changed
# ===========================================================================


class TestFleetGridOnInputChanged(unittest.TestCase):
    def test_fleet_filter_input(self):
        grid = _make_fleet_grid()
        event = Mock()
        event.input.id = "fleet-filter-input"
        event.value = "debug"
        with patch.object(grid, "_rebuild_grid"):
            grid.on_input_changed(event)
        assert grid.filter_text == "debug"

    def test_other_input_ignored(self):
        grid = _make_fleet_grid()
        event = Mock()
        event.input.id = "some-other-input"
        event.value = "xyz"
        # Should not touch filter_text; use PropertyMock to verify
        with patch.object(type(grid), "filter_text", new_callable=PropertyMock) as mock_ft:
            grid.on_input_changed(event)
            mock_ft.assert_not_called()


# ===========================================================================
# FleetGrid._parse_search_command
# ===========================================================================


class TestParseSearchCommand(unittest.TestCase):
    def _parse(self, text):
        grid = _make_fleet_grid()
        return grid._parse_search_command(text)

    def test_simple_entity(self):
        name, task = self._parse("architect")
        assert name == "architect"
        assert task == ""

    def test_entity_with_task(self):
        name, task = self._parse("envision Analyse this repo")
        assert name == "envision"
        assert task == "Analyse this repo"

    def test_strip_euxis_prefix(self):
        name, task = self._parse("euxis combos run envision task")
        assert name == "envision"
        assert task == "task"

    def test_strip_all_prefixes(self):
        name, task = self._parse("euxis agents run deploy orchestrator Review code")
        assert name == "orchestrator"
        assert task == "Review code"

    def test_quoted_task(self):
        name, task = self._parse('envision "Analyse this repo"')
        assert name == "envision"
        assert task == "Analyse this repo"

    def test_single_quoted_task(self):
        name, task = self._parse("envision 'Analyse this repo'")
        assert name == "envision"
        assert task == "Analyse this repo"

    def test_quoted_entity_name(self):
        name, task = self._parse('"envision" some task')
        assert name == "envision"
        assert task == "some task"

    def test_all_prefix_words_stripped(self):
        # All words are prefix/category tokens
        name, task = self._parse("euxis run")
        # After stripping, words is empty, so fallback to text.strip().lower()
        assert name == "euxis run"
        assert task == ""

    def test_system_command(self):
        name, task = self._parse("health")
        assert name == "health"
        assert task == ""

    def test_case_insensitive_prefix_strip(self):
        name, task = self._parse("Euxis Combos Run envision")
        assert name == "envision"

    def test_short_task_not_unquoted(self):
        # Task is single char in quotes -- length < MIN_QUOTED_LENGTH after join
        name, task = self._parse("agent x")
        # "x" has length 1, so the quote-stripping branch won't trigger
        assert name == "x"
        assert task == ""

    def test_task_with_mismatched_quotes_not_stripped(self):
        name, task = self._parse('envision "Analyse this')
        assert name == "envision"
        # Quotes don't match (first char != last char), so task unchanged
        assert task == '"Analyse this'


# ===========================================================================
# FleetGrid._find_exact_entity
# ===========================================================================


class TestFindExactEntity(unittest.TestCase):
    def test_find_agent(self):
        agent = _make_agent(id="architect")
        grid = _make_fleet_grid(agents=[agent])
        result = grid._find_exact_entity("architect")
        assert result == ("agent", "architect")

    def test_find_squad(self):
        squad = _make_squad(id="alpha")
        grid = _make_fleet_grid(squads=[squad])
        result = grid._find_exact_entity("alpha")
        assert result == ("squad", "alpha")

    def test_find_combo(self):
        combo = _make_combo(id="envision")
        grid = _make_fleet_grid(combos=[combo])
        result = grid._find_exact_entity("envision")
        assert result == ("combo", "envision")

    def test_no_match(self):
        grid = _make_fleet_grid()
        result = grid._find_exact_entity("nonexistent")
        assert result is None

    def test_agent_priority_over_squad(self):
        """Agent match takes priority when agent and squad have same id."""
        agent = _make_agent(id="alpha")
        squad = _make_squad(id="alpha")
        grid = _make_fleet_grid(agents=[agent], squads=[squad])
        result = grid._find_exact_entity("alpha")
        assert result == ("agent", "alpha")


# ===========================================================================
# FleetGrid._dispatch_selection
# ===========================================================================


class TestDispatchSelection(unittest.TestCase):
    def test_dispatch_agent(self):
        grid = _make_fleet_grid()
        grid.post_message = Mock()
        grid._dispatch_selection("agent", "architect", "Review code")
        msg = grid.post_message.call_args[0][0]
        assert isinstance(msg, AgentSelected)
        assert msg.agent_id == "architect"
        assert msg.task == "Review code"

    def test_dispatch_squad(self):
        grid = _make_fleet_grid()
        grid.post_message = Mock()
        grid._dispatch_selection("squad", "alpha", "Deploy all")
        msg = grid.post_message.call_args[0][0]
        assert isinstance(msg, SquadSelected)
        assert msg.squad_id == "alpha"
        assert msg.task == "Deploy all"

    def test_dispatch_combo(self):
        grid = _make_fleet_grid()
        grid.post_message = Mock()
        grid._dispatch_selection("combo", "envision", "Analyse")
        msg = grid.post_message.call_args[0][0]
        assert isinstance(msg, ComboSelected)
        assert msg.combo_id == "envision"
        assert msg.task == "Analyse"

    def test_dispatch_unknown_type(self):
        grid = _make_fleet_grid()
        grid.post_message = Mock()
        grid._dispatch_selection("unknown", "something")
        grid.post_message.assert_not_called()


# ===========================================================================
# FleetGrid._clear_search
# ===========================================================================


class TestClearSearch(unittest.TestCase):
    def test_clears_input_and_filter(self):
        grid = _make_fleet_grid()
        mock_input = Mock()
        with patch.object(grid, "_rebuild_grid"):
            grid._clear_search(mock_input)
        assert mock_input.value == ""
        assert grid.filter_text == ""


# ===========================================================================
# FleetGrid.on_input_submitted
# ===========================================================================


class TestOnInputSubmitted(unittest.TestCase):
    def _make_event(self, value, input_id="fleet-filter-input"):
        event = Mock()
        event.input.id = input_id
        event.value = value
        return event

    def test_wrong_input_id_returns(self):
        grid = _make_fleet_grid()
        grid.post_message = Mock()
        event = self._make_event("health", input_id="other-input")
        grid.on_input_submitted(event)
        grid.post_message.assert_not_called()

    def test_empty_query_returns(self):
        grid = _make_fleet_grid()
        grid.post_message = Mock()
        event = self._make_event("   ")
        grid.on_input_submitted(event)
        grid.post_message.assert_not_called()

    def test_exact_agent_match(self):
        agent = _make_agent(id="architect")
        grid = _make_fleet_grid(agents=[agent])
        grid.post_message = Mock()
        grid._clear_search = Mock()

        event = self._make_event("architect")
        grid.on_input_submitted(event)

        msg = grid.post_message.call_args[0][0]
        assert isinstance(msg, AgentSelected)
        assert msg.agent_id == "architect"
        grid._clear_search.assert_called_once()

    def test_exact_squad_match(self):
        squad = _make_squad(id="alpha")
        grid = _make_fleet_grid(squads=[squad])
        grid.post_message = Mock()
        grid._clear_search = Mock()

        event = self._make_event("alpha")
        grid.on_input_submitted(event)

        msg = grid.post_message.call_args[0][0]
        assert isinstance(msg, SquadSelected)
        grid._clear_search.assert_called_once()

    def test_exact_combo_match(self):
        combo = _make_combo(id="envision")
        grid = _make_fleet_grid(combos=[combo])
        grid.post_message = Mock()
        grid._clear_search = Mock()

        event = self._make_event("envision")
        grid.on_input_submitted(event)

        msg = grid.post_message.call_args[0][0]
        assert isinstance(msg, ComboSelected)
        grid._clear_search.assert_called_once()

    def test_system_command(self):
        grid = _make_fleet_grid()
        grid.post_message = Mock()
        grid._clear_search = Mock()

        event = self._make_event("health")
        grid.on_input_submitted(event)

        msg = grid.post_message.call_args[0][0]
        assert isinstance(msg, SystemCommandRequested)
        assert msg.command == "health"
        grid._clear_search.assert_called_once()

    def test_fallback_first_filtered_agent(self):
        agent = _make_agent(id="architect")
        grid = _make_fleet_grid(agents=[agent])
        grid.post_message = Mock()
        grid._clear_search = Mock()
        # Set filter_text so _matches_filter returns True for partial match
        with patch.object(grid, "_rebuild_grid"):
            grid.filter_text = "arch"

        event = self._make_event("arch")
        grid.on_input_submitted(event)

        msg = grid.post_message.call_args[0][0]
        assert isinstance(msg, AgentSelected)
        grid._clear_search.assert_called_once()

    def test_no_match_no_clear(self):
        grid = _make_fleet_grid()
        grid.post_message = Mock()
        grid._clear_search = Mock()

        event = self._make_event("nonexistent_thing")
        grid.on_input_submitted(event)

        grid._clear_search.assert_not_called()

    def test_with_task_text(self):
        agent = _make_agent(id="architect")
        grid = _make_fleet_grid(agents=[agent])
        grid.post_message = Mock()
        grid._clear_search = Mock()

        event = self._make_event("architect Review the code")
        grid.on_input_submitted(event)

        msg = grid.post_message.call_args[0][0]
        assert isinstance(msg, AgentSelected)
        assert msg.task == "Review the code"


# ===========================================================================
# FleetGrid._dispatch_first_filtered
# ===========================================================================


class TestDispatchFirstFiltered(unittest.TestCase):
    def test_agent_match(self):
        agent = _make_agent(id="architect")
        grid = _make_fleet_grid(agents=[agent])
        grid.post_message = Mock()
        # Empty filter means _matches_filter returns True
        result = grid._dispatch_first_filtered()
        assert result is True
        msg = grid.post_message.call_args[0][0]
        assert isinstance(msg, AgentSelected)

    def test_squad_match_when_no_agent(self):
        squad = _make_squad(id="alpha")
        grid = _make_fleet_grid(squads=[squad])
        grid.post_message = Mock()
        result = grid._dispatch_first_filtered()
        assert result is True
        msg = grid.post_message.call_args[0][0]
        assert isinstance(msg, SquadSelected)

    def test_combo_match_when_no_agent_or_squad(self):
        combo = _make_combo(id="envision")
        grid = _make_fleet_grid(combos=[combo])
        grid.post_message = Mock()
        result = grid._dispatch_first_filtered()
        assert result is True
        msg = grid.post_message.call_args[0][0]
        assert isinstance(msg, ComboSelected)

    def test_no_match_returns_false(self):
        grid = _make_fleet_grid()
        grid.post_message = Mock()
        result = grid._dispatch_first_filtered()
        assert result is False
        grid.post_message.assert_not_called()

    def test_filtered_squad_no_agent_match(self):
        """Agent doesn't match filter, but squad does."""
        agent = _make_agent(id="architect")
        squad = _make_squad(id="security-team", name="Security Team",
                            purpose="security audits", lead="guard",
                            members=("guard", "auditor"))
        grid = _make_fleet_grid(agents=[agent], squads=[squad])
        grid.post_message = Mock()
        with patch.object(grid, "_rebuild_grid"):
            grid.filter_text = "security"
        result = grid._dispatch_first_filtered()
        assert result is True
        msg = grid.post_message.call_args[0][0]
        assert isinstance(msg, SquadSelected)

    def test_filtered_combo_no_agent_or_squad_match(self):
        """Neither agent nor squad matches filter, but combo does."""
        agent = _make_agent(id="architect")
        squad = _make_squad(id="alpha")
        combo = _make_combo(id="deep-analysis", name="Deep Analysis",
                            description="deep investigation pipeline",
                            chain=("analyst", "researcher"))
        grid = _make_fleet_grid(agents=[agent], squads=[squad], combos=[combo])
        grid.post_message = Mock()
        with patch.object(grid, "_rebuild_grid"):
            grid.filter_text = "deep"
        result = grid._dispatch_first_filtered()
        assert result is True
        msg = grid.post_message.call_args[0][0]
        assert isinstance(msg, ComboSelected)


# ===========================================================================
# FleetGrid._matches_filter
# ===========================================================================


class TestMatchesFilter(unittest.TestCase):
    def _call(self, filter_text, agent):
        mock_self = Mock()
        mock_self.filter_text = filter_text
        return FleetGrid._matches_filter(mock_self, agent)

    def test_empty_filter(self):
        assert self._call("", _make_agent()) is True

    def test_match_by_id(self):
        assert self._call("architect", _make_agent(id="architect")) is True

    def test_match_by_tag(self):
        assert self._call("security", _make_agent(tags=["security", "audit"])) is True

    def test_match_by_tier(self):
        assert self._call("core", _make_agent(tier="core")) is True

    def test_match_by_activation(self):
        assert self._call("on-demand", _make_agent(activation="on-demand")) is True

    def test_no_match(self):
        assert self._call("xyz", _make_agent()) is False

    def test_case_insensitive(self):
        assert self._call("ARCHITECT", _make_agent(id="Architect")) is True


# ===========================================================================
# FleetGrid._matches_squad
# ===========================================================================


class TestMatchesSquad(unittest.TestCase):
    def _call(self, filter_text, squad):
        mock_self = Mock()
        mock_self.filter_text = filter_text
        return FleetGrid._matches_squad(mock_self, squad)

    def test_empty_filter(self):
        assert self._call("", _make_squad()) is True

    def test_match_by_id(self):
        assert self._call("alpha", _make_squad(id="alpha")) is True

    def test_match_by_name(self):
        assert self._call("Alpha", _make_squad(name="Alpha Squad")) is True

    def test_match_by_purpose(self):
        assert self._call("engineering", _make_squad(purpose="Main engineering")) is True

    def test_match_by_lead(self):
        assert self._call("architect", _make_squad(lead="architect")) is True

    def test_match_by_member(self):
        assert self._call("debugger", _make_squad(members=("architect", "debugger"))) is True

    def test_no_match(self):
        assert self._call("xyz_nope", _make_squad()) is False


# ===========================================================================
# FleetGrid._matches_combo
# ===========================================================================


class TestMatchesCombo(unittest.TestCase):
    def _call(self, filter_text, combo):
        mock_self = Mock()
        mock_self.filter_text = filter_text
        return FleetGrid._matches_combo(mock_self, combo)

    def test_empty_filter(self):
        assert self._call("", _make_combo()) is True

    def test_match_by_id(self):
        assert self._call("envision", _make_combo(id="envision")) is True

    def test_match_by_name(self):
        assert self._call("Envision", _make_combo(name="Envision Combo")) is True

    def test_match_by_description(self):
        assert self._call("pipeline", _make_combo(description="Full analysis pipeline")) is True

    def test_match_by_chain_member(self):
        assert self._call("analyst", _make_combo(chain=("analyst", "architect"))) is True

    def test_no_match(self):
        assert self._call("xyz_nope", _make_combo()) is False


# ===========================================================================
# FleetGrid.watch_filter_text / watch_view_mode
# ===========================================================================


class TestWatchFilterText(unittest.TestCase):
    def test_rebuild_when_mounted(self):
        grid = _make_fleet_grid()
        grid._mounted = True
        grid._rebuilding = False
        grid._rebuild_grid = Mock()

        grid.watch_filter_text()

        grid._rebuild_grid.assert_called_once()

    def test_no_rebuild_when_not_mounted(self):
        grid = _make_fleet_grid()
        grid._mounted = False
        grid._rebuild_grid = Mock()

        grid.watch_filter_text()

        grid._rebuild_grid.assert_not_called()

    def test_no_rebuild_when_already_rebuilding(self):
        grid = _make_fleet_grid()
        grid._mounted = True
        grid._rebuilding = True
        grid._rebuild_grid = Mock()

        grid.watch_filter_text()

        grid._rebuild_grid.assert_not_called()


class TestWatchViewMode(unittest.TestCase):
    def test_rebuild_when_mounted(self):
        grid = _make_fleet_grid()
        grid._mounted = True
        grid._rebuilding = False
        grid._rebuild_grid = Mock()

        grid.watch_view_mode()

        grid._rebuild_grid.assert_called_once()

    def test_no_rebuild_when_not_mounted(self):
        grid = _make_fleet_grid()
        grid._mounted = False
        grid._rebuild_grid = Mock()

        grid.watch_view_mode()

        grid._rebuild_grid.assert_not_called()

    def test_no_rebuild_when_already_rebuilding(self):
        grid = _make_fleet_grid()
        grid._mounted = True
        grid._rebuilding = True
        grid._rebuild_grid = Mock()

        grid.watch_view_mode()

        grid._rebuild_grid.assert_not_called()


# ===========================================================================
# FleetGrid.action_toggle_view
# ===========================================================================


class TestActionToggleView(unittest.TestCase):
    def test_activation_to_hierarchy(self):
        grid = _make_fleet_grid()
        grid._reactive_view_mode = "activation"
        mock_app = Mock()
        with patch.object(type(grid), "app", new_callable=PropertyMock, return_value=mock_app):
            with patch.object(grid, "_rebuild_grid"):
                grid.action_toggle_view()
        assert grid.view_mode == "hierarchy"
        mock_app.notify.assert_called_once()
        assert "Hierarchy" in mock_app.notify.call_args[0][0]

    def test_hierarchy_to_activation(self):
        grid = _make_fleet_grid()
        with patch.object(grid, "_rebuild_grid"):
            grid._reactive_view_mode = "hierarchy"
        mock_app = Mock()
        with patch.object(type(grid), "app", new_callable=PropertyMock, return_value=mock_app):
            with patch.object(grid, "_rebuild_grid"):
                grid.action_toggle_view()
        assert grid.view_mode == "activation"
        mock_app.notify.assert_called_once()
        assert "Activation" in mock_app.notify.call_args[0][0]


# ===========================================================================
# FleetGrid._rebuild_grid
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


class TestNavigation(unittest.TestCase):
    def _make_nav_grid(self, num_cards=8, focused_index=0):
        """Create a grid with mock cards and screen for navigation testing."""
        grid = _make_fleet_grid()
        cards = [Mock() for _ in range(num_cards)]
        grid.query = Mock(return_value=cards)

        mock_screen = Mock()
        mock_screen.focused = cards[focused_index] if focused_index >= 0 else None

        return grid, cards, mock_screen

    def test_get_card_list(self):
        grid = _make_fleet_grid()
        mock_cards = [Mock(), Mock()]
        grid.query = Mock(return_value=mock_cards)
        result = grid._get_card_list()
        assert result == mock_cards

    def test_get_focused_index_found(self):
        grid, cards, mock_screen = self._make_nav_grid(num_cards=4, focused_index=2)
        with patch.object(type(grid), "screen", new_callable=PropertyMock, return_value=mock_screen):
            result = grid._get_focused_index()
        assert result == 2

    def test_get_focused_index_not_found(self):
        grid, cards, mock_screen = self._make_nav_grid(num_cards=4, focused_index=0)
        mock_screen.focused = Mock()  # Focused widget not in cards
        with patch.object(type(grid), "screen", new_callable=PropertyMock, return_value=mock_screen):
            result = grid._get_focused_index()
        assert result == -1

    def test_focus_card_at_valid(self):
        grid, cards, _ = self._make_nav_grid(num_cards=4, focused_index=0)
        grid._focus_card_at(2)
        cards[2].focus.assert_called_once()
        cards[2].scroll_visible.assert_called_once()

    def test_focus_card_at_clamps_high(self):
        grid, cards, _ = self._make_nav_grid(num_cards=4, focused_index=0)
        grid._focus_card_at(100)
        cards[3].focus.assert_called_once()

    def test_focus_card_at_clamps_low(self):
        grid, cards, _ = self._make_nav_grid(num_cards=4, focused_index=0)
        grid._focus_card_at(-5)
        cards[0].focus.assert_called_once()

    def test_focus_card_at_no_cards(self):
        grid = _make_fleet_grid()
        grid.query = Mock(return_value=[])
        grid._focus_card_at(0)  # Should not raise

    def test_action_nav_up_from_valid(self):
        grid, cards, mock_screen = self._make_nav_grid(num_cards=8, focused_index=4)
        with patch.object(type(grid), "screen", new_callable=PropertyMock, return_value=mock_screen):
            grid.action_nav_up()
        # 4 - 4 = 0
        cards[0].focus.assert_called_once()

    def test_action_nav_up_at_top(self):
        grid, cards, mock_screen = self._make_nav_grid(num_cards=8, focused_index=2)
        with patch.object(type(grid), "screen", new_callable=PropertyMock, return_value=mock_screen):
            grid.action_nav_up()
        # 2 - 4 = -2, so no navigation
        for card in cards:
            card.focus.assert_not_called()

    def test_action_nav_up_no_focus(self):
        grid, cards, mock_screen = self._make_nav_grid(num_cards=4, focused_index=0)
        mock_screen.focused = Mock()  # Not in cards
        grid.focus_first_card = Mock()
        with patch.object(type(grid), "screen", new_callable=PropertyMock, return_value=mock_screen):
            grid.action_nav_up()
        grid.focus_first_card.assert_called_once()

    def test_action_nav_down_from_valid(self):
        grid, cards, mock_screen = self._make_nav_grid(num_cards=8, focused_index=0)
        with patch.object(type(grid), "screen", new_callable=PropertyMock, return_value=mock_screen):
            grid.action_nav_down()
        # 0 + 4 = 4
        cards[4].focus.assert_called_once()

    def test_action_nav_down_at_bottom(self):
        grid, cards, mock_screen = self._make_nav_grid(num_cards=8, focused_index=6)
        with patch.object(type(grid), "screen", new_callable=PropertyMock, return_value=mock_screen):
            grid.action_nav_down()
        # 6 + 4 = 10 >= 8, so no navigation
        for card in cards:
            card.focus.assert_not_called()

    def test_action_nav_down_no_focus(self):
        grid, cards, mock_screen = self._make_nav_grid(num_cards=4, focused_index=0)
        mock_screen.focused = Mock()
        grid.focus_first_card = Mock()
        with patch.object(type(grid), "screen", new_callable=PropertyMock, return_value=mock_screen):
            grid.action_nav_down()
        grid.focus_first_card.assert_called_once()

    def test_action_nav_left_from_valid(self):
        grid, cards, mock_screen = self._make_nav_grid(num_cards=4, focused_index=2)
        with patch.object(type(grid), "screen", new_callable=PropertyMock, return_value=mock_screen):
            grid.action_nav_left()
        cards[1].focus.assert_called_once()

    def test_action_nav_left_at_start(self):
        grid, cards, mock_screen = self._make_nav_grid(num_cards=4, focused_index=0)
        with patch.object(type(grid), "screen", new_callable=PropertyMock, return_value=mock_screen):
            grid.action_nav_left()
        # current=0, 0 > 0 is False, so no nav
        for card in cards:
            card.focus.assert_not_called()

    def test_action_nav_left_no_focus(self):
        grid, cards, mock_screen = self._make_nav_grid(num_cards=4, focused_index=0)
        mock_screen.focused = Mock()
        grid.focus_first_card = Mock()
        with patch.object(type(grid), "screen", new_callable=PropertyMock, return_value=mock_screen):
            grid.action_nav_left()
        grid.focus_first_card.assert_called_once()

    def test_action_nav_right_from_valid(self):
        grid, cards, mock_screen = self._make_nav_grid(num_cards=4, focused_index=1)
        with patch.object(type(grid), "screen", new_callable=PropertyMock, return_value=mock_screen):
            grid.action_nav_right()
        cards[2].focus.assert_called_once()

    def test_action_nav_right_at_end(self):
        grid, cards, mock_screen = self._make_nav_grid(num_cards=4, focused_index=3)
        with patch.object(type(grid), "screen", new_callable=PropertyMock, return_value=mock_screen):
            grid.action_nav_right()
        # current=3, 3 < 3 is False, so no nav
        for card in cards:
            card.focus.assert_not_called()

    def test_action_nav_right_no_focus(self):
        grid, cards, mock_screen = self._make_nav_grid(num_cards=4, focused_index=0)
        mock_screen.focused = Mock()
        grid.focus_first_card = Mock()
        with patch.object(type(grid), "screen", new_callable=PropertyMock, return_value=mock_screen):
            grid.action_nav_right()
        grid.focus_first_card.assert_called_once()


# ===========================================================================
# FleetGrid.focus_first_card
# ===========================================================================


class TestFocusFirstCard(unittest.TestCase):
    def test_with_cards(self):
        grid = _make_fleet_grid()
        mock_card = Mock()
        mock_cards = Mock()
        mock_cards.__bool__ = Mock(return_value=True)
        mock_cards.first = Mock(return_value=mock_card)
        grid.query = Mock(return_value=mock_cards)

        grid.focus_first_card()
        mock_card.focus.assert_called_once()

    def test_no_cards(self):
        grid = _make_fleet_grid()
        mock_cards = Mock()
        mock_cards.__bool__ = Mock(return_value=False)
        grid.query = Mock(return_value=mock_cards)

        grid.focus_first_card()  # Should not raise


# ===========================================================================
# FleetGrid.update_active_indicator
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


class TestActionGotoActive(unittest.TestCase):
    def test_with_active_cards(self):
        grid = _make_fleet_grid()
        mock_card = Mock()
        mock_card.status = STATUS_ACTIVE

        grid.query = Mock(return_value=[mock_card])

        grid.action_goto_active()

        mock_card.focus.assert_called_once()
        mock_card.scroll_visible.assert_called_once()

    def test_no_active_cards(self):
        grid = _make_fleet_grid()
        mock_card = Mock()
        mock_card.status = "idle"

        grid.query = Mock(return_value=[mock_card])

        grid.action_goto_active()

        mock_card.focus.assert_not_called()

    def test_empty_query(self):
        grid = _make_fleet_grid()
        grid.query = Mock(return_value=[])
        grid.action_goto_active()  # Should not raise


# ===========================================================================
# FleetGrid.action_restart_all_failed
# ===========================================================================


class TestActionRestartAllFailed(unittest.TestCase):
    def test_with_error_cards(self):
        grid = _make_fleet_grid()
        grid.post_message = Mock()

        mock_card1 = Mock()
        mock_card1.status = AC_STATUS_ERROR
        mock_card1.agent = Mock(id="guard")
        mock_card2 = Mock()
        mock_card2.status = AC_STATUS_ERROR
        mock_card2.agent = Mock(id="debugger")
        mock_idle = Mock()
        mock_idle.status = "idle"

        grid.query = Mock(return_value=[mock_card1, mock_card2, mock_idle])

        grid.action_restart_all_failed()

        msg = grid.post_message.call_args[0][0]
        assert isinstance(msg, BulkRestartRequested)
        assert msg.agent_ids == ["guard", "debugger"]

    def test_no_error_cards(self):
        grid = _make_fleet_grid()
        grid.post_message = Mock()

        mock_card = Mock()
        mock_card.status = "idle"
        grid.query = Mock(return_value=[mock_card])

        grid.action_restart_all_failed()

        grid.post_message.assert_not_called()

    def test_empty_query(self):
        grid = _make_fleet_grid()
        grid.post_message = Mock()
        grid.query = Mock(return_value=[])
        grid.action_restart_all_failed()
        grid.post_message.assert_not_called()


# ===========================================================================
# FleetGrid BINDINGS
# ===========================================================================


class TestFleetGridBindings(unittest.TestCase):
    def test_bindings_count(self):
        assert len(FleetGrid.BINDINGS) == 11

    def test_nav_keys_present(self):
        keys = [b.key for b in FleetGrid.BINDINGS]
        for k in ["up", "down", "left", "right", "k", "j", "h", "l", "g", "R", "v"]:
            assert k in keys, f"Expected key '{k}' in bindings"


# ===========================================================================
# FleetGrid._STRIP_PREFIXES / _STRIP_CATEGORIES
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


class TestFleetGridReactiveDefaults(unittest.TestCase):
    def test_filter_text_default(self):
        grid = _make_fleet_grid()
        assert grid.filter_text == ""

    def test_view_mode_default(self):
        grid = _make_fleet_grid()
        assert grid.view_mode == "activation"

    def test_cards_per_row(self):
        assert FleetGrid._CARDS_PER_ROW == 4


if __name__ == "__main__":
    unittest.main()
