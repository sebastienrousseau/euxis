# (c) 2026 Euxis Fleet. All rights reserved.
"""About screen with system information and branding."""

from __future__ import annotations

import platform
import sys
from typing import TYPE_CHECKING, ClassVar

from textual.binding import Binding
from textual.containers import Center, VerticalScroll
from textual.screen import Screen
from textual.widgets import Footer, Static

from tui.widgets.header import ETXHeader

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.app import EuxisApp

EUXIS_LOGO = """\
[bold cyan]
  ███████╗██╗   ██╗██╗  ██╗██╗███████╗
  ██╔════╝██║   ██║╚██╗██╔╝██║██╔════╝
  █████╗  ██║   ██║ ╚███╔╝ ██║███████╗
  ██╔══╝  ██║   ██║ ██╔██╗ ██║╚════██║
  ███████╗╚██████╔╝██╔╝ ██╗██║███████║
  ╚══════╝ ╚═════╝ ╚═╝  ╚═╝╚═╝╚══════╝
[/]
"""


class AboutScreen(Screen):
    """About screen with system info and version details."""

    BINDINGS: ClassVar[list[Binding]] = [
        ("escape", "go_back", "Back"),
        ("q", "go_back", "Close"),
    ]

    @property
    def euxis_app(self) -> EuxisApp:
        """Return the typed application instance."""
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        """Build the about screen layout."""
        yield ETXHeader(id="header")
        with VerticalScroll():
            with Center():
                yield Static(EUXIS_LOGO, id="about-logo")
            yield Static(id="about-info")
        yield Footer()

    def on_mount(self) -> None:
        """Populate system information display on mount."""
        header = self.query_one(ETXHeader)
        header.project = "About"

        reg = self.euxis_app.fleet_registry
        config = self.euxis_app.config

        info = self.query_one("#about-info", Static)
        info.update(
            f"[bold]Enterprise Unified eXecution Intelligence System[/]\n\n"
            f"[bold cyan]ETX[/] [dim]Terminal Experience[/]\n\n"
            f"[bold]Version[/]        {reg.version}\n"
            f"[bold]Agents[/]         {len(reg.agents)}\n"
            f"[bold]Squads[/]         {len(reg.squads)}\n"
            f"[bold]Combos[/]         {len(reg.combos)}\n"
            f"[bold]Theme[/]          {config.theme}\n"
            f"[bold]Provider[/]       {config.default_provider}\n"
            f"[bold]Python[/]         {sys.version.split()[0]}\n"
            f"[bold]Platform[/]       {platform.system()} {platform.release()}\n"
            f"[bold]Terminal[/]        Textual 7.5\n\n"
            f"[dim]Designed by Sebastien Rousseau[/]\n"
            f"[dim]Engineered with Euxis[/]\n\n"
            f"[bold]Build something that matters.[/]"
        )

    def action_go_back(self) -> None:
        """Return to the previous screen."""
        self.app.pop_screen()
