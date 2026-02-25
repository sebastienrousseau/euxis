# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Command palette providers for ETX.

Implements the quick-action pattern system:
  @ agent   — Deploy an agent
  # squad   — Deploy a squad
  > command — Run a system command
  ? help    — Documentation lookup
  ! tool    — Utility execution
"""

from __future__ import annotations

from collections.abc import Callable
from typing import TYPE_CHECKING, ClassVar

from textual.command import Hit, Hits, Provider

from tui.i18n import _

if TYPE_CHECKING:
    from tui.app import EuxisApp


class AgentCommandProvider(Provider):
    """Command provider for deploying agents via @ prefix."""

    @property
    def _app(self) -> EuxisApp:
        return self.screen.app  # type: ignore[return-value]

    async def search(self, query: str) -> Hits:
        """Search agents by name and tags."""
        registry = self._app.fleet_registry
        matcher = self.matcher(query)

        for agent in registry.agents:
            # Match against agent ID and tags
            search_text = f"{agent.id} {' '.join(agent.tags)}"
            match = matcher.match(search_text)
            if match > 0:
                tier_badge = "CORE" if agent.tier == "core" else "FLEET"
                yield Hit(
                    match,
                    matcher.highlight(agent.id),
                    self._make_agent_callback(agent.id),
                    help=f"[{tier_badge}] {', '.join(agent.tags[:3])}",
                )

    def _make_agent_callback(self, agent_id: str) -> Callable[[], None]:
        def callback() -> None:
            self._app.action_deploy_agent(agent_id)
        return callback


class SquadCommandProvider(Provider):
    """Command provider for deploying squads via # prefix."""

    @property
    def _app(self) -> EuxisApp:
        return self.screen.app  # type: ignore[return-value]

    async def search(self, query: str) -> Hits:
        """Search squads and combos by name and purpose."""
        registry = self._app.fleet_registry
        matcher = self.matcher(query)

        for squad in registry.squads:
            search_text = f"{squad.name} {squad.purpose}"
            match = matcher.match(search_text)
            if match > 0:
                members_str = ", ".join(squad.members[:4])
                yield Hit(
                    match,
                    matcher.highlight(squad.name),
                    self._make_squad_callback(squad.id),
                    help=f"{squad.purpose} — {members_str}",
                )

        for combo in registry.combos:
            search_text = f"{combo.name} {combo.description}"
            match = matcher.match(search_text)
            if match > 0:
                chain_str = " → ".join(combo.chain)
                yield Hit(
                    match,
                    matcher.highlight(f"{combo.name} ({_('combo')})"),
                    self._make_combo_callback(combo.id),
                    help=f"{combo.description} — {chain_str}",
                )

    def _make_squad_callback(self, squad_id: str) -> Callable[[], None]:
        def callback() -> None:
            self._app.action_deploy_squad(squad_id)
        return callback

    def _make_combo_callback(self, combo_id: str) -> Callable[[], None]:
        def callback() -> None:
            self._app.action_deploy_combo(combo_id)
        return callback


class SystemCommandProvider(Provider):
    """Command provider for system commands via > prefix."""

    SYSTEM_COMMANDS: ClassVar[list[tuple[str, str, str]]] = [
        # Core system commands
        ("Health Check", "Run euxis-health to verify system status", "health"),
        ("Certification", "Run euxis-certify to validate all gates", "certify"),
        ("Lint Fleet", "Run euxis-lint to check fleet integrity", "lint"),
        ("Refresh", "Reload fleet registry", "refresh"),

        # Router commands (cost optimization)
        ("Router Status", "Show model routing configuration and costs", "router_stats"),
        ("Router Benchmark", "Estimate costs for benchmark mission", "router_benchmark"),
        ("Router Config", "View router.json configuration", "router_config"),

        # Mesh commands (A2A communication) — Full mesh.sh primitives
        ("Mesh Status", "Show mesh network state and online agents", "mesh_status"),
        ("Mesh Discover", "Find agents by capability tag", "mesh_discover"),
        ("Mesh Online Agents", "List all currently online agents", "mesh_list_online"),
        ("Mesh Inbox", "Check pending messages for current session", "mesh_inbox"),
        ("Mesh Heartbeat", "Update agent heartbeat timestamp", "mesh_heartbeat"),
        ("Mesh Cleanup", "Remove old messages and offline agents", "mesh_cleanup"),
        ("Mesh State", "View shared mesh state JSON", "mesh_state"),
        ("Mesh Deadlock Check", "Detect circular waits between agents", "mesh_deadlock"),

        # Dispatch commands (from euxis-dispatch)
        ("Dispatch Mission", "Launch a mission via dispatch", "dispatch_mission"),
        ("Dispatch Playbook", "Execute a playbook via dispatch", "dispatch_playbook"),

        # Resource commands
        ("Resource Status", "Show CPU/RAM/thermal status", "resource_status"),
        ("Resource Throttle", "View thermal throttling status", "resource_throttle"),

        # Navigation
        ("Toggle Theme", "Switch between dark/light/contrast themes", "toggle_theme"),
        ("Settings", "Open settings panel", "settings"),
        ("Fleet Monitor", "Monitor active agent execution", "fleet_monitor"),
        ("View Logs", "Browse agent output logs", "view_logs"),
        ("Help", "Open keyboard shortcut reference", "help"),
        ("About", "View system information and version", "about"),
        ("Playbooks", "Browse and inspect playbooks", "playbooks"),
        ("Metrics", "View performance metrics dashboard", "metrics"),
        ("Cortex Memory", "Browse semantic memory system", "cortex"),
        ("Cortex Status", "Check semantic memory status", "cortex_status"),
        ("Sync Docs", "Synchronize documentation", "sync_docs"),
        ("Squad & Combo Details", "View squad compositions and combo chains", "squad_detail"),

        # Focus/Zoom commands
        ("Maximize Pane", "Toggle fullscreen for focused pane (z)", "maximize_pane"),
        ("Focus Next", "Move focus to next pane (Tab/l)", "focus_next"),
        ("Focus Prev", "Move focus to previous pane (Shift+Tab/h)", "focus_prev"),

        # Durable execution
        ("Time-Travel Debugger", "Replay mission timeline, recover from checkpoints", "time_travel"),
        ("Mission History", "List all recorded missions", "mission_history"),
    ]

    @property
    def _app(self) -> EuxisApp:
        return self.screen.app  # type: ignore[return-value]

    async def search(self, query: str) -> Hits:
        """Search system commands by name and description."""
        matcher = self.matcher(query)

        for name, description, action in self.SYSTEM_COMMANDS:
            match = matcher.match(f"{_(name)} {_(description)}")
            if match > 0:
                yield Hit(
                    match,
                    matcher.highlight(_(name)),
                    self._make_command_callback(action),
                    help=_(description),
                )

    def _make_command_callback(self, action: str) -> Callable[[], None]:
        def callback() -> None:
            self._app.run_system_command(action)
        return callback
