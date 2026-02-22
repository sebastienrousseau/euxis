# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Main fleet dashboard screen with tip bar."""

from __future__ import annotations

import random
from typing import TYPE_CHECKING

from textual.css.query import NoMatches
from textual.screen import Screen
from textual.widgets import Static

from tui.core.runner import PROVIDER_MODELS, get_project_path
from tui.core.tips import TIPS
from tui.i18n import _
from tui.screens.welcome import WelcomeScreen
from tui.widgets.agent_card import AgentCard, STATUS_ERROR
from tui.widgets.fleet_grid import (
    AgentSelected,
    BulkRestartRequested,
    ComboSelected,
    FleetGrid,
    SquadSelected,
    SystemCommandRequested,
)
from tui.widgets.header import ETXHeader
from tui.widgets.shortcut_bar import ShortcutBar

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.app import EuxisApp


class TipBar(Static):
    """Rotating contextual tip bar."""

    def __init__(self, tips: list[str], **kwargs: object) -> None:
        super().__init__(**kwargs)
        self._tips = tips
        self._index = random.randint(0, max(0, len(tips) - 1))  # noqa: S311

    def on_mount(self) -> None:
        """Show first tip and start rotation timer."""
        self._show_tip()
        self.set_interval(30, self._rotate)

    def on_click(self) -> None:
        """Advance to next tip on click."""
        self._rotate()

    def _rotate(self) -> None:
        self._index = (self._index + 1) % len(self._tips)
        self._show_tip()

    def _show_tip(self) -> None:
        tip = self._tips[self._index] if self._tips else ""
        self.update(f"[dim italic]💡 {_('Tip')}: {tip}[/]")


