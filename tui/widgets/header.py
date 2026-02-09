# (c) 2026 Euxis Fleet. All rights reserved.
"""ETX header bar widget."""

from __future__ import annotations

from typing import TYPE_CHECKING

from textual.reactive import reactive
from textual.widget import Widget
from textual.widgets import Static

if TYPE_CHECKING:
    from textual.app import ComposeResult


class ETXHeader(Widget):
    """Custom header bar showing logo, project context, and status."""

    DEFAULT_CSS = """
    ETXHeader {
        dock: top;
        height: 3;
        background: $primary-darken-3;
        layout: horizontal;
        padding: 0 1;
    }
    """

    project: reactive[str] = reactive("euxis")
    branch: reactive[str] = reactive("")
    provider: reactive[str] = reactive("claude")
    agent_count: reactive[int] = reactive(41)
    version: reactive[str] = reactive("0.0.7")

    def compose(self) -> ComposeResult:
        """Build header bar with logo, context, and status areas."""
        yield Static(id="etx-header-logo")
        yield Static(id="etx-header-context")
        yield Static(id="etx-header-status")

    def on_mount(self) -> None:
        """Render initial header content."""
        self._update_display()

    def watch_project(self) -> None:
        """Refresh display when project changes."""
        self._update_display()

    def watch_branch(self) -> None:
        """Refresh display when branch changes."""
        self._update_display()

    def watch_provider(self) -> None:
        """Refresh display when provider changes."""
        self._update_display()

    def _update_display(self) -> None:
        logo = self.query_one("#etx-header-logo", Static)
        logo.update("[bold cyan]EUXIS[/] [dim]ETX[/]")

        context = self.query_one("#etx-header-context", Static)
        parts = [f"[bold]{self.project}[/]"]
        if self.branch:
            parts.append(f"[dim]on[/] [yellow]{self.branch}[/]")
        context.update("  ".join(parts))

        status = self.query_one("#etx-header-status", Static)
        status.update(
            f"[dim]v{self.version}[/]  "
            f"[bold]{self.agent_count}[/][dim] agents[/]  "
            f"[yellow]{self.provider}[/]"
        )
