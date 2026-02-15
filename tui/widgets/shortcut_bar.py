# (c) 2026 Euxis Fleet. All rights reserved.
"""Compact Codex-style shortcut bar replacing Textual's Footer."""

from __future__ import annotations

from typing import Any

from textual.widgets import Static

# Default shortcuts shown on the dashboard and most screens
DEFAULT_SHORTCUTS = [
    ("?", "Help"),
    ("/", "Search"),
    ("^K", "Commands"),
    ("^T", "Theme"),
    ("^P", "Playbooks"),
    ("^S", "Settings"),
    ("F2", "Welcome"),
    ("^Q", "Quit"),
]


class ShortcutBar(Static):
    """Single-line shortcut bar docked to the bottom of the screen."""

    DEFAULT_CSS = """
    ShortcutBar {
        dock: bottom;
        height: 2;
        background: $panel;
        border-top: solid $accent 40%;
    }
    """

    def __init__(
        self,
        shortcuts: list[tuple[str, str]] | None = None,
        **kwargs: Any,
    ) -> None:
        super().__init__("", **kwargs)
        self._shortcuts = shortcuts or DEFAULT_SHORTCUTS

    def on_mount(self) -> None:
        """Render shortcut labels."""
        parts: list[str] = []
        for key, label in self._shortcuts:
            parts.append(f"[bold] {key} [/][dim] {label}[/]")
        self.update("  ".join(parts))
