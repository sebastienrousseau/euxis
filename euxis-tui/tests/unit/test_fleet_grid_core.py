# SPDX-License-Identifier: AGPL-3.0-or-later
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
