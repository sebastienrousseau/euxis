# (c) 2026 Euxis Fleet. All rights reserved.
"""Main fleet dashboard screen."""

from __future__ import annotations

from typing import TYPE_CHECKING

from textual.app import ComposeResult
from textual.css.query import NoMatches
from textual.screen import Screen
from textual.widgets import Footer, Static

from tui.widgets.agent_card import AgentCard
from tui.widgets.fleet_grid import FleetGrid
from tui.widgets.header import ETXHeader

if TYPE_CHECKING:
    from tui.app import EuxisApp


class DashboardScreen(Screen):
    """Main dashboard displaying the agent fleet grid."""

    BINDINGS = [
        ("ctrl+k", "app.command_palette", "Commands"),
        ("slash", "focus_search", "Search"),
        ("f1", "app.action_help", "Help"),
        ("escape", "app.pop_screen", "Back"),
    ]

    @property
    def euxis_app(self) -> EuxisApp:
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        yield ETXHeader(id="header")
        yield FleetGrid(self.euxis_app.fleet_registry, id="fleet-grid")
        yield Footer()

    def on_mount(self) -> None:
        header = self.query_one(ETXHeader)
        header.project = self.euxis_app.project_name
        header.branch = self.euxis_app.git_branch or ""
        header.provider = self.euxis_app.config.default_provider
        header.agent_count = len(self.euxis_app.fleet_registry.agents)
        header.version = self.euxis_app.fleet_registry.version

        # Announce for screen readers
        self.notify(
            f"Fleet Dashboard: {len(self.euxis_app.fleet_registry.agents)} agents ready",
            timeout=3,
        )

    def on_agent_card_selected(self, event: AgentCard.Selected) -> None:
        """Handle agent card selection."""
        self.euxis_app.action_deploy_agent(event.agent.id)

    def action_focus_search(self) -> None:
        """Focus the search input."""
        grid = self.query_one(FleetGrid)
        try:
            search_input = grid.query_one("#fleet-filter-input")
            search_input.focus()
        except NoMatches:
            pass  # Search input not yet mounted
