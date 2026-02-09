# (c) 2026 Euxis Fleet. All rights reserved.
"""Provider selection modal for choosing AI provider before agent execution."""

from __future__ import annotations

from textual.app import ComposeResult
from textual.containers import Center, Container
from textual.message import Message
from textual.screen import ModalScreen
from textual.widgets import Button, Footer, Label, OptionList, Static
from textual.widgets.option_list import Option

from tui.core.runner import PROVIDERS


class ProviderSelectModal(ModalScreen):
    """Modal dialog for selecting an AI provider."""

    DEFAULT_CSS = """
    ProviderSelectModal {
        align: center middle;
    }

    #provider-dialog {
        width: 50;
        height: auto;
        max-height: 20;
        background: $surface;
        border: round $accent;
        padding: 1 2;
    }

    #provider-title {
        text-style: bold;
        color: $accent;
        margin: 0 0 1 0;
        content-align: center middle;
    }

    #provider-list {
        height: auto;
        max-height: 12;
    }
    """

    class Selected(Message):
        """Posted when a provider is selected."""

        def __init__(self, provider: str) -> None:
            super().__init__()
            self.provider = provider

    BINDINGS = [
        ("escape", "dismiss", "Cancel"),
    ]

    def __init__(self, current: str = "claude", **kwargs) -> None:
        super().__init__(**kwargs)
        self.current = current

    def compose(self) -> ComposeResult:
        with Container(id="provider-dialog"):
            yield Static("[bold cyan]Select Provider[/]", id="provider-title")
            option_list = OptionList(id="provider-list")
            for key, display in PROVIDERS.items():
                marker = " (current)" if key == self.current else ""
                option_list.add_option(Option(f"{display}{marker}", id=key))
            yield option_list

    def on_option_list_option_selected(self, event: OptionList.OptionSelected) -> None:
        if event.option.id:
            self.dismiss(event.option.id)
