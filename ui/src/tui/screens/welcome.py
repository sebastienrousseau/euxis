# (c) 2026 Euxis Fleet. All rights reserved.
"""Welcome splash screen — minimal, spacious, keyboard-first."""

from __future__ import annotations

from typing import TYPE_CHECKING

from textual.containers import Center, Middle
from textual.screen import Screen
from textual.widgets import Static

from tui.i18n import _
from tui.widgets.shortcut_bar import ShortcutBar

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.app import EuxisApp

SPLASH_LOGO = """\
[bold]
  ███████╗██╗   ██╗██╗  ██╗██╗███████╗
  ██╔════╝██║   ██║╚██╗██╔╝██║██╔════╝
  █████╗  ██║   ██║ ╚███╔╝ ██║███████╗
  ██╔══╝  ██║   ██║ ██╔██╗ ██║╚════██║
  ███████╗╚██████╔╝██╔╝ ██╗██║███████║
  ╚══════╝ ╚═════╝ ╚═╝  ╚═╝╚═╝╚══════╝
[/]
"""


class WelcomeScreen(Screen[None]):
    """Welcome splash — logo, stats, prompt."""

    DEFAULT_CSS = """
    WelcomeScreen {
        align: center middle;
        background: $surface;
    }

    #welcome-logo {
        content-align: center middle;
        width: auto;
        height: auto;
        color: $accent;
    }

    #welcome-stats {
        content-align: center middle;
        color: $text-muted;
        margin: 1 0 3 0;
        width: auto;
        text-align: center;
    }

    #welcome-prompt {
        content-align: center middle;
        color: $text-muted;
        width: auto;
        text-align: center;
    }
    """

    BINDINGS = [
        ("enter", "go_dashboard", "Dashboard"),
        ("ctrl+k", "app.command_palette", "Commands"),
        ("escape", "go_dashboard", "Back"),
    ]

    @property
    def euxis_app(self) -> EuxisApp:
        """Return the typed application instance."""
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        """Build the welcome splash screen layout."""
        with Middle(), Center():
            yield Static(SPLASH_LOGO, id="welcome-logo")
            yield Static(id="welcome-stats")
            yield Static(id="welcome-prompt")
        yield ShortcutBar()

    def on_mount(self) -> None:
        """Display version and fleet statistics."""
        reg = self.euxis_app.fleet_registry

        stats = self.query_one("#welcome-stats", Static)
        stats.update(
            f"[dim]v{reg.version}  ·  "
            f"{len(reg.agents)} {_('agents')}  ·  "
            f"{len(reg.squads)} {_('squads')}  ·  "
            f"{len(reg.combos)} {_('combos')}[/]"
        )

        prompt = self.query_one("#welcome-prompt", Static)
        prompt.update(
            f"[dim]{_('Press')} [/]"
            f"[bold]Enter[/]"
            f"[dim] {_('to begin')}  ·  [/]"
            f"[bold]Ctrl+K[/]"
            f"[dim] {_('for commands')}[/]"
        )

    def action_go_dashboard(self) -> None:
        """Dismiss welcome and show the main dashboard."""
        self.dismiss()
