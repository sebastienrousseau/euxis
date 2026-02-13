# (c) 2026 Euxis Fleet. All rights reserved.
"""ETX header bar widget with Liquid Glass branding."""

from __future__ import annotations

from typing import TYPE_CHECKING

from textual.reactive import reactive
from textual.widget import Widget
from textual.widgets import Static

from tui.i18n import _

if TYPE_CHECKING:
    from textual.app import ComposeResult

PROVIDER_ICONS = {
    "claude": "◈",
    "openai": "◉",
    "gemini": "◇",
    "local": "◎",
}


class ETXHeader(Widget):
    """Custom header bar showing logo, project context, and status."""

    DEFAULT_CSS = """
    ETXHeader {
        dock: top;
        height: 3;
        background: $panel;
        layout: horizontal;
        padding: 0 2;
        border-bottom: solid $accent 40%;
    }
    """

    project: reactive[str] = reactive("euxis")
    project_path: reactive[str] = reactive("")
    branch: reactive[str] = reactive("")
    provider: reactive[str] = reactive("claude")
    agent_count: reactive[int] = reactive(41)
    version: reactive[str] = reactive("0.0.7")
    model: reactive[str] = reactive("")

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

    def _update_display(self) -> None:
        logo = self.query_one("#etx-header-logo", Static)
        logo.update(f"[bold]⬡ EUXIS[/] [dim]v{self.version}[/]")

        context = self.query_one("#etx-header-context", Static)
        path_display = self.project_path or self.project
        parts = [f"\U0001f4c1 [bold]{path_display}[/]"]
        if self.branch:
            parts.append(f"[dim]on[/] [bold]{self.branch}[/]")
        context.update("  │  ".join(parts))

        status = self.query_one("#etx-header-status", Static)
        icon = PROVIDER_ICONS.get(self.provider, "●")
        model_display = self.model or self.provider
        status.update(
            f"[bold]{self.agent_count}[/] [dim]{_('agents')}[/]  │  "
            f"{icon} {model_display}"
        )
