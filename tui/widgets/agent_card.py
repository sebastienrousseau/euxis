# (c) 2026 Euxis Fleet. All rights reserved.
"""Agent card widget for the fleet grid."""

from __future__ import annotations

from textual.app import ComposeResult
from textual.message import Message
from textual.widget import Widget
from textual.widgets import Static

from tui.core.registry import Agent


# Provider tier indicators
TIER_STYLES = {
    "core": ("bold yellow", "CORE"),
    "fleet": ("dim cyan", "FLEET"),
}

ACTIVATION_ICONS = {
    "default": "[green]●[/]",
    "on-demand": "[yellow]◐[/]",
    "specialist": "[magenta]◆[/]",
}


class AgentCard(Widget, can_focus=True):
    """A card representing a single agent in the fleet grid."""

    DEFAULT_CSS = """
    AgentCard {
        height: 5;
        border: round $primary-background-darken-2;
        padding: 0 1;
        background: $surface;
    }
    AgentCard:hover {
        border: round $accent;
        background: $surface-lighten-1;
    }
    AgentCard:focus {
        border: double $accent;
        background: $primary-background-darken-1;
    }
    AgentCard.tier-core {
        border: round $warning-darken-2;
    }
    AgentCard.tier-core:hover {
        border: round $warning;
    }
    AgentCard.tier-core:focus {
        border: double $warning;
    }
    """

    class Selected(Message):
        """Posted when an agent card is selected."""

        def __init__(self, agent: Agent) -> None:
            super().__init__()
            self.agent = agent

    def __init__(self, agent: Agent, **kwargs) -> None:
        super().__init__(**kwargs)
        self.agent = agent
        if agent.tier == "core":
            self.add_class("tier-core")

    def compose(self) -> ComposeResult:
        style, label = TIER_STYLES.get(self.agent.tier, ("dim", "FLEET"))
        icon = ACTIVATION_ICONS.get(self.agent.activation, "●")

        yield Static(
            f"{icon} [bold]{self.agent.id}[/]",
            classes="agent-card-name",
        )
        yield Static(
            f"  [{style}]{label}[/] [dim]·[/] [dim]{self.agent.activation_label}[/]",
            classes="agent-card-tier",
        )
        tags_display = " ".join(self.agent.tags[:3])
        yield Static(
            f"  [dim italic]{tags_display}[/]",
            classes="agent-card-tags",
        )

    def on_click(self) -> None:
        self.post_message(self.Selected(self.agent))

    def on_key(self, event) -> None:
        if event.key == "enter":
            self.post_message(self.Selected(self.agent))
            event.stop()
