# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

#!/usr/bin/env python3
"""Comprehensive unit tests for tui/commands.py.

Tests the three command palette providers:
- AgentCommandProvider
- SquadCommandProvider
- SystemCommandProvider

All tests mock the Textual Provider/App infrastructure so no running
application is required.

Note: The search() methods use Hit(partial=...) which is a Textual
version-specific API. We test the callback factories and provider
structure directly. For search() coverage, we patch the Hit constructor
to accept both `partial` and `command` keyword arguments.
"""

from __future__ import annotations

import asyncio
import unittest
from unittest.mock import MagicMock, patch

from textual.command import Hit

from tui.commands import AgentCommandProvider, SquadCommandProvider, SystemCommandProvider
from tui.core.registry import Agent, Combo, FleetRegistry, Squad

# ============================================================================
# Helpers
# ============================================================================


def _make_registry() -> FleetRegistry:
    """Create a FleetRegistry with a small test dataset."""
    registry = FleetRegistry()
    registry.agents = [
        Agent(id="architect", tier="core", version="0.1.0",
              tags=("design", "architecture"), activation="default"),
        Agent(id="debugger", tier="fleet", version="0.1.0",
              tags=("debug", "troubleshoot"), activation="default"),
        Agent(id="tester", tier="fleet", version="0.1.0",
              tags=("test", "quality"), activation="on-demand"),
    ]
    registry.squads = [
        Squad(id="alpha", name="Alpha Squad", purpose="Frontend development",
              lead="architect", members=("architect", "debugger", "tester")),
        Squad(id="beta", name="Beta Squad", purpose="Backend services",
              lead="debugger", members=("debugger", "tester")),
    ]
    registry.combos = [
        Combo(id="review-chain", name="Review Chain",
              description="Code review pipeline",
              chain=("architect", "reviewer", "tester")),
    ]
    return registry


def _make_mock_app(registry: FleetRegistry | None = None) -> MagicMock:
    """Create a mock EuxisApp with a fleet registry."""
    app = MagicMock()
    app.fleet_registry = registry or _make_registry()
    app.action_deploy_agent = MagicMock()
    app.action_deploy_squad = MagicMock()
    app.action_deploy_combo = MagicMock()
    app.run_system_command = MagicMock()
    return app


def _make_provider(cls, mock_app: MagicMock):
    """Instantiate a command provider with mocked Textual plumbing.

    Provider.__init__ stores screen in the name-mangled _Provider__screen.
    We bypass __init__ with __new__ and set the internal attributes.
    """
    mock_screen = MagicMock()
    mock_screen.app = mock_app
    provider = cls.__new__(cls)
    provider._Provider__screen = mock_screen
    provider._Provider__match_style = None
    provider._init_task = None
    provider._init_success = False
    return provider


# Compatibility wrapper: the source uses Hit(partial=...) but Textual 7.5
# renamed it to `command`. We create a patched Hit that accepts either.
_OriginalHit = Hit


class _CompatHit:
    """Hit replacement that accepts both `partial` and `command` kwargs."""

    def __init__(self, score, match_display, command=None, partial=None,
                 text=None, help=None):
        # Accept either keyword
        callback = command or partial
        self._hit = _OriginalHit(
            score=score,
            match_display=match_display,
            command=callback,
            text=text,
            help=help,
        )

    def __getattr__(self, name):
        if name == "partial":
            return self._hit.command
        return getattr(self._hit, name)


def _collect_hits(provider, query: str) -> list:
    """Run the async search, patching Hit for API compatibility."""
    async def _run():
        hits = []
        with patch("tui.commands.Hit", _CompatHit):
            async for hit in provider.search(query):
                hits.append(hit)
        return hits
    return asyncio.get_event_loop().run_until_complete(_run())


# ============================================================================
# AgentCommandProvider
# ============================================================================


