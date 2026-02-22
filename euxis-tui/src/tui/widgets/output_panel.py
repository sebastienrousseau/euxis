# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Streaming output panel for agent execution."""

from __future__ import annotations

from typing import Any

from rich.markup import escape
from rich.text import Text
from textual.widgets import RichLog

try:
    import euxis_tui_rs
    HAS_RUST_TUI = True
except ImportError:
    HAS_RUST_TUI = False


class OutputPanel(RichLog):
    """A scrollable output panel for streaming agent results.

    Extends RichLog with agent-specific output formatting:
    - [euxis] prefixed lines styled as status messages
    - Markdown-like content rendered with colors
    - Error lines highlighted in red
    """

    DEFAULT_CSS = """
    OutputPanel {
        height: 1fr;
        border: round $primary-background-darken-2;
        background: $surface;
        padding: 1;
        scrollbar-size: 1 1;
    }
    OutputPanel:focus {
        border: round $accent;
    }
    """

    def __init__(self, **kwargs: Any) -> None:
        super().__init__(
            highlight=True,
            markup=True,
            wrap=True,
            **kwargs,
        )

    def write_line(self, line: str) -> None:
        """Write a single line with agent-aware formatting."""
        # 2026 Optimization: Offload ANSI stripping to Rust PyO3
        if HAS_RUST_TUI:
            line = euxis_tui_rs.strip_ansi(line)

        if line.startswith("[euxis]"):
            # Euxis status lines — accent colored
            # Strip spinner characters
            cleaned = line
            for char in "⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏":
                cleaned = cleaned.replace(char, "")
            safe = escape(cleaned.strip())
            self.write(Text.from_markup(f"[bold cyan]{safe}[/]"))
        elif line.startswith(("ERROR", "[ERROR")):
            safe = escape(line)
            self.write(Text.from_markup(f"[bold red]{safe}[/]"))
        elif line.startswith(("WARNING", "[WARNING")):
            safe = escape(line)
            self.write(Text.from_markup(f"[yellow]{safe}[/]"))
        elif line.startswith(("PASS", "✅")):
            safe = escape(line)
            self.write(Text.from_markup(f"[green]{safe}[/]"))
        elif line.startswith(("FAIL", "✗")):
            safe = escape(line)
            self.write(Text.from_markup(f"[bold red]{safe}[/]"))
        elif line.startswith("#"):
            # Markdown-like headings
            safe = escape(line)
            self.write(Text.from_markup(f"[bold]{safe}[/]"))
        elif line.startswith("```"):
            safe = escape(line)
            self.write(Text.from_markup(f"[dim]{safe}[/]"))
        elif line.startswith(("- ", "* ")):
            safe = escape(line)
            self.write(Text.from_markup(f"  {safe}"))
        else:
            self.write(line)

    def write_status(self, message: str, style: str = "cyan") -> None:
        """Write a status message."""
        safe = escape(message)
        self.write(Text.from_markup(f"[bold {style}]{safe}[/]"))

    def write_separator(self) -> None:
        """Write a visual separator line."""
        self.write(Text.from_markup("[dim]" + "─" * 60 + "[/]"))
