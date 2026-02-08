# (c) 2026 Euxis Fleet. All rights reserved.
"""Log viewer screen for browsing agent output history."""

from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

from textual.app import ComposeResult
from textual.containers import Container, Horizontal
from textual.screen import Screen
from textual.widgets import Footer, OptionList, Static
from textual.widgets.option_list import Option

from tui.core.runner import EUXIS_HOME, get_project_name
from tui.widgets.header import ETXHeader
from tui.widgets.output_panel import OutputPanel

if TYPE_CHECKING:
    from tui.app import EuxisApp


class LogViewerScreen(Screen):
    """Browse and view agent output logs."""

    BINDINGS = [
        ("escape", "go_back", "Back"),
        ("ctrl+k", "app.command_palette", "Commands"),
    ]

    @property
    def euxis_app(self) -> EuxisApp:
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        yield ETXHeader(id="header")
        with Container(id="log-viewer"):
            with Horizontal():
                yield OptionList(id="log-list")
                yield OutputPanel(id="log-content")
        yield Footer()

    def on_mount(self) -> None:
        header = self.query_one(ETXHeader)
        header.project = self.euxis_app.project_name
        header.branch = self.euxis_app.git_branch or ""
        header.provider = self.euxis_app.config.default_provider

        self._load_log_list()

    def _load_log_list(self) -> None:
        option_list = self.query_one("#log-list", OptionList)
        project = get_project_name()
        project_dir = EUXIS_HOME / "data" / "projects" / project

        if not project_dir.exists():
            option_list.add_option(Option("No logs found", disabled=True))
            return

        # Find all agent directories with output
        for agent_dir in sorted(project_dir.iterdir()):
            if not agent_dir.is_dir():
                continue
            output_dir = agent_dir / "output"
            if output_dir.exists() and list(output_dir.glob("*.md")):
                count = len(list(output_dir.glob("*.md")))
                option_list.add_option(
                    Option(f"{agent_dir.name} ({count})", id=agent_dir.name)
                )

    def on_option_list_option_selected(self, event: OptionList.OptionSelected) -> None:
        agent_id = event.option.id
        if not agent_id:
            return

        output = self.query_one("#log-content", OutputPanel)
        output.clear()

        project = get_project_name()
        output_dir = EUXIS_HOME / "data" / "projects" / project / agent_id / "output"

        if not output_dir.exists():
            output.write_status("No outputs found", "yellow")
            return

        # Show the most recent output
        files = sorted(output_dir.glob("*.md"), reverse=True)
        if not files:
            output.write_status("No output files found", "yellow")
            return

        latest = files[0]
        output.write_status(f"Agent: {agent_id}", "cyan")
        output.write_status(f"File: {latest.name}", "dim")
        output.write_separator()

        content = latest.read_text()
        for line in content.split("\n"):
            output.write_line(line)

    def action_go_back(self) -> None:
        self.app.pop_screen()