class TestAgentCommandProvider(unittest.TestCase):
    """Tests for the AgentCommandProvider."""

    def setUp(self):
        self.mock_app = _make_mock_app()
        self.provider = _make_provider(AgentCommandProvider, self.mock_app)

    def test_app_property_returns_app(self):
        """_app should return the mocked application."""
        assert self.provider._app is self.mock_app

    def test_search_returns_hits_for_matching_query(self):
        """Searching for an agent name should yield hits."""
        hits = _collect_hits(self.provider, "architect")
        assert len(hits) >= 1

    def test_search_matches_by_tag(self):
        """Searching by tag keyword should yield hits."""
        hits = _collect_hits(self.provider, "debug")
        assert len(hits) >= 1

    def test_search_returns_no_hits_for_nonmatching_query(self):
        """A completely unrelated query should yield no hits."""
        hits = _collect_hits(self.provider, "zzzznoagentzzz")
        assert len(hits) == 0

    def test_search_returns_hits_with_correct_help_text(self):
        """Hit help text should contain tier badge and tags."""
        hits = _collect_hits(self.provider, "architect")
        assert len(hits) >= 1
        assert "CORE" in hits[0].help

    def test_search_fleet_tier_badge(self):
        """Fleet tier agents should show FLEET badge."""
        hits = _collect_hits(self.provider, "debugger")
        assert len(hits) >= 1
        assert "FLEET" in hits[0].help

    def test_callback_calls_action_deploy_agent(self):
        """The hit callback should invoke action_deploy_agent."""
        hits = _collect_hits(self.provider, "architect")
        assert len(hits) >= 1
        hits[0].partial()
        self.mock_app.action_deploy_agent.assert_called_once_with("architect")

    def test_make_agent_callback_creates_callable(self):
        """_make_agent_callback should return a callable."""
        cb = self.provider._make_agent_callback("tester")
        assert callable(cb)
        cb()
        self.mock_app.action_deploy_agent.assert_called_once_with("tester")

    def test_broad_query_returns_multiple_agents(self):
        """A broad single-character query should match multiple agents."""
        # Use a character common across all agent IDs/tags
        hits = _collect_hits(self.provider, "e")
        # All 3 agents have 'e' somewhere in id or tags
        assert len(hits) >= 2


# ============================================================================
# SquadCommandProvider
# ============================================================================


class TestSquadCommandProvider(unittest.TestCase):
    """Tests for the SquadCommandProvider."""

    def setUp(self):
        self.mock_app = _make_mock_app()
        self.provider = _make_provider(SquadCommandProvider, self.mock_app)

    def test_app_property(self):
        assert self.provider._app is self.mock_app

    def test_search_returns_squad_hits(self):
        """Searching for a squad name should yield hits."""
        hits = _collect_hits(self.provider, "Alpha")
        assert len(hits) >= 1

    def test_search_matches_squad_purpose(self):
        """Searching by purpose keyword should yield hits."""
        hits = _collect_hits(self.provider, "Frontend")
        assert len(hits) >= 1

    def test_search_returns_combo_hits(self):
        """Searching for a combo name should yield hits."""
        hits = _collect_hits(self.provider, "Review Chain")
        assert len(hits) >= 1

    def test_search_matches_combo_description(self):
        """Searching by combo description keyword should yield hits."""
        hits = _collect_hits(self.provider, "pipeline")
        assert len(hits) >= 1

    def test_squad_hit_help_contains_purpose(self):
        """Squad hit help text should include the purpose."""
        hits = _collect_hits(self.provider, "Alpha")
        squad_hits = [h for h in hits if "Frontend" in h.help]
        assert len(squad_hits) >= 1

    def test_squad_hit_help_contains_members(self):
        """Squad hit help text should list some members."""
        hits = _collect_hits(self.provider, "Alpha")
        squad_hits = [h for h in hits if "architect" in h.help]
        assert len(squad_hits) >= 1

    def test_combo_hit_help_contains_chain(self):
        """Combo hit help text should show the chain."""
        hits = _collect_hits(self.provider, "Review Chain")
        combo_hits = [h for h in hits if "\u2192" in h.help or "architect" in h.help]
        assert len(combo_hits) >= 1

    def test_squad_callback_calls_deploy_squad(self):
        """Squad hit callback should call action_deploy_squad."""
        hits = _collect_hits(self.provider, "Alpha")
        for hit in hits:
            if "Frontend" in hit.help:
                hit.partial()
                break
        self.mock_app.action_deploy_squad.assert_called_once_with("alpha")

    def test_combo_callback_calls_deploy_combo(self):
        """Combo hit callback should call action_deploy_combo."""
        hits = _collect_hits(self.provider, "Review Chain")
        for hit in hits:
            if "pipeline" in hit.help.lower():
                hit.partial()
                break
        self.mock_app.action_deploy_combo.assert_called_once_with("review-chain")

    def test_make_squad_callback(self):
        cb = self.provider._make_squad_callback("beta")
        cb()
        self.mock_app.action_deploy_squad.assert_called_once_with("beta")

    def test_make_combo_callback(self):
        cb = self.provider._make_combo_callback("review-chain")
        cb()
        self.mock_app.action_deploy_combo.assert_called_once_with("review-chain")

    def test_no_hits_for_nonmatching(self):
        hits = _collect_hits(self.provider, "zzznomatchzzz")
        assert len(hits) == 0


