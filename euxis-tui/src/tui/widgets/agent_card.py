# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Agent card widget for the fleet grid with Liquid Glass styling."""

from __future__ import annotations

from typing import TYPE_CHECKING, Any

from textual.events import Key
from textual.message import Message
from textual.widget import Widget
from textual.widgets import Static

from tui.i18n import _

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.core.registry import Agent

# Tier indicators — styled by weight, colors from CSS theme
TIER_STYLES = {
    "core": ("bold", "CORE"),
    "fleet": ("", "FLEET"),
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
        height: 7;
        padding: 1 1;
        border: round $accent 40%;
    }
    AgentCard:hover {
        border: round $accent;
    }
    AgentCard:focus {
        border: round $accent;
    }
    AgentCard.tier-core {
        border: round $secondary 50%;
    }
    AgentCard.tier-core:hover {
        border: round $accent;
    }
    AgentCard.tier-core:focus {
        border: round $accent;
    }
    """

    class Selected(Message):
        """Posted when an agent card is selected."""

        def __init__(self, agent: Agent) -> None:
            super().__init__()
            self.agent = agent

    def __init__(self, agent: Agent, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.agent = agent
        if agent.tier == "core":
            self.add_class("tier-core")

    def compose(self) -> ComposeResult:
        """Build the agent card display with tier badge and tags."""
        style, label = TIER_STYLES.get(self.agent.tier, ("dim", "FLEET"))
        icon = ACTIVATION_ICONS.get(self.agent.activation, "●")

        yield Static(
            f"{icon} [bold]{self.agent.id}[/]",
            classes="agent-card-name",
        )
        tier_text = f"[{style}]{label}[/]" if style else label
        yield Static(
            f"  {tier_text} · {_(self.agent.activation_label)}",
            classes="agent-card-tier",
        )
        tags_display = " ".join(self.agent.tags[:3])
        yield Static(
            f"  [italic]{tags_display}[/]",
            classes="agent-card-tags",
        )

    def on_click(self) -> None:
        """Post selection message on mouse click."""
        self.post_message(self.Selected(self.agent))

    def on_key(self, event: Key) -> None:
        """Post selection message on Enter key press."""
        if event.key == "enter":
            self.post_message(self.Selected(self.agent))
            event.stop()
