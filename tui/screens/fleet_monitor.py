# (c) 2026 Euxis Fleet. All rights reserved.
"""Fleet monitor screen for squad and dispatch operations."""

from __future__ import annotations

import asyncio
from typing import TYPE_CHECKING

from textual.app import ComposeResult
from textual.containers import Container, Horizontal, VerticalScroll
from textual.screen import Screen
from textual.widgets import Footer, ProgressBar, Static

from tui.core.runner import run_squad, run_combo
from tui.widgets.header import ETXHeader
from tui.widgets.output_panel import OutputPanel

if TYPE_CHECKING:
    from tui.app import EuxisApp


class AgentMonitorRow(Horizontal):
    """A row in the fleet monitor showing one agent's status."""

    DEFAULT_CSS = """
    AgentMonitorRow {
        height: 3;
        padding: 0 1;
        border-bottom: solid $primary-background-darken-2;
    }
    AgentMonitorRow:hover {
        background: $surface;
    }
    """

    def __init__(self, agent_id: str, **kwargs) -> None:
        super().__init__(**kwargs)
        self.agent_id = agent_id
        self._status = "pending"

    def compose(self) -> ComposeResult:
        yield Static(
            f"[bold]{self.agent_id}[/]",
            classes="monitor-agent-name",
        )
        yield ProgressBar(total=100, show_eta=False, classes="monitor-agent-progress")
        yield Static("[dim]Pending[/]", classes="monitor-agent-status")

    def set_status(self, status: str, progress: int = 0) -> None:
        self._status = status
        status_widget = self.query(".monitor-agent-status").first(Static)
        progress_bar = self.query_one(ProgressBar)

        if status == "running":
            status_widget.update("[bold cyan]Running...[/]")
            progress_bar.progress = progress
        elif status == "complete":
            status_widget.update("[bold green]Complete[/]")
            progress_bar.progress = 100
        elif status == "error":
            status_widget.update("[bold red]Error[/]")
        else:
            status_widget.update("[dim]Pending[/]")
            progress_bar.progress = 0


class FleetMonitorScreen(Screen):
    """Monitor screen for squad deployments and dispatch operations."""

    BINDINGS = [
        ("escape", "go_back", "Back"),
        ("ctrl+k", "app.command_palette", "Commands"),
    ]

    def __init__(
        self,
        operation_type: str = "squad",
        operation_id: str = "",
        members: list[str] | None = None,
        task: str = "",
        **kwargs,
    ) -> None:
        super().__init__(**kwargs)
        self.operation_type = operation_type
        self.operation_id = operation_id
        self.members = members or []
        self.task = task

    @property
    def euxis_app(self) -> EuxisApp:
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        yield ETXHeader(id="header")
        with Container(id="fleet-monitor"):
            with Horizontal(id="monitor-header"):
                yield Static(id="monitor-title")
                yield Static(id="monitor-stats")

            yield VerticalScroll(id="monitor-grid")

            yield OutputPanel(id="monitor-output")

        yield Footer()

    def on_mount(self) -> None:
        header = self.query_one(ETXHeader)
        header.project = self.euxis_app.project_name
        header.branch = self.euxis_app.git_branch or ""
        header.provider = self.euxis_app.config.default_provider

        title = self.query_one("#monitor-title", Static)
        op_label = self.operation_type.title()
        title.update(f"[bold cyan]{op_label}: {self.operation_id}[/]")

        stats = self.query_one("#monitor-stats", Static)
        stats.update(f"[dim]{len(self.members)} agents[/]")

        # Create agent monitor rows
        grid = self.query_one("#monitor-grid", VerticalScroll)
        for member in self.members:
            row = AgentMonitorRow(member, id=f"monitor-{member}")
            grid.mount(row)

        # Start the operation
        if self.task:
            self.run_worker(self._execute_operation(), exclusive=True)

    async def _execute_operation(self) -> None:
        output = self.query_one("#monitor-output", OutputPanel)
        output.write_status(f"Starting {self.operation_type}: {self.operation_id}")
        output.write_status(f"Task: {self.task}", "dim")
        output.write_separator()

        if self.operation_type == "squad":
            stream = run_squad(self.operation_id, self.task)
        else:
            stream = run_combo(self.operation_id, self.task)

        current_agent = None
        try:
            async for line in stream:
                output.write_line(line)

                # Parse agent status from output lines
                if "[euxis]" in line and "Agent:" in line:
                    agent_name = line.split("Agent:")[-1].strip()
                    if current_agent and current_agent != agent_name:
                        self._set_agent_status(current_agent, "complete")
                    current_agent = agent_name
                    self._set_agent_status(agent_name, "running", 50)

            if current_agent:
                self._set_agent_status(current_agent, "complete")

            output.write_separator()
            output.write_status("Operation complete.", "green")
            self.notify("Fleet operation complete", severity="information")

        except Exception as exc:
            output.write_status(f"Error: {exc}", "red")
            self.notify(f"Fleet error: {exc}", severity="error")

    def _set_agent_status(self, agent_id: str, status: str, progress: int = 0) -> None:
        try:
            row = self.query_one(f"#monitor-{agent_id}", AgentMonitorRow)
            row.set_status(status, progress)
        except Exception:
            pass

    def action_go_back(self) -> None:
        self.app.pop_screen()