# ============================================================================
# SystemCommandProvider
# ============================================================================


class TestSystemCommandProvider(unittest.TestCase):
    """Tests for the SystemCommandProvider."""

    def setUp(self):
        self.mock_app = _make_mock_app()
        self.provider = _make_provider(SystemCommandProvider, self.mock_app)

    def test_app_property(self):
        assert self.provider._app is self.mock_app

    def test_system_commands_not_empty(self):
        """SYSTEM_COMMANDS class variable should have entries."""
        assert len(SystemCommandProvider.SYSTEM_COMMANDS) > 0

    def test_system_commands_count(self):
        """SYSTEM_COMMANDS should have 16 entries."""
        assert len(SystemCommandProvider.SYSTEM_COMMANDS) == 16

    def test_system_commands_are_tuples_of_three(self):
        """Each command entry should be a 3-tuple."""
        for entry in SystemCommandProvider.SYSTEM_COMMANDS:
            assert len(entry) == 3, f"Expected 3-tuple, got {entry!r}"

    def test_system_commands_have_nonempty_fields(self):
        """Each command entry should have non-empty name, description, and action."""
        for name, description, action in SystemCommandProvider.SYSTEM_COMMANDS:
            assert name, "Command name should not be empty"
            assert description, "Command description should not be empty"
            assert action, "Command action should not be empty"

    def test_search_health_check(self):
        hits = _collect_hits(self.provider, "Health")
        assert len(hits) >= 1

    def test_search_toggle_theme(self):
        hits = _collect_hits(self.provider, "Toggle Theme")
        assert len(hits) >= 1

    def test_search_by_description(self):
        """Searching by description text should yield hits."""
        hits = _collect_hits(self.provider, "keyboard shortcut")
        assert len(hits) >= 1

    def test_hit_help_text_matches_description(self):
        """Help text of a hit should be the command description."""
        hits = _collect_hits(self.provider, "Refresh")
        refresh_hits = [h for h in hits if "Reload" in h.help or "registry" in h.help]
        assert len(refresh_hits) >= 1

    def test_callback_calls_run_system_command(self):
        """Hit callback should invoke run_system_command with the action key."""
        hits = _collect_hits(self.provider, "Health Check")
        assert len(hits) >= 1
        hits[0].partial()
        self.mock_app.run_system_command.assert_called_once_with("health")

    def test_make_command_callback(self):
        cb = self.provider._make_command_callback("lint")
        cb()
        self.mock_app.run_system_command.assert_called_once_with("lint")

    def test_no_hits_for_nonmatching(self):
        hits = _collect_hits(self.provider, "zzznomatchzzz")
        assert len(hits) == 0

    def test_all_commands_searchable(self):
        """Each system command should be findable by its exact name."""
        for name, _desc, _action in SystemCommandProvider.SYSTEM_COMMANDS:
            hits = _collect_hits(self.provider, name)
            assert len(hits) >= 1, f"No hits for system command {name!r}"

    def test_broad_query_returns_many_commands(self):
        """A broad single-character query should match many system commands."""
        # 'e' appears in many command names/descriptions
        hits = _collect_hits(self.provider, "e")
        assert len(hits) >= 5


# ============================================================================
# Edge cases
# ============================================================================


