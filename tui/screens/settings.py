# (c) 2026 Euxis Fleet. All rights reserved.
"""Settings screen for ETX configuration."""

from __future__ import annotations

from typing import TYPE_CHECKING

from textual.app import ComposeResult
from textual.containers import Container, Horizontal, Vertical
from textual.screen import Screen
from textual.widgets import Button, Footer, Label, Select, Static, Switch

from tui.core.config import ETXConfig
from tui.core.runner import PROVIDERS
from tui.widgets.header import ETXHeader

if TYPE_CHECKING:
    from tui.app import EuxisApp

THEME_OPTIONS = [
    ("ETX Dark", "textual-dark"),
    ("ETX Light", "textual-light"),
    ("High Contrast", "textual-ansi"),
]


class SettingsScreen(Screen):
    """Configuration and preferences screen."""

    BINDINGS = [
        ("escape", "go_back", "Back"),
        ("ctrl+s", "save_settings", "Save"),
        ("ctrl+k", "app.command_palette", "Commands"),
    ]

    @property
    def euxis_app(self) -> EuxisApp:
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        config = self.euxis_app.config

        yield ETXHeader(id="header")
        with Container(id="settings-screen"):
            yield Static("[bold]Settings[/]", classes="section-title")

            with Container(id="settings-container"):
                # Theme selection
                yield Static("[bold cyan]Appearance[/]", classes="settings-group-title")
                with Horizontal(classes="settings-row"):
                    yield Label("Theme", classes="settings-label")
                    yield Select(
                        [(name, value) for name, value in THEME_OPTIONS],
                        value=config.theme,
                        id="theme-select",
                    )

                # Provider selection
                yield Static("[bold cyan]Provider[/]", classes="settings-group-title")
                with Horizontal(classes="settings-row"):
                    yield Label("Default Provider", classes="settings-label")
                    yield Select(
                        [(display, key) for key, display in PROVIDERS.items()],
                        value=config.default_provider,
                        id="provider-select",
                    )

                # Display options
                yield Static("[bold cyan]Display[/]", classes="settings-group-title")
                with Horizontal(classes="settings-row"):
                    yield Label("Show Agent Tags", classes="settings-label")
                    yield Switch(value=config.show_agent_tags, id="tags-switch")
                with Horizontal(classes="settings-row"):
                    yield Label("Reduced Motion", classes="settings-label")
                    yield Switch(value=config.reduced_motion, id="motion-switch")

                # Accessibility
                yield Static("[bold cyan]Accessibility[/]", classes="settings-group-title")
                with Horizontal(classes="settings-row"):
                    yield Label("Accessible Mode", classes="settings-label")
                    yield Switch(value=config.accessible_mode, id="accessible-switch")

                # Actions
                with Horizontal(classes="settings-row"):
                    yield Button("Save", variant="primary", id="save-btn")
                    yield Button("Cancel", id="cancel-btn")

        yield Footer()

    def on_mount(self) -> None:
        header = self.query_one(ETXHeader)
        header.project = self.euxis_app.project_name
        header.branch = self.euxis_app.git_branch or ""
        header.provider = self.euxis_app.config.default_provider

    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "save-btn":
            self.action_save_settings()
        elif event.button.id == "cancel-btn":
            self.action_go_back()

    def action_save_settings(self) -> None:
        config = self.euxis_app.config

        theme_select = self.query_one("#theme-select", Select)
        if theme_select.value is not Select.BLANK:
            config.theme = str(theme_select.value)
            self.app.theme = config.theme

        provider_select = self.query_one("#provider-select", Select)
        if provider_select.value is not Select.BLANK:
            config.default_provider = str(provider_select.value)

        config.show_agent_tags = self.query_one("#tags-switch", Switch).value
        config.reduced_motion = self.query_one("#motion-switch", Switch).value
        config.accessible_mode = self.query_one("#accessible-switch", Switch).value

        config.save()
        self.notify("Settings saved", severity="information")
        self.app.pop_screen()

    def action_go_back(self) -> None:
        self.app.pop_screen()
