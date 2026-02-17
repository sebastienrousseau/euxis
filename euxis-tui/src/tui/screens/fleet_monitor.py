# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Fleet monitor screen for squad and dispatch operations."""

from __future__ import annotations

import re
from collections.abc import AsyncIterator
from typing import TYPE_CHECKING, Any

from textual.containers import Container, Horizontal, VerticalScroll
from textual.css.query import NoMatches
from textual.screen import Screen
from textual.widgets import Input, ProgressBar, Static

from tui.core.runner import run_combo, run_squad
from tui.widgets.header import ETXHeader
from tui.widgets.output_panel import OutputPanel
from tui.widgets.shortcut_bar import ShortcutBar

if TYPE_CHECKING:
    from textual.app import ComposeResult

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

    def __init__(self, agent_id: str, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.agent_id = agent_id
        self._status = "pending"

    def compose(self) -> ComposeResult:
        """Build the agent monitor row layout."""
        yield Static(
            f"[bold]{self.agent_id}[/]",
            classes="monitor-agent-name",
        )
        yield ProgressBar(total=100, show_eta=False, classes="monitor-agent-progress")
        yield Static("[dim]Pending[/]", classes="monitor-agent-status")

    def set_status(self, status: str, progress: int = 0) -> None:
        """Update the agent status indicator and progress bar."""
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

    @property
    def status(self) -> str:
        """Return the current status."""
        return self._status


class FleetMonitorScreen(Screen[None]):
    """Monitor screen for squad deployments and dispatch operations."""

    BINDINGS = [
        ("escape", "go_back", "Dashboard"),
        ("ctrl+k", "app.command_palette", "Commands"),
    ]

    def __init__(
        self,
        operation_type: str = "squad",
        operation_id: str = "",
        members: list[str] | None = None,
        task_description: str = "",
        **kwargs: Any,
    ) -> None:
        super().__init__(**kwargs)
        self.operation_type = operation_type
        self.operation_id = operation_id
        self.members = members or []
        self.task_description = task_description

    @property
    def euxis_app(self) -> EuxisApp:
        """Return the typed application instance."""
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        """Build the fleet monitor layout."""
        yield ETXHeader(id="header")
        with Container(id="fleet-monitor"):
            with Horizontal(id="monitor-header"):
                yield Static(id="monitor-title")
                yield Static(id="monitor-stats")

            yield VerticalScroll(id="monitor-grid")

            yield OutputPanel(id="monitor-output")

        yield ShortcutBar()

    def on_mount(self) -> None:
        """Configure header and build agent monitor rows."""
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
        if self.task_description:
            self.run_worker(self._execute_operation, exclusive=True)

    # Patterns from euxis-combo / euxis-squad / dispatch.sh output
    _RE_STEP = re.compile(r"\s*Step\s+(\d+)/(\d+):\s+(\S+)")
    _RE_AGENT = re.compile(r"Agent:\s*(\S+)")
    _RE_DELEGATING = re.compile(r"Delegating to\s+(\S+)")
    _RE_COMPLETE = re.compile(r"(?:complete|finished|done)\b", re.IGNORECASE)
    _RE_ERROR = re.compile(r"(?:error|failed|abort)", re.IGNORECASE)

    def _get_stream(self) -> AsyncIterator[str]:
        """Return the async output stream for the selected operation."""
        if self.operation_type == "squad":
            return run_squad(self.operation_id, self.task_description)
        return run_combo(self.operation_id, self.task_description)

    def _set_active_agent(
        self,
        current_agent: str | None,
        agent_name: str,
        completed_count: int,
        progress: int,
    ) -> tuple[str, int]:
        """Transition active agent and update progress."""
        if current_agent and current_agent != agent_name:
            self._set_agent_status(current_agent, "complete")
            completed_count += 1
        self._set_agent_status(agent_name, "running", max(progress, 10))
        return agent_name, completed_count

    def _handle_step_line(
        self,
        line: str,
        current_agent: str | None,
        completed_count: int,
    ) -> tuple[str | None, int, bool]:
        """Handle combo-style step lines."""
        match = self._RE_STEP.search(line)
        if not match:
            return current_agent, completed_count, False

        step_num = int(match.group(1))
        step_total = int(match.group(2))
        agent_name = match.group(3)
        progress = int((step_num / step_total) * 100)
        current_agent, completed_count = self._set_active_agent(
            current_agent,
            agent_name,
            completed_count,
            progress,
        )
        return current_agent, completed_count, True

    def _handle_agent_line(
        self,
        line: str,
        current_agent: str | None,
        completed_count: int,
        total: int,
    ) -> tuple[str | None, int, bool]:
        """Handle dispatch-style agent lines."""
        if "Agent:" not in line:
            return current_agent, completed_count, False
        match = self._RE_AGENT.search(line)
        if not match:
            return current_agent, completed_count, False

        agent_name = match.group(1)
        progress = int(((completed_count + 1) / total) * 100)
        current_agent, completed_count = self._set_active_agent(
            current_agent,
            agent_name,
            completed_count,
            progress,
        )
        return current_agent, completed_count, True

    def _handle_delegate_line(
        self,
        line: str,
        current_agent: str | None,
        completed_count: int,
    ) -> tuple[str | None, int, bool]:
        """Handle delegation lines."""
        if "Delegating to" not in line:
            return current_agent, completed_count, False
        match = self._RE_DELEGATING.search(line)
        if not match:
            return current_agent, completed_count, False

        agent_name = match.group(1)
        current_agent, completed_count = self._set_active_agent(
            current_agent,
            agent_name,
            completed_count,
            50,
        )
        return current_agent, completed_count, True

    def _handle_error_line(self, line: str, current_agent: str | None) -> None:
        """Set error status when an error line appears."""
        if current_agent and self._RE_ERROR.search(line) and "[euxis]" in line:
            self._set_agent_status(current_agent, "error")

    def _mark_remaining_complete(self) -> None:
        """Mark any pending agents as complete."""
        for member in self.members:
            try:
                row = self.query_one(f"#monitor-{member}", AgentMonitorRow)
                if row.status == "pending":
                    row.set_status("complete")
            except NoMatches:
                pass

    def _finish_success(self, output: OutputPanel) -> None:
        """Finalize the UI after a successful run."""
        output.write_separator()
        output.write_status("Operation complete.", "green")
        self.notify("Fleet operation complete", severity="information")
        self._show_next_actions(output)

    def _finish_error(self, output: OutputPanel, exc: Exception, current_agent: str | None) -> None:
        """Finalize the UI after a failure."""
        if current_agent:
            self._set_agent_status(current_agent, "error")
        output.write_separator()
        output.write_status(f"Error: {exc}", "red")
        self.notify(f"Fleet error: {exc}", severity="error")
        self._show_next_actions(output)

    async def _execute_operation(self) -> None:
        output = self.query_one("#monitor-output", OutputPanel)
        output.write_status(f"Starting {self.operation_type}: {self.operation_id}")
        output.write_status(f"Task: {self.task_description}", "dim")
        output.write_separator()

        stream = self._get_stream()

        current_agent = None
        completed_count = 0
        total = len(self.members) or 1

        try:
            async for line in stream:
                output.write_line(line)

                # ── Combo step: "  Step 1/4: planner" ──
                current_agent, completed_count, handled = self._handle_step_line(
                    line,
                    current_agent,
                    completed_count,
                )
                if handled:
                    continue

                # ── Dispatch-style: "[euxis] ... Agent: orchestrator" ──
                current_agent, completed_count, handled = self._handle_agent_line(
                    line,
                    current_agent,
                    completed_count,
                    total,
                )
                if handled:
                    continue

                # ── Delegation: "Delegating to sub-agent..." ──
                current_agent, completed_count, handled = self._handle_delegate_line(
                    line,
                    current_agent,
                    completed_count,
                )
                if handled:
                    continue

                # ── Error in output ──
                self._handle_error_line(line, current_agent)

            # Mark final agent complete
            if current_agent:
                self._set_agent_status(current_agent, "complete")

            self._mark_remaining_complete()
            self._finish_success(output)

        except (OSError, RuntimeError, ValueError) as exc:
            self._finish_error(output, exc, current_agent)

    def _show_next_actions(self, output: OutputPanel) -> None:
        """Show post-completion hints and mount command input."""
        output.write_separator()
        output.write_status("What's next?")
        output.write_line(
            "  Type a new task below to re-run, "
            "or press Esc to return to the dashboard."
        )

        # Mount the command input above the shortcut bar
        container = self.query_one("#fleet-monitor", Container)
        task_input = Input(
            placeholder=f"Re-run {self.operation_id}, or Esc → Dashboard, ^K → Commands",
            id="next-task-input",
        )
        container.mount(task_input)
        task_input.focus()

        # Update shortcut bar for post-completion context
        try:
            bar = self.query_one(ShortcutBar)
            bar.update(
                "[bold] Esc [/][dim] Dashboard[/]  "
                "[bold] Enter [/][dim] Re-run[/]  "
                "[bold] ^K [/][dim] Commands[/]  "
                "[bold] ^P [/][dim] Playbooks[/]  "
                "[bold] ^Q [/][dim] Quit[/]"
            )
        except NoMatches:
            pass

    async def on_input_submitted(self, event: Input.Submitted) -> None:
        """Re-run the operation with a new task."""
        if event.input.id != "next-task-input":
            return

        task = event.value.strip()
        if not task:
            return

        # Reset the UI for a fresh run
        self.task_description = task
        event.input.remove()

        # Reset agent rows
        for member in self.members:
            self._set_agent_status(member, "pending")

        # Clear output
        output = self.query_one("#monitor-output", OutputPanel)
        output.clear()

        # Re-run
        self.run_worker(self._execute_operation, exclusive=True)

    def _set_agent_status(self, agent_id: str, status: str, progress: int = 0) -> None:
        try:
            row = self.query_one(f"#monitor-{agent_id}", AgentMonitorRow)
            row.set_status(status, progress)
        except NoMatches:
            pass  # Agent not in monitor grid

    def action_go_back(self) -> None:
        """Return to the dashboard."""
        self.app.pop_screen()
