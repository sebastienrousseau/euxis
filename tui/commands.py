# (c) 2026 Euxis Fleet. All rights reserved.
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
                    partial=self._make_agent_callback(agent.id),
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
                    partial=self._make_squad_callback(squad.id),
                    help=f"{squad.purpose} — {members_str}",
                )

        for combo in registry.combos:
            search_text = f"{combo.name} {combo.description}"
            match = matcher.match(search_text)
            if match > 0:
                chain_str = " → ".join(combo.chain)
                yield Hit(
                    match,
                    matcher.highlight(f"{combo.name} (combo)"),
                    partial=self._make_combo_callback(combo.id),
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
        ("Health Check", "Run euxis-health to verify system status", "health"),
        ("Certification", "Run euxis-certify to validate all gates", "certify"),
        ("Lint Fleet", "Run euxis-lint to check fleet integrity", "lint"),
        ("Cortex Status", "Check semantic memory status", "cortex_status"),
        ("Sync Docs", "Synchronize documentation", "sync_docs"),
        ("Toggle Theme", "Switch between dark/light/contrast themes", "toggle_theme"),
        ("Settings", "Open settings panel", "settings"),
        ("Fleet Monitor", "Monitor active agent execution", "fleet_monitor"),
        ("View Logs", "Browse agent output logs", "view_logs"),
        ("Help", "Open keyboard shortcut reference", "help"),
        ("About", "View system information and version", "about"),
        ("Playbooks", "Browse and inspect playbooks", "playbooks"),
        ("Metrics", "View performance metrics dashboard", "metrics"),
        ("Cortex Memory", "Browse semantic memory system", "cortex"),
        ("Squad & Combo Details", "View squad compositions and combo chains", "squad_detail"),
        ("Refresh", "Reload fleet registry", "refresh"),
    ]

    @property
    def _app(self) -> EuxisApp:
        return self.screen.app  # type: ignore[return-value]

    async def search(self, query: str) -> Hits:
        """Search system commands by name and description."""
        matcher = self.matcher(query)

        for name, description, action in self.SYSTEM_COMMANDS:
            match = matcher.match(f"{name} {description}")
            if match > 0:
                yield Hit(
                    match,
                    matcher.highlight(name),
                    partial=self._make_command_callback(action),
                    help=description,
                )

    def _make_command_callback(self, action: str) -> Callable[[], None]:
        def callback() -> None:
            self._app.run_system_command(action)
        return callback
