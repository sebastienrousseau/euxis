# (c) 2026 Euxis Fleet. All rights reserved.
"""Squad and combo detail screen with agent composition visualization."""

from __future__ import annotations

from typing import TYPE_CHECKING, ClassVar

from textual.binding import Binding
from textual.containers import Container, VerticalScroll
from textual.screen import Screen
from textual.widgets import Footer, Static

from tui.widgets.header import ETXHeader

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.app import EuxisApp


class SquadDetailScreen(Screen):
    """Detailed view of squads and combos with composition diagrams."""

    BINDINGS: ClassVar[list[Binding]] = [
        ("escape", "go_back", "Back"),
        ("ctrl+k", "app.command_palette", "Commands"),
    ]

    DEFAULT_CSS = """
    #squad-detail {
        layout: vertical;
        padding: 1 2;
    }

    .squad-card {
        height: auto;
        border: round $primary-background-darken-2;
        padding: 1 2;
        margin: 0 0 1 0;
        background: $surface;
    }

    .squad-card:hover {
        border: round $accent;
    }

    .squad-name {
        text-style: bold;
        color: $accent;
    }

    .squad-purpose {
        color: $text-muted;
    }

    .squad-members {
        margin: 1 0 0 0;
    }

    .combo-chain {
        color: $warning;
        margin: 0 0 1 0;
    }

    .deploy-row {
        height: 3;
        margin: 1 0 0 0;
    }
    """

    @property
    def euxis_app(self) -> EuxisApp:
        """Return the typed application instance."""
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        """Build the squad detail layout."""
        yield ETXHeader(id="header")
        with VerticalScroll(id="squad-detail"):
            yield Static(
                "[bold cyan]Squads[/] [dim]Parallel agent teams[/]",
                classes="section-title",
            )
            yield Container(id="squads-list")
            yield Static(
                "[bold cyan]Combos[/] [dim]Sequential agent chains[/]",
                classes="section-title",
            )
            yield Container(id="combos-list")
        yield Footer()

    def on_mount(self) -> None:
        """Populate squad and combo cards from registry."""
        header = self.query_one(ETXHeader)
        header.project = self.euxis_app.project_name
        header.branch = self.euxis_app.git_branch or ""

        reg = self.euxis_app.fleet_registry

        # Build squad cards
        squads_container = self.query_one("#squads-list", Container)
        for squad in reg.squads:
            member_icons = []
            for m in squad.members:
                agent = reg.get_agent(m)
                if agent and agent.tier == "core":
                    member_icons.append(f"[yellow]{m}[/]")
                else:
                    member_icons.append(f"[cyan]{m}[/]")

            lead_badge = f"[bold yellow]Lead: {squad.lead}[/]"
            members_str = "  ".join(member_icons)

            card = Static(
                f"[bold]{squad.name}[/]\n"
                f"[dim]{squad.purpose}[/]  {lead_badge}\n"
                f"Members: {members_str}",
                classes="squad-card",
            )
            squads_container.mount(card)

        # Build combo cards
        combos_container = self.query_one("#combos-list", Container)
        for combo in reg.combos:
            chain_display = " [bold dim]→[/] ".join(
                f"[cyan]{agent}[/]" for agent in combo.chain
            )

            card = Static(
                f"[bold]{combo.name}[/]\n"
                f"[dim]{combo.description}[/]\n"
                f"Chain: {chain_display}",
                classes="squad-card",
            )
            combos_container.mount(card)

    def action_go_back(self) -> None:
        """Return to the previous screen."""
        self.app.pop_screen()
