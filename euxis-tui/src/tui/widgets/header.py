# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""ETX header bar widget with Liquid Glass branding and cost tracking."""

from __future__ import annotations

from typing import TYPE_CHECKING

from textual.reactive import reactive
from textual.widget import Widget
from textual.widgets import Static

from tui.i18n import _
from tui.widgets.sparkline import sparkline_text

if TYPE_CHECKING:
    from textual.app import ComposeResult

PROVIDER_ICONS = {
    "claude": "◈",
    "openai": "◉",
    "gemini": "◇",
    "local": "◎",
}


class ETXHeader(Widget):
    """Custom header bar showing logo, project context, cost burn rate, and status."""

    DEFAULT_CSS = """
    ETXHeader {
        dock: top;
        height: 3;
        background: $panel;
        layout: horizontal;
        padding: 0 2;
        border-bottom: solid $surface;
    }
    """

    project: reactive[str] = reactive("euxis")
    project_path: reactive[str] = reactive("")
    branch: reactive[str] = reactive("")
    provider: reactive[str] = reactive("claude")
    agent_count: reactive[int] = reactive(50)
    version: reactive[str] = reactive("0.1.0")
    model: reactive[str] = reactive("")
    # Cost tracking for burn rate visualization
    total_cost: reactive[float] = reactive(0.0)
    burn_rate: reactive[float] = reactive(0.0)  # $/minute

    def __init__(self, **kwargs) -> None:
        super().__init__(**kwargs)
        self._cost_history: list[float] = []  # Last 20 cost samples for sparkline

    def compose(self) -> ComposeResult:
        """Build header bar with logo, context, cost burn rate, and status areas."""
        yield Static(id="etx-header-logo")
        yield Static(id="etx-header-context")
        yield Static(id="etx-header-cost")  # New: cost burn rate display
        yield Static(id="etx-header-status")

    def on_mount(self) -> None:
        """Render initial header content."""
        self._update_display()

    def watch_project(self) -> None:
        """Refresh display when project changes."""
        self._update_display()

    def watch_project_path(self) -> None:
        """Refresh display when project path changes."""
        self._update_display()

    def watch_branch(self) -> None:
        """Refresh display when branch changes."""
        self._update_display()

    def watch_provider(self) -> None:
        """Refresh display when provider changes."""
        self._update_display()

    def watch_model(self) -> None:
        """Refresh display when model changes."""
        self._update_display()

    def watch_total_cost(self) -> None:
        """Refresh display when cost changes."""
        self._update_cost_display()

    def watch_burn_rate(self) -> None:
        """Refresh display when burn rate changes."""
        self._update_cost_display()

    def add_cost(self, amount: float) -> None:
        """Add a cost event and update burn rate tracking."""
        self.total_cost += amount
        self._cost_history.append(amount)
        if len(self._cost_history) > 20:
            self._cost_history = self._cost_history[-20:]

        # Calculate burn rate (sum of recent costs / time window)
        if len(self._cost_history) >= 2:
            self.burn_rate = sum(self._cost_history[-5:]) * 12  # Approx $/min based on 5s samples
        self._update_cost_display()

    def _update_cost_display(self) -> None:
        """Update the cost burn rate display."""
        try:
            cost_widget = self.query_one("#etx-header-cost", Static)

            if self.total_cost > 0 or self._cost_history:
                # Generate mini sparkline for cost trend
                spark = sparkline_text(self._cost_history, width=8) if self._cost_history else ""

                # Color based on burn rate
                if self.burn_rate > 1.0:
                    color = "red"
                elif self.burn_rate > 0.1:
                    color = "yellow"
                else:
                    color = "green"

                cost_widget.update(
                    f"[{color}]${self.total_cost:.3f}[/] {spark}"
                )
            else:
                cost_widget.update("")
        except Exception:
            pass  # Widget may not be mounted

    def _update_display(self) -> None:
        logo = self.query_one("#etx-header-logo", Static)
        logo.update(f"[bold]⬡ EUXIS[/] [dim]v{self.version}[/]")

        context = self.query_one("#etx-header-context", Static)
        path_display = self.project_path or self.project
        parts = [f"\U0001f4c1 [bold]{path_display}[/]"]
        if self.branch:
            parts.append(f"[dim]on[/] [bold]{self.branch}[/]")
        context.update("  │  ".join(parts))

        self._update_cost_display()

        status = self.query_one("#etx-header-status", Static)
        icon = PROVIDER_ICONS.get(self.provider, "●")
        model_display = self.model or self.provider
        status.update(
            f"[bold]{self.agent_count}[/] [dim]{_('agents')}[/]  │  "
            f"{icon} {model_display}"
        )

    def reset_cost(self) -> None:
        """Reset cost tracking for a new mission."""
        self.total_cost = 0.0
        self.burn_rate = 0.0
        self._cost_history = []
        self._update_cost_display()