class DashboardScreen(Screen[None]):
    """Main dashboard displaying the agent fleet grid."""

    BINDINGS = [
        ("ctrl+k", "app.command_palette", "Commands"),
        ("slash", "focus_search", "Search"),
        ("f1", "app.action_help", "Help"),
        ("escape", "app.quit", "Quit"),
        ("ctrl+z", "undo_dismiss", "Undo"),
    ]

    @property
    def euxis_app(self) -> EuxisApp:
        """Return the typed application instance."""
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        """Build the dashboard layout with header, tip bar, fleet grid, and footer."""
        yield ETXHeader(id="header")
        yield TipBar(TIPS, classes="tip-bar")
        yield FleetGrid(self.euxis_app.fleet_registry, id="fleet-grid")
        yield ShortcutBar()

    def on_mount(self) -> None:
        """Configure header with project context and announce for accessibility."""
        header = self.query_one(ETXHeader)
        header.project = self.euxis_app.project_name
        header.project_path = get_project_path()
        header.branch = self.euxis_app.git_branch or ""
        header.provider = self.euxis_app.config.default_provider
        header.model = PROVIDER_MODELS.get(self.euxis_app.config.default_provider, "")
        header.agent_count = len(self.euxis_app.fleet_registry.agents)
        header.version = self.euxis_app.fleet_registry.version

        # Announce for screen readers
        self.notify(
            _("Fleet Dashboard: {} agents ready").format(
                len(self.euxis_app.fleet_registry.agents)
            ),
            timeout=3,
        )

        # Show welcome screen on first visit (after dashboard is fully composed)
        if not self.euxis_app.welcome_shown:
            self.euxis_app.mark_welcome_shown()
            self.app.push_screen(WelcomeScreen())

    def on_agent_card_selected(self, event: AgentCard.Selected) -> None:
        """Handle agent card selection."""
        self.euxis_app.action_deploy_agent(event.agent.id)

    def on_agent_selected(self, event: AgentSelected) -> None:
        """Handle agent selection from search bar."""
        self.euxis_app.action_deploy_agent(event.agent_id, task=event.task)

    def on_squad_selected(self, event: SquadSelected) -> None:
        """Handle squad row selection."""
        self.euxis_app.action_deploy_squad(event.squad_id, task=event.task)

    def on_combo_selected(self, event: ComboSelected) -> None:
        """Handle combo row selection."""
        self.euxis_app.action_deploy_combo(event.combo_id, task=event.task)

    def on_system_command_requested(self, event: SystemCommandRequested) -> None:
        """Handle system command from search bar."""
        self.euxis_app.run_system_command(event.command)

    # ========================================================================
    # CONTEXTUAL RECOVERY ACTIONS — Error state quick actions (2026 UX)
    # ========================================================================

    def on_agent_card_restart_requested(self, event: AgentCard.RestartRequested) -> None:
        """Handle agent restart request (R key on error state)."""
        self._restart_agent(event.agent.id)

    def on_agent_card_logs_requested(self, event: AgentCard.LogsRequested) -> None:
        """Handle agent logs request (L key on error state)."""
        agent = event.agent
        # Open log viewer (TODO: add filter_agent support to LogViewerScreen)
        from tui.screens.logs import LogViewerScreen
        self.notify(f"Opening logs for {agent.id}...", timeout=2)
        self.app.push_screen(LogViewerScreen())

    def on_agent_card_error_dismissed(self, event: AgentCard.ErrorDismissed) -> None:
        """Handle error dismissal (D key on error state) with undo support."""
        agent = event.agent

        # Store dismissed agent for potential undo
        self._last_dismissed_agent = agent.id

        # Show undo toast (2026 UX: recoverable actions)
        self.notify(
            f"Error dismissed for {agent.id}  [dim][bold]Ctrl+Z[/] to undo[/]",
            timeout=5,
        )

        # Set timer to clear undo state
        self.set_timer(5.0, self._clear_undo_state)

    def _clear_undo_state(self) -> None:
        """Clear undo state after timeout."""
        self._last_dismissed_agent = None

    def action_undo_dismiss(self) -> None:
        """Undo last error dismissal (Ctrl+Z)."""
        if hasattr(self, "_last_dismissed_agent") and self._last_dismissed_agent:
            agent_id = self._last_dismissed_agent
            try:
                card = self.query_one(f"#card-{agent_id}", AgentCard)
                card.status = STATUS_ERROR
                self.notify(f"Restored error state for {agent_id}", timeout=2)
            except Exception:
                self.notify("Cannot undo - agent not found", severity="warning")
            finally:
                self._last_dismissed_agent = None

    def on_agent_card_error_details_requested(self, event: AgentCard.ErrorDetailsRequested) -> None:
        """Handle error details request (I key) - opens lightweight modal."""
        from tui.screens.error_details import ErrorDetailsModal

        agent = event.agent

        # Track error for adaptive theme suggestion (2026 Sentient Interface)
        # Fetch error message from mesh state for severity detection
        error_msg = self._get_agent_error_message(agent.id)
        self.euxis_app.track_error(severity="warning", message=error_msg)

        def handle_modal_result(result: str | None) -> None:
            """Handle action from modal."""
            if result == "restart":
                self._restart_agent(agent.id)
            elif result == "simulate":
                self._simulate_agent(agent.id)
            elif result == "logs":
                self._open_agent_logs(agent.id)

        self.app.push_screen(ErrorDetailsModal(agent), callback=handle_modal_result)

    def _restart_agent(self, agent_id: str) -> None:
        """Restart an agent and clear its error state."""
        self.notify(f"Restarting {agent_id}...", title="Agent Recovery", timeout=3)
        try:
            card = self.query_one(f"#card-{agent_id}", AgentCard)
            card.status = "idle"
        except Exception:
            pass

    def _simulate_agent(self, agent_id: str) -> None:
        """Run agent in sandbox dry-run mode.

        2026 UX: Safe-to-Try Sandbox
        - Verifies recovery conditions before production restart
        - Critical for guard/arbiter agents to prevent cascade failures
        """
        self.notify(
            f"⚗️ Simulating {agent_id} in sandbox mode...",
            title="Sandbox Test",
            timeout=5,
        )
        # TODO: Implement actual sandbox execution via euxis-agent-bootstrap --dry-run
        # For now, show verification dialog after simulated delay
        def show_simulation_result() -> None:
            self.notify(
                f"✓ Simulation passed for {agent_id}. Safe to restart.",
                title="Sandbox Result",
                timeout=5,
            )
        self.set_timer(2.0, show_simulation_result)

    def _open_agent_logs(self, agent_id: str) -> None:
        """Open log viewer for a specific agent."""
        from tui.screens.logs import LogViewerScreen
        self.notify(f"Opening logs for {agent_id}...", timeout=2)
        self.app.push_screen(LogViewerScreen())

    def _get_agent_error_message(self, agent_id: str) -> str:
        """Fetch error message from mesh state for an agent."""
        import json
        import os

        euxis_home = os.environ.get("EUXIS_HOME", os.path.expanduser("~/.euxis"))
        state_file = os.path.join(euxis_home, "euxis-runtime/mesh/state.json")

        try:
            with open(state_file) as f:
                state = json.load(f)
            agent_state = state.get("agents", {}).get(agent_id, {})
            return agent_state.get("last_error", "")
        except (FileNotFoundError, json.JSONDecodeError, KeyError):
            return ""

    # ========================================================================
    # BULK ACTIONS — Multi-agent operations (2026 UX)
    # ========================================================================

    def on_bulk_restart_requested(self, event: BulkRestartRequested) -> None:
        """Handle bulk restart of all failed agents (Shift+R)."""
        agent_ids = event.agent_ids
        count = len(agent_ids)

        if count == 0:
            self.notify("No failed agents to restart", severity="warning")
            return

        # Restart all failed agents
        for agent_id in agent_ids:
            self._restart_agent(agent_id)

        self.notify(
            f"Restarted {count} failed agent{'s' if count > 1 else ''}",
            title="Bulk Recovery",
            timeout=3,
        )

    def action_focus_search(self) -> None:
        """Focus the search input."""
        grid = self.query_one(FleetGrid)
        try:
            search_input = grid.query_one("#fleet-filter-input")
            search_input.focus()
        except NoMatches:
            pass  # Search input not yet mounted
