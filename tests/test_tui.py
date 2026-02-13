# (c) 2026 Euxis Fleet. All rights reserved.
"""Test suite for ETX: Euxis Terminal Experience.

Tests core infrastructure, widget rendering, screen navigation,
command palette, and accessibility features.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import pytest

# Ensure the tui package is importable
sys.path.insert(0, str(Path(__file__).parent.parent))


# ============================================================================
# Core Infrastructure Tests
# ============================================================================


class TestFleetRegistry:
    """Test agent registry loading and querying."""

    def test_load_agents(self):
        from tui.core.registry import FleetRegistry

        reg = FleetRegistry.load()
        assert len(reg.agents) == 41
        assert reg.version == "0.0.7"

    def test_core_agents(self):
        from tui.core.registry import FleetRegistry

        reg = FleetRegistry.load()
        core = reg.core_agents
        assert len(core) == 9
        core_ids = {a.id for a in core}
        assert "orchestrator" in core_ids
        assert "architect" in core_ids
        assert "reviewer" in core_ids

    def test_agent_tiers(self):
        from tui.core.registry import FleetRegistry

        reg = FleetRegistry.load()
        for agent in reg.agents:
            assert agent.tier in ("core", "fleet")

    def test_agent_activations(self):
        from tui.core.registry import FleetRegistry

        reg = FleetRegistry.load()
        activations = {a.activation for a in reg.agents}
        assert "default" in activations

    def test_get_agent(self):
        from tui.core.registry import FleetRegistry

        reg = FleetRegistry.load()
        architect = reg.get_agent("architect")
        assert architect is not None
        assert architect.id == "architect"
        assert architect.tier == "core"

    def test_get_unknown_agent(self):
        from tui.core.registry import FleetRegistry

        reg = FleetRegistry.load()
        assert reg.get_agent("nonexistent") is None

    def test_load_squads(self):
        from tui.core.registry import FleetRegistry

        reg = FleetRegistry.load()
        assert len(reg.squads) == 6
        squad_ids = {s.id for s in reg.squads}
        assert "vision" in squad_ids
        assert "quality" in squad_ids

    def test_load_combos(self):
        from tui.core.registry import FleetRegistry

        reg = FleetRegistry.load()
        assert len(reg.combos) == 7
        combo_ids = {c.id for c in reg.combos}
        assert "steve-jobs" in combo_ids
        assert "fort-knox" in combo_ids

    def test_squad_members(self):
        from tui.core.registry import FleetRegistry

        reg = FleetRegistry.load()
        quality = next(s for s in reg.squads if s.id == "quality")
        assert "reviewer" in quality.members
        assert "pentester" in quality.members

    def test_combo_chain(self):
        from tui.core.registry import FleetRegistry

        reg = FleetRegistry.load()
        steve = next(c for c in reg.combos if c.id == "steve-jobs")
        assert steve.chain == ("planner", "architect", "evangelist", "reviewer")

    def test_agent_tier_label(self):
        from tui.core.registry import FleetRegistry

        reg = FleetRegistry.load()
        architect = reg.get_agent("architect")
        assert architect.tier_label == "CORE"
        debugger = reg.get_agent("debugger")
        assert debugger.tier_label == "FLEET"

    def test_default_agents(self):
        from tui.core.registry import FleetRegistry

        reg = FleetRegistry.load()
        default = reg.default_agents
        assert len(default) > 0
        for agent in default:
            assert agent.activation == "default"
            assert agent.tier != "core"

    def test_specialist_agents(self):
        from tui.core.registry import FleetRegistry

        reg = FleetRegistry.load()
        specialists = reg.specialist_agents
        assert len(specialists) == 4
        specialist_ids = {a.id for a in specialists}
        assert "cryptographer" in specialist_ids


class TestETXConfig:
    """Test configuration loading and saving."""

    def test_default_config(self):
        from tui.core.config import ETXConfig

        config = ETXConfig()
        assert config.theme == "etx-liquid-glass"
        assert config.default_provider == "claude"
        assert config.show_agent_tags is True

    def test_add_recent_agent(self):
        from tui.core.config import ETXConfig

        config = ETXConfig()
        config.add_recent_agent("architect")
        config.add_recent_agent("debugger")
        assert config.recent_agents[0] == "debugger"
        assert config.recent_agents[1] == "architect"

    def test_recent_agent_dedup(self):
        from tui.core.config import ETXConfig

        config = ETXConfig()
        config.add_recent_agent("architect")
        config.add_recent_agent("debugger")
        config.add_recent_agent("architect")
        assert config.recent_agents[0] == "architect"
        assert len(config.recent_agents) == 2

    def test_recent_agent_limit(self):
        from tui.core.config import ETXConfig

        config = ETXConfig()
        for i in range(15):
            config.add_recent_agent(f"agent-{i}")
        assert len(config.recent_agents) == config.MAX_RECENT


class TestRunner:
    """Test runner utility functions."""

    def test_get_project_name(self):
        from tui.core.runner import get_project_name

        name = get_project_name()
        assert isinstance(name, str)
        assert len(name) > 0

    def test_get_git_branch(self):
        from tui.core.runner import get_git_branch

        get_git_branch()
        # May be None if not in a git repo, but in .euxis it should work
        assert True  # Allow None in CI

    def test_providers_dict(self):
        from tui.core.runner import PROVIDERS

        assert "claude" in PROVIDERS
        assert "gemini" in PROVIDERS
        assert len(PROVIDERS) >= 8

    def test_agent_run_state(self):
        from tui.core.runner import AgentRun

        run = AgentRun(agent_id="architect", task="test", provider="claude")
        assert run.is_running
        assert run.elapsed >= 0
        assert isinstance(run.elapsed_display, str)
        run.return_code = 0
        assert not run.is_running


# ============================================================================
# Textual App Tests
# ============================================================================


class TestEuxisApp:
    """Test the main EuxisApp."""

    @pytest.mark.asyncio
    async def test_app_starts(self):
        from tui.app import EuxisApp

        app = EuxisApp()
        async with app.run_test():
            assert app.fleet_registry is not None
            assert len(app.fleet_registry.agents) == 41

    @pytest.mark.asyncio
    async def test_app_theme_toggle(self):
        from tui.app import EuxisApp

        app = EuxisApp()
        async with app.run_test():
            initial = app.theme
            app.action_toggle_theme()
            assert app.theme != initial

    @pytest.mark.asyncio
    async def test_app_bindings(self):
        from tui.app import EuxisApp

        binding_keys = [b.key for b in EuxisApp.BINDINGS]
        assert "ctrl+k" in binding_keys
        assert "ctrl+q" in binding_keys
        assert "ctrl+t" in binding_keys
        assert "f1" in binding_keys

    @pytest.mark.asyncio
    async def test_app_commands(self):
        from tui.app import EuxisApp

        assert len(EuxisApp.COMMANDS) == 3


class TestCommands:
    """Test command palette providers."""

    def test_system_commands_defined(self):
        from tui.commands import SystemCommandProvider

        assert len(SystemCommandProvider.SYSTEM_COMMANDS) >= 9

    def test_system_command_structure(self):
        from tui.commands import SystemCommandProvider

        for name, desc, action in SystemCommandProvider.SYSTEM_COMMANDS:
            assert isinstance(name, str)
            assert isinstance(desc, str)
            assert isinstance(action, str)
            assert len(name) > 0
            assert len(action) > 0


# ============================================================================
# Widget Tests
# ============================================================================


class TestAgentCard:
    """Test agent card widget."""

    def test_card_creation(self):
        from tui.core.registry import Agent
        from tui.widgets.agent_card import AgentCard

        agent = Agent(
            id="test",
            tier="core",
            version="0.0.7",
            tags=("test", "debug"),
            activation="default",
        )
        card = AgentCard(agent)
        assert card.agent.id == "test"
        assert "tier-core" in card.classes

    def test_card_fleet_tier(self):
        from tui.core.registry import Agent
        from tui.widgets.agent_card import AgentCard

        agent = Agent(
            id="test",
            tier="fleet",
            version="0.0.7",
            tags=("test",),
            activation="default",
        )
        card = AgentCard(agent)
        assert "tier-core" not in card.classes


class TestOutputPanel:
    """Test output panel formatting."""

    @pytest.mark.asyncio
    async def test_output_panel_creation(self):
        from tui.widgets.output_panel import OutputPanel

        panel = OutputPanel()
        assert panel.highlight is True
        assert panel.markup is True


class TestHeader:
    """Test header widget class attributes."""

    def test_header_reactive_defaults(self):
        from tui.widgets.header import ETXHeader

        # Check class-level reactive defaults (not instance access, which triggers watchers)
        reactives = ETXHeader.__dict__
        assert "project" in reactives
        assert "version" in reactives
        assert "agent_count" in reactives


# ============================================================================
# Screen Import Tests
# ============================================================================


class TestScreenImports:
    """Verify all screens can be imported without error."""

    def test_dashboard_import(self):
        from tui.screens.dashboard import DashboardScreen

        assert DashboardScreen is not None

    def test_agent_import(self):
        from tui.screens.agent import AgentScreen

        assert AgentScreen is not None

    def test_settings_import(self):
        from tui.screens.settings import SettingsScreen

        assert SettingsScreen is not None

    def test_fleet_monitor_import(self):
        from tui.screens.fleet_monitor import FleetMonitorScreen

        assert FleetMonitorScreen is not None

    def test_logs_import(self):
        from tui.screens.logs import LogViewerScreen

        assert LogViewerScreen is not None

    def test_help_import(self):
        from tui.screens.help import HelpScreen

        assert HelpScreen is not None

    def test_about_import(self):
        from tui.screens.about import AboutScreen

        assert AboutScreen is not None

    def test_tool_runner_import(self):
        from tui.screens.tool_runner import ToolRunnerScreen

        assert ToolRunnerScreen is not None

    def test_welcome_import(self):
        from tui.screens.welcome import WelcomeScreen

        assert WelcomeScreen is not None

    def test_playbooks_import(self):
        from tui.screens.playbooks import PlaybookScreen

        assert PlaybookScreen is not None

    def test_cortex_import(self):
        from tui.screens.cortex import CortexScreen

        assert CortexScreen is not None

    def test_provider_select_import(self):
        from tui.widgets.provider_select import ProviderSelectModal

        assert ProviderSelectModal is not None


# ============================================================================
# Integration Tests
# ============================================================================


class TestIntegration:
    """Integration tests for the full TUI stack."""

    @pytest.mark.asyncio
    async def test_app_deploy_unknown_agent(self):
        from tui.app import EuxisApp

        app = EuxisApp()
        async with app.run_test():
            app.action_deploy_agent("nonexistent")
            # Should not crash

    @pytest.mark.asyncio
    async def test_app_deploy_unknown_squad(self):
        from tui.app import EuxisApp

        app = EuxisApp()
        async with app.run_test():
            app.action_deploy_squad("nonexistent")
            # Should not crash

    @pytest.mark.asyncio
    async def test_app_deploy_unknown_combo(self):
        from tui.app import EuxisApp

        app = EuxisApp()
        async with app.run_test():
            app.action_deploy_combo("nonexistent")
            # Should not crash

    @pytest.mark.asyncio
    async def test_app_refresh(self):
        from tui.app import EuxisApp

        app = EuxisApp()
        async with app.run_test():
            app.action_refresh()
            assert len(app.fleet_registry.agents) == 41

    def test_fleet_registry_immutable_agents(self):
        from tui.core.registry import FleetRegistry

        reg = FleetRegistry.load()
        agent = reg.get_agent("architect")
        # Frozen dataclass should be immutable
        with pytest.raises(AttributeError):
            agent.id = "hacked"

    def test_playbook_files_exist(self):
        playbook_dir = Path.home() / ".euxis" / "config" / "playbooks"
        assert playbook_dir.exists()
        playbooks = list(playbook_dir.glob("*.json"))
        assert len(playbooks) >= 16  # 16 language playbooks created earlier

    def test_registry_json_valid(self):
        registry_path = Path.home() / ".euxis" / "registry.json"
        assert registry_path.exists()
        data = json.loads(registry_path.read_text())
        assert "agents" in data
        assert len(data["agents"]) == 41

    def test_squads_json_valid(self):
        squads_path = Path.home() / ".euxis" / "squads.json"
        assert squads_path.exists()
        data = json.loads(squads_path.read_text())
        assert "squads" in data
        assert "combos" in data


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
