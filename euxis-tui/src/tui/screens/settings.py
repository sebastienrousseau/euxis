# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Settings screen for ETX configuration."""

from __future__ import annotations

from typing import TYPE_CHECKING

from textual.containers import Container, Horizontal
from textual.screen import Screen
from textual.widgets import Button, Label, Select, Static, Switch

from tui.core.runner import PROVIDERS
from tui.i18n import _, set_locale
from tui.widgets.header import ETXHeader
from tui.widgets.shortcut_bar import ShortcutBar

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.app import EuxisApp

THEME_OPTIONS = [
    ("Liquid Glass", "etx-liquid-glass"),
    ("Liquid Glass Light", "etx-liquid-light"),
    ("Catppuccin Mocha", "etx-catppuccin-mocha"),
    ("Catppuccin Latte", "etx-catppuccin-latte"),
    ("Tokyo Night", "etx-tokyo-night"),
    ("Dracula", "etx-dracula"),
    ("Nord", "etx-nord"),
    ("Gruvbox Dark", "etx-gruvbox"),
    ("Rosé Pine", "etx-rose-pine"),
    ("Ayu Mirage", "etx-ayu-mirage"),
]

LOCALE_OPTIONS = [
    ("English", "en"),
    ("Français", "fr"),
    ("Deutsch", "de"),
    ("Español", "es"),
    ("日本語", "ja"),
]


class SettingsScreen(Screen[None]):
    """Configuration and preferences screen."""

    BINDINGS = [
        ("escape", "go_back", "Back"),
        ("ctrl+s", "save_settings", "Save"),
        ("ctrl+k", "app.command_palette", "Commands"),
    ]

    @property
    def euxis_app(self) -> EuxisApp:
        """Return the typed application instance."""
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        """Build the settings form layout."""
        config = self.euxis_app.config

        yield ETXHeader(id="header")
        with Container(id="settings-screen"):
            yield Static(f"[bold]{_('Settings')}[/]", classes="section-title")

            with Container(id="settings-container"):
                # Theme selection
                yield Static(f"[bold]{_('Appearance')}[/]", classes="settings-group-title")
                with Horizontal(classes="settings-row"):
                    yield Label(_("Theme"), classes="settings-label")
                    yield Select(
                        [(name, value) for name, value in THEME_OPTIONS],
                        value=config.theme,
                        id="theme-select",
                    )

                # Provider selection
                yield Static(f"[bold]{_('Provider')}[/]", classes="settings-group-title")
                with Horizontal(classes="settings-row"):
                    yield Label(_("Default Provider"), classes="settings-label")
                    yield Select(
                        [(display, key) for key, display in PROVIDERS.items()],
                        value=config.default_provider,
                        id="provider-select",
                    )

                # Language selection
                yield Static(f"[bold]{_('Language')}[/]", classes="settings-group-title")
                with Horizontal(classes="settings-row"):
                    yield Label(_("Language"), classes="settings-label")
                    yield Select(
                        [(name, code) for name, code in LOCALE_OPTIONS],
                        value=config.locale,
                        id="locale-select",
                    )

                # Display options
                yield Static(f"[bold]{_('Display')}[/]", classes="settings-group-title")
                with Horizontal(classes="settings-row"):
                    yield Label(_("Show Agent Tags"), classes="settings-label")
                    yield Switch(value=config.show_agent_tags, id="tags-switch")
                with Horizontal(classes="settings-row"):
                    yield Label(_("Reduced Motion"), classes="settings-label")
                    yield Switch(value=config.reduced_motion, id="motion-switch")

                # Accessibility
                yield Static(f"[bold]{_('Accessibility')}[/]", classes="settings-group-title")
                with Horizontal(classes="settings-row"):
                    yield Label(_("Accessible Mode"), classes="settings-label")
                    yield Switch(value=config.accessible_mode, id="accessible-switch")

                # Actions
                with Horizontal(classes="settings-row"):
                    yield Button(_("Save"), variant="primary", id="save-btn")
                    yield Button(_("Cancel"), id="cancel-btn")

        yield ShortcutBar()

    def on_mount(self) -> None:
        """Configure header with current project context."""
        header = self.query_one(ETXHeader)
        header.project = self.euxis_app.project_name
        header.branch = self.euxis_app.git_branch or ""
        header.provider = self.euxis_app.config.default_provider

    def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handle save and cancel button presses."""
        if event.button.id == "save-btn":
            self.action_save_settings()
        elif event.button.id == "cancel-btn":
            self.action_go_back()

    def action_save_settings(self) -> None:
        """Persist current form values to configuration file."""
        config = self.euxis_app.config

        theme_select = self.query_one("#theme-select", Select)
        if theme_select.value is not Select.BLANK:
            config.theme = str(theme_select.value)
            self.app.theme = config.theme

        provider_select = self.query_one("#provider-select", Select)
        if provider_select.value is not Select.BLANK:
            config.default_provider = str(provider_select.value)

        locale_select = self.query_one("#locale-select", Select)
        if locale_select.value is not Select.BLANK:  # pragma: no cover
            config.locale = str(locale_select.value)
            set_locale(config.locale)

        config.show_agent_tags = self.query_one("#tags-switch", Switch).value
        config.reduced_motion = self.query_one("#motion-switch", Switch).value
        config.accessible_mode = self.query_one("#accessible-switch", Switch).value

        config.save()
        self.notify(_("Settings saved"), severity="information")
        self.app.pop_screen()

    def action_go_back(self) -> None:
        """Return to the previous screen."""
        self.app.pop_screen()
