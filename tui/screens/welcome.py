# (c) 2026 Euxis Fleet. All rights reserved.
"""Welcome splash screen with animated logo and quick actions."""

from __future__ import annotations

from typing import TYPE_CHECKING

from textual.app import ComposeResult
from textual.containers import Center, Container, Horizontal, Vertical
from textual.screen import Screen
from textual.widgets import Button, Footer, Static

if TYPE_CHECKING:
    from tui.app import EuxisApp

SPLASH_LOGO = """\
[bold cyan]
  ███████╗██╗   ██╗██╗  ██╗██╗███████╗
  ██╔════╝██║   ██║╚██╗██╔╝██║██╔════╝
  █████╗  ██║   ██║ ╚███╔╝ ██║███████╗
  ██╔══╝  ██║   ██║ ██╔██╗ ██║╚════██║
  ███████╗╚██████╔╝██╔╝ ██╗██║███████║
  ╚══════╝ ╚═════╝ ╚═╝  ╚═╝╚═╝╚══════╝
[/]
[bold]Enterprise Unified eXecution Intelligence System[/]
[dim]41 AI specialists. Deploy in seconds.[/]
"""


class WelcomeScreen(Screen):
    """Welcome splash with quick actions for first-time experience."""

    DEFAULT_CSS = """
    WelcomeScreen {
        align: center middle;
        background: $primary-darken-3;
    }

    #welcome-container {
        width: 60;
        height: auto;
        padding: 2 4;
        background: $surface;
        border: round $accent;
    }

    #welcome-logo {
        content-align: center middle;
        margin: 0 0 1 0;
    }

    #welcome-version {
        content-align: center middle;
        color: $text-muted;
        margin: 0 0 2 0;
    }

    #welcome-actions {
        height: auto;
        align: center middle;
    }

    #welcome-actions Button {
        margin: 0 1;
    }

    #welcome-hint {
        content-align: center middle;
        color: $text-disabled;
        margin: 2 0 0 0;
    }
    """

    BINDINGS = [
        ("enter", "go_dashboard", "Dashboard"),
        ("ctrl+k", "app.command_palette", "Commands"),
        ("escape", "app.quit", "Quit"),
    ]

    @property
    def euxis_app(self) -> EuxisApp:
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        with Center():
            with Container(id="welcome-container"):
                yield Static(SPLASH_LOGO, id="welcome-logo")
                yield Static(id="welcome-version")
                with Horizontal(id="welcome-actions"):
                    yield Button(
                        "Fleet Dashboard",
                        variant="primary",
                        id="btn-dashboard",
                    )
                    yield Button(
                        "Deploy Agent",
                        variant="default",
                        id="btn-agent",
                    )
                    yield Button(
                        "Help",
                        variant="default",
                        id="btn-help",
                    )
                yield Static(
                    "[dim]Press Enter or Ctrl+K to begin[/]",
                    id="welcome-hint",
                )
        yield Footer()

    def on_mount(self) -> None:
        reg = self.euxis_app.fleet_registry
        version = self.query_one("#welcome-version", Static)
        version.update(
            f"[dim]v{reg.version}  ·  "
            f"{len(reg.agents)} agents  ·  "
            f"{len(reg.squads)} squads  ·  "
            f"{len(reg.combos)} combos[/]"
        )

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "btn-dashboard":
            self.action_go_dashboard()
        elif event.button.id == "btn-agent":
            self.dismiss()
            self.euxis_app.action_deploy_agent("architect")
        elif event.button.id == "btn-help":
            self.dismiss()
            self.euxis_app.action_help()

    def action_go_dashboard(self) -> None:
        self.dismiss()
