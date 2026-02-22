# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Compact Codex-style shortcut bar replacing Textual's Footer.

Uses platform-aware formatting to display shortcuts appropriately
for macOS (⌘⌃⌥), Linux (Ctrl+Alt), and WSL environments.
"""

from __future__ import annotations

from typing import Any

from textual.widgets import Static

from tui.core.platform import PLATFORM

# Default shortcuts shown on the dashboard and most screens
# Format: (key_combo, label)
# Using F-keys to avoid terminal emulator conflicts (Ctrl+T = new tab, etc.)
DEFAULT_SHORTCUTS = [
    ("?", "Help"),
    ("/", "Find"),
    ("g", "Go Active"),
    ("F3", "Theme"),
    ("Ctrl+Q", "Quit"),
]


class ShortcutBar(Static):
    """Single-line shortcut bar docked to the bottom of the screen."""

    DEFAULT_CSS = """
    ShortcutBar {
        dock: bottom;
        height: 2;
        background: $panel;
        border-top: solid $surface;
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
        """Render shortcut labels with platform-aware formatting."""
        parts: list[str] = []
        for key, label in self._shortcuts:
            # Format key combo for current platform
            formatted_key = PLATFORM.format_shortcut(key)
            parts.append(f"[bold] {formatted_key} [/][dim] {label}[/]")
        self.update("  ".join(parts))

    @property
    def renderable(self) -> str:
        """Expose current renderable for tests."""
        return getattr(self, "_renderable", "")
