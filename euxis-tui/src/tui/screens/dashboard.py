# SPDX-License-Identifier: MIT
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
from tui.widgets.fleet_grid import (
    AgentSelected,
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
    from tui.widgets.agent_card import AgentCard


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

    def action_focus_search(self) -> None:
        """Focus the search input."""
        grid = self.query_one(FleetGrid)
        try:
            search_input = grid.query_one("#fleet-filter-input")
            search_input.focus()
        except NoMatches:
            pass  # Search input not yet mounted