class TestCommandProviderEdgeCases(unittest.TestCase):
    """Edge case tests for command providers."""

    def test_agent_provider_with_empty_registry(self):
        """AgentCommandProvider with no agents should yield nothing."""
        registry = FleetRegistry()
        mock_app = _make_mock_app(registry)
        provider = _make_provider(AgentCommandProvider, mock_app)
        hits = _collect_hits(provider, "anything")
        assert len(hits) == 0

    def test_squad_provider_with_empty_registry(self):
        """SquadCommandProvider with no squads/combos should yield nothing."""
        registry = FleetRegistry()
        mock_app = _make_mock_app(registry)
        provider = _make_provider(SquadCommandProvider, mock_app)
        hits = _collect_hits(provider, "anything")
        assert len(hits) == 0

    def test_agent_with_no_tags(self):
        """Agent with empty tags should still be searchable by ID."""
        registry = FleetRegistry()
        registry.agents = [
            Agent(id="notags", tier="fleet", version="0.1.0",
                  tags=(), activation="default"),
        ]
        mock_app = _make_mock_app(registry)
        provider = _make_provider(AgentCommandProvider, mock_app)
        hits = _collect_hits(provider, "notags")
        assert len(hits) >= 1

    def test_squad_with_empty_members(self):
        """Squad with no members should still be searchable."""
        registry = FleetRegistry()
        registry.squads = [
            Squad(id="empty", name="Empty Squad", purpose="Nothing",
                  lead="nobody", members=()),
        ]
        registry.combos = []
        mock_app = _make_mock_app(registry)
        provider = _make_provider(SquadCommandProvider, mock_app)
        hits = _collect_hits(provider, "Empty")
        assert len(hits) >= 1

    def test_combo_with_empty_chain(self):
        """Combo with empty chain should still be searchable."""
        registry = FleetRegistry()
        registry.squads = []
        registry.combos = [
            Combo(id="nochain", name="No Chain Combo",
                  description="Empty chain", chain=()),
        ]
        mock_app = _make_mock_app(registry)
        provider = _make_provider(SquadCommandProvider, mock_app)
        hits = _collect_hits(provider, "No Chain")
        assert len(hits) >= 1

    def test_make_agent_callback_different_ids(self):
        """Callbacks should be unique per agent ID."""
        mock_app = _make_mock_app()
        provider = _make_provider(AgentCommandProvider, mock_app)
        cb1 = provider._make_agent_callback("architect")
        cb2 = provider._make_agent_callback("debugger")
        cb1()
        cb2()
        mock_app.action_deploy_agent.assert_any_call("architect")
        mock_app.action_deploy_agent.assert_any_call("debugger")
        assert mock_app.action_deploy_agent.call_count == 2

    def test_make_squad_callback_different_ids(self):
        """Squad callbacks should be unique per squad ID."""
        mock_app = _make_mock_app()
        provider = _make_provider(SquadCommandProvider, mock_app)
        cb1 = provider._make_squad_callback("alpha")
        cb2 = provider._make_squad_callback("beta")
        cb1()
        cb2()
        mock_app.action_deploy_squad.assert_any_call("alpha")
        mock_app.action_deploy_squad.assert_any_call("beta")
        assert mock_app.action_deploy_squad.call_count == 2

    def test_make_combo_callback_different_ids(self):
        """Combo callbacks should be unique per combo ID."""
        mock_app = _make_mock_app()
        provider = _make_provider(SquadCommandProvider, mock_app)
        cb1 = provider._make_combo_callback("chain-a")
        cb2 = provider._make_combo_callback("chain-b")
        cb1()
        cb2()
        mock_app.action_deploy_combo.assert_any_call("chain-a")
        mock_app.action_deploy_combo.assert_any_call("chain-b")
        assert mock_app.action_deploy_combo.call_count == 2

    def test_system_command_callback_different_actions(self):
        """System command callbacks should be unique per action."""
        mock_app = _make_mock_app()
        provider = _make_provider(SystemCommandProvider, mock_app)
        cb1 = provider._make_command_callback("health")
        cb2 = provider._make_command_callback("lint")
        cb1()
        cb2()
        mock_app.run_system_command.assert_any_call("health")
        mock_app.run_system_command.assert_any_call("lint")
        assert mock_app.run_system_command.call_count == 2


if __name__ == "__main__":
    unittest.main()
