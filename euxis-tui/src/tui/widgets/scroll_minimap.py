# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Scroll minimap widget showing active agent positions in the fleet grid.

Provides a visual "breadcrumb" indicator on the scrollbar area that
highlights where active agents are located, helping users navigate
large fleets without losing track of running agents.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Any

from textual.reactive import reactive
from textual.widget import Widget

if TYPE_CHECKING:
    from textual.app import ComposeResult


class ScrollMinimap(Widget):
    """A vertical minimap showing scroll position and active agent markers.

    Renders as a thin vertical bar with:
    - Current viewport indicator (bright region)
    - Active agent markers (colored dots at their scroll positions)
    """

    DEFAULT_CSS = """
    ScrollMinimap {
        width: 2;
        height: 100%;
        dock: right;
        background: $surface;
        border-left: solid $panel;
    }
    """

    # Reactive properties
    viewport_start: reactive[float] = reactive(0.0)  # 0.0-1.0
    viewport_size: reactive[float] = reactive(0.2)   # Fraction of total
    active_positions: reactive[tuple[float, ...]] = reactive(())  # Agent positions 0.0-1.0

    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)

    def render(self) -> str:
        """Render the minimap as a vertical bar with markers."""
        height = self.size.height
        if height < 1:
            return ""

        lines: list[str] = []

        # Calculate viewport region
        vp_start_line = int(self.viewport_start * height)
        vp_end_line = int((self.viewport_start + self.viewport_size) * height)

        # Convert active positions to line numbers
        active_lines = {int(pos * height) for pos in self.active_positions}

        for i in range(height):
            if i in active_lines:
                # Active agent marker - bright green dot
                lines.append("[bold green]●[/]")
            elif vp_start_line <= i <= vp_end_line:
                # Current viewport - highlighted
                lines.append("[on $panel]░[/]")
            else:
                # Outside viewport - dim
                lines.append("[dim]·[/]")

        return "\n".join(lines)

    def update_viewport(self, scroll_y: float, max_scroll: float, viewport_height: float, content_height: float) -> None:
        """Update the viewport indicator based on scroll position.

        Args:
            scroll_y: Current scroll offset (pixels)
            max_scroll: Maximum scroll offset (pixels)
            viewport_height: Visible area height (pixels)
            content_height: Total content height (pixels)
        """
        if content_height <= 0 or max_scroll <= 0:
            self.viewport_start = 0.0
            self.viewport_size = 1.0
            return

        self.viewport_start = scroll_y / content_height
        self.viewport_size = min(1.0, viewport_height / content_height)

    def set_active_positions(self, positions: list[float]) -> None:
        """Set the positions of active agents (0.0-1.0 normalized).

        Args:
            positions: List of normalized positions (0.0 = top, 1.0 = bottom)
        """
        self.active_positions = tuple(positions)
