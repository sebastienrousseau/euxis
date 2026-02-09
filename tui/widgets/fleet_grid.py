# (c) 2026 Euxis Fleet. All rights reserved.
"""Fleet grid widget showing all agents organized by tier."""

from __future__ import annotations

from typing import TYPE_CHECKING, Any

from textual.containers import Container, Horizontal, VerticalScroll
from textual.reactive import reactive
from textual.widget import Widget
from textual.widgets import Input, Static

from tui.widgets.agent_card import AgentCard

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.core.registry import Agent, FleetRegistry


class FleetGrid(Widget):
    """Grid display of the full agent fleet, organized by tier."""

    DEFAULT_CSS = """
    FleetGrid {
        height: 1fr;
        layout: vertical;
    }
    """

    filter_text: reactive[str] = reactive("")

    def __init__(self, registry: FleetRegistry, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.registry = registry

    def compose(self) -> ComposeResult:
        """Build the fleet grid layout with search and scroll area."""
        with Container(id="fleet-search"):
            yield Input(
                placeholder="Search agents by name or tag...",
                id="fleet-filter-input",
            )
        yield VerticalScroll(id="fleet-scroll")

    def on_mount(self) -> None:
        """Build the initial agent grid."""
        self._rebuild_grid()

    def on_input_changed(self, event: Input.Changed) -> None:
        """Update filter text when search input changes."""
        if event.input.id == "fleet-filter-input":
            self.filter_text = event.value

    def watch_filter_text(self) -> None:
        """Rebuild the grid when the filter text changes."""
        self._rebuild_grid()

    def _matches_filter(self, agent: Agent) -> bool:
        if not self.filter_text:
            return True
        query = self.filter_text.lower()
        return (
            query in agent.id.lower()
            or any(query in t.lower() for t in agent.tags)
            or query in agent.tier.lower()
            or query in agent.activation.lower()
        )

    def _rebuild_grid(self) -> None:
        scroll = self.query_one("#fleet-scroll", VerticalScroll)
        scroll.remove_children()

        sections = [
            ("Core Agents", self.registry.core_agents, "core"),
            ("Default Agents", self.registry.default_agents, "default"),
            ("On-Demand Agents", self.registry.ondemand_agents, "ondemand"),
            ("Specialist Agents", self.registry.specialist_agents, "specialist"),
        ]

        for title, agents, _section_id in sections:
            filtered = [a for a in agents if self._matches_filter(a)]
            if not filtered:
                continue

            label = Static(
                f"[bold cyan]{title}[/] [dim]({len(filtered)})[/]",
                classes="section-title",
            )
            scroll.mount(label)

            # Build rows of 4 cards
            for i in range(0, len(filtered), 4):
                row_agents = filtered[i : i + 4]
                row = Horizontal(classes="fleet-row")
                scroll.mount(row)
                for agent in row_agents:
                    card = AgentCard(agent, id=f"card-{agent.id}")
                    row.mount(card)

    def focus_first_card(self) -> None:
        """Focus the first agent card."""
        cards = self.query(AgentCard)
        if cards:
            cards.first().focus()
