# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Cortex memory browser and management screen."""

from __future__ import annotations

import asyncio
import os
from pathlib import Path
from typing import TYPE_CHECKING

from textual.containers import Container, Horizontal
from textual.screen import Screen
from textual.widgets import Input, Static

from tui.core import EUXIS_HOME
from tui.widgets.header import ETXHeader
from tui.widgets.output_panel import OutputPanel
from tui.widgets.shortcut_bar import ShortcutBar

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.app import EuxisApp


class CortexScreen(Screen[None]):
    """Browse and interact with the Cortex semantic memory system."""

    BINDINGS = [
        ("escape", "go_back", "Back"),
        ("ctrl+k", "app.command_palette", "Commands"),
    ]

    @property
    def euxis_app(self) -> EuxisApp:
        """Return the typed application instance."""
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        """Build the cortex browser layout."""
        yield ETXHeader(id="header")
        with Container(id="agent-screen"):
            with Horizontal(id="agent-info-bar"):
                yield Static(
                    "[bold cyan]Cortex[/] [dim]Semantic Memory System[/]",
                    id="agent-name-display",
                )
                yield Static(id="agent-provider-display")
            with Container(id="task-input-container"):
                yield Input(
                    placeholder="Query cortex memory (e.g., 'recall auth patterns')...",
                    id="cortex-input",
                )
            yield OutputPanel(id="agent-output")

        yield ShortcutBar()

    def on_mount(self) -> None:
        """Configure header and load cortex status."""
        header = self.query_one(ETXHeader)
        header.project = self.euxis_app.project_name
        header.branch = self.euxis_app.git_branch or ""

        # Load cortex status
        self.run_worker(self._load_status, exclusive=True)

    async def _load_status(self) -> None:
        output = self.query_one(OutputPanel)
        output.write_status("Cortex Semantic Memory", "cyan")
        output.write_separator()

        cortex_bin = Path.home() / "cli" / "bin" / "euxis-cortex"
        if not cortex_bin.exists():
            cortex_bin = EUXIS_HOME / "cli" / "bin" / "euxis-cortex"

        if not cortex_bin.exists():
            output.write_status("Cortex binary not found", "red")
            return

        env = os.environ.copy()
        env["EUXIS_HOME"] = str(EUXIS_HOME)

        try:
            process = await asyncio.create_subprocess_exec(
                str(cortex_bin), "stats",
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.STDOUT,
                env=env,
            )
            if process.stdout is None:
                return
            while True:
                line = await process.stdout.readline()
                if not line:
                    break
                decoded = line.decode("utf-8", errors="replace").rstrip("\n")
                output.write_line(decoded)

            await process.wait()
        except (OSError, RuntimeError) as exc:
            output.write_status(f"Error loading cortex: {exc}", "red")

        output.write_separator()
        output.write_status("Enter a query to search memory", "dim")

    async def on_input_submitted(self, event: Input.Submitted) -> None:
        """Handle query submission and recall from cortex memory."""
        if event.input.id != "cortex-input":
            return

        query = event.value.strip()
        if not query:
            return

        event.input.value = ""

        output = self.query_one(OutputPanel)
        output.write_separator()
        output.write_status(f"Recalling: {query}", "cyan")

        cortex_bin = Path.home() / "cli" / "bin" / "euxis-cortex"
        if not cortex_bin.exists():
            cortex_bin = EUXIS_HOME / "cli" / "bin" / "euxis-cortex"  # pragma: no cover

        env = os.environ.copy()
        env["EUXIS_HOME"] = str(EUXIS_HOME)

        try:
            process = await asyncio.create_subprocess_exec(
                str(cortex_bin), "recall", query,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.STDOUT,
                env=env,
            )
            if process.stdout is None:
                return
            while True:
                line = await process.stdout.readline()
                if not line:
                    break
                decoded = line.decode("utf-8", errors="replace").rstrip("\n")
                output.write_line(decoded)

            await process.wait()
        except (OSError, RuntimeError) as exc:
            output.write_status(f"Error: {exc}", "red")

    def action_go_back(self) -> None:
        """Return to the previous screen."""
        self.app.pop_screen()
