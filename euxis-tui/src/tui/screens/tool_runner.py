# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Tool runner screen for executing euxis system commands."""

from __future__ import annotations

import asyncio
import os
from pathlib import Path
from typing import TYPE_CHECKING, Any

from textual.containers import Container
from textual.screen import Screen
from textual.widgets import Static

from tui.core import EUXIS_HOME
from tui.widgets.header import ETXHeader
from tui.widgets.output_panel import OutputPanel
from tui.widgets.shortcut_bar import ShortcutBar

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.app import EuxisApp


class ToolRunnerScreen(Screen[None]):
    """Run an euxis CLI tool and stream output."""

    BINDINGS = [
        ("escape", "go_back", "Back"),
        ("ctrl+l", "clear_output", "Clear"),
    ]

    def __init__(self, tool_name: str, tool_label: str = "", **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.tool_name = tool_name
        self.tool_label = tool_label or tool_name

    @property
    def euxis_app(self) -> EuxisApp:
        """Return the typed application instance."""
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        """Build the tool runner layout."""
        yield ETXHeader(id="header")
        with Container(id="agent-screen"):
            yield Static(id="tool-title")
            yield OutputPanel(id="agent-output")

        yield ShortcutBar()

    def on_mount(self) -> None:
        """Configure header and start tool execution."""
        header = self.query_one(ETXHeader)
        header.project = self.euxis_app.project_name
        header.branch = self.euxis_app.git_branch or ""

        title = self.query_one("#tool-title", Static)
        title.update(f"[bold cyan]{self.tool_label}[/]")

        self.run_worker(self._execute_tool, exclusive=True)

    async def _execute_tool(self) -> None:
        output = self.query_one(OutputPanel)
        output.write_status(f"Running {self.tool_name}...", "cyan")
        output.write_separator()

        tool_bin = Path.home() / "bin" / self.tool_name
        if not tool_bin.exists():
            tool_bin = EUXIS_HOME / "bin" / self.tool_name

        if not tool_bin.exists():
            output.write_status(f"Tool not found: {self.tool_name}", "red")
            return

        env = os.environ.copy()
        env["EUXIS_HOME"] = str(EUXIS_HOME)

        process = await asyncio.create_subprocess_exec(
            str(tool_bin),
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
            env=env,
        )

        if process.stdout is None:
            output.write_status("Failed to capture output", "red")
            return
        while True:
            line = await process.stdout.readline()
            if not line:
                break
            decoded = line.decode("utf-8", errors="replace").rstrip("\n")
            output.write_line(decoded)

        await process.wait()
        rc = process.returncode

        output.write_separator()
        if rc == 0:
            output.write_status("Complete (exit 0)", "green")
            self.notify(f"{self.tool_label} complete", severity="information")
        else:
            output.write_status(f"Failed (exit {rc})", "red")
            self.notify(f"{self.tool_label} failed", severity="error")

    def action_go_back(self) -> None:
        """Return to the previous screen."""
        self.app.pop_screen()

    def action_clear_output(self) -> None:
        """Clear the output panel."""
        self.query_one(OutputPanel).clear()
