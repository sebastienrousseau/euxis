# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""About screen with Liquid Glass branding and system information."""

from __future__ import annotations

import platform
import sys
from typing import TYPE_CHECKING

from textual.containers import Center, VerticalScroll
from textual.screen import Screen
from textual.widgets import Static

from tui.i18n import _
from tui.widgets.header import ETXHeader
from tui.widgets.shortcut_bar import ShortcutBar

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.app import EuxisApp

EUXIS_LOGO = """\
[bold]
  ███████╗██╗   ██╗██╗  ██╗██╗███████╗
  ██╔════╝██║   ██║╚██╗██╔╝██║██╔════╝
  █████╗  ██║   ██║ ╚███╔╝ ██║███████╗
  ██╔══╝  ██║   ██║ ██╔██╗ ██║╚════██║
  ███████╗╚██████╔╝██╔╝ ██╗██║███████║
  ╚══════╝ ╚═════╝ ╚═╝  ╚═╝╚═╝╚══════╝
[/]
"""


class AboutScreen(Screen[None]):
    """About screen with system info and version details."""

    BINDINGS = [
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
        yield ShortcutBar()

    def on_mount(self) -> None:
        """Populate system information display on mount."""
        header = self.query_one(ETXHeader)
        header.project = _("About")

        reg = self.euxis_app.fleet_registry
        config = self.euxis_app.config

        info = self.query_one("#about-info", Static)
        info.update(
            f"[bold]Enterprise Unified eXecution Intelligence System[/]\n"
            f"[bold]ETX[/] [dim]{_('Terminal Experience')}[/]\n\n"
            f"[bold italic]{_('Build something that matters.')}[/]\n\n"
            f"[dim]{'─' * 40}[/]\n\n"
            f"[bold]{_('Version')}[/]        {reg.version}\n"
            f"[bold]{_('Agents')}[/]         {len(reg.agents)}\n"
            f"[bold]{_('Squads')}[/]         {len(reg.squads)}\n"
            f"[bold]{_('Combos')}[/]         {len(reg.combos)}\n"
            f"[bold]{_('Theme')}[/]          {config.theme}\n"
            f"[bold]{_('Provider')}[/]       {config.default_provider}\n"
            f"[bold]Python[/]         {sys.version.split()[0]}\n"
            f"[bold]{_('Platform')}[/]       {platform.system()} {platform.release()}\n"
            f"[bold]Terminal[/]        Textual 7.5\n\n"
            f"[dim]{'─' * 40}[/]\n\n"
            f"[dim]{_('Designed by Sebastien Rousseau')}[/]\n"
            f"[dim]{_('Engineered with Euxis')}[/]\n"
        )

    def action_go_back(self) -> None:
        """Return to the previous screen."""
        self.app.pop_screen()
