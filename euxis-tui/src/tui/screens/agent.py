# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Agent execution screen with streaming output."""

from __future__ import annotations

import asyncio
from typing import TYPE_CHECKING, Any

from textual.containers import Container, Horizontal
from textual.screen import Screen
from textual.widgets import Input, Static

from tui.core.runner import AgentRun, run_agent
from tui.widgets.header import ETXHeader
from tui.widgets.output_panel import OutputPanel
from tui.widgets.shortcut_bar import ShortcutBar

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.app import EuxisApp
    from tui.core.registry import Agent


class AgentScreen(Screen[None]):
    """Screen for executing an agent and viewing streaming output."""

    BINDINGS = [
        ("ctrl+k", "app.command_palette", "Commands"),
        ("escape", "go_back", "Back"),
        ("ctrl+l", "clear_output", "Clear"),
    ]

    def __init__(
        self,
        agent: Agent,
        provider: str = "claude",
        initial_task: str = "",
        **kwargs: Any,
    ) -> None:
        super().__init__(**kwargs)
        self.agent = agent
        self.provider = provider
        self.initial_task = initial_task
        self._run: AgentRun | None = None
        self._timer_task: asyncio.Task | None = None

    @property
    def euxis_app(self) -> EuxisApp:
        """Return the typed application instance."""
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        """Build the agent execution screen layout."""
        yield ETXHeader(id="header")
        with Container(id="agent-screen"):
            with Horizontal(id="agent-info-bar"):
                yield Static(id="agent-name-display")
                yield Static(id="agent-provider-display")
                yield Static(id="agent-elapsed-display")
            with Container(id="task-input-container"):
                yield Input(
                    placeholder=f"Enter task for {self.agent.id}...",
                    id="task-input",
                )
            yield OutputPanel(id="agent-output")

        yield ShortcutBar()

    def on_mount(self) -> None:
        """Configure header and agent info display."""
        # Set header
        header = self.query_one(ETXHeader)
        header.project = self.euxis_app.project_name
        header.branch = self.euxis_app.git_branch or ""
        header.provider = self.provider
        header.agent_count = len(self.euxis_app.fleet_registry.agents)

        # Set info bar
        name_display = self.query_one("#agent-name-display", Static)
        tier_badge = "[yellow]CORE[/]" if self.agent.tier == "core" else "[cyan]FLEET[/]"
        name_display.update(
            f"[bold]{self.agent.id}[/]  {tier_badge}  "
            f"[dim]{', '.join(self.agent.tags[:3])}[/]"
        )

        provider_display = self.query_one("#agent-provider-display", Static)
        provider_display.update(f"[yellow]Provider: {self.provider}[/]")

        elapsed_display = self.query_one("#agent-elapsed-display", Static)
        elapsed_display.update("[dim]Ready[/]")

        # Focus the task input and pre-fill if launched with a task
        task_input = self.query_one("#task-input", Input)
        if self.initial_task:
            task_input.value = self.initial_task  # pragma: no cover
        task_input.focus()

        # Welcome message
        output = self.query_one(OutputPanel)
        output.write_status(f"Agent: {self.agent.id}")
        output.write_status(f"Tier: {self.agent.tier_label}")
        output.write_status(f"Provider: {self.provider}")
        output.write_separator()
        if self.initial_task:
            output.write_status(f"Task ready: {self.initial_task}", "cyan")  # pragma: no cover
            output.write_status("Press Enter to deploy.", "dim")  # pragma: no cover
        else:
            output.write_status("Enter a task and press Enter to begin.", "dim")

    async def on_input_submitted(self, event: Input.Submitted) -> None:
        """Handle task submission and start agent execution."""
        if event.input.id != "task-input":
            return

        task = event.value.strip()
        if not task:
            return

        # Disable input during execution
        task_input = self.query_one("#task-input", Input)
        task_input.disabled = True

        # Track in config
        self.euxis_app.config.add_recent_agent(self.agent.id)
        self.euxis_app.config.add_recent_command(f"{self.agent.id}: {task[:50]}")
        self.euxis_app.config.save()

        # Create run state
        self._run = AgentRun(
            agent_id=self.agent.id,
            task=task,
            provider=self.provider,
        )

        # Start output streaming
        output = self.query_one(OutputPanel)
        output.clear()
        output.write_separator()
        output.write_status(f"Deploying {self.agent.id}...", "cyan")
        output.write_status(f"Task: {task}", "dim")
        output.write_separator()

        # Start elapsed timer
        if self._timer_task and not self._timer_task.done():
            self._timer_task.cancel()
        self._timer_task = asyncio.create_task(self._update_elapsed())

        # Run agent in background
        self.run_worker(lambda: self._execute_agent(task), exclusive=True)

    async def _execute_agent(self, task: str) -> None:
        output = self.query_one(OutputPanel)

        try:
            async for line in run_agent(
                self.agent.id,
                task,
                self.provider,
            ):
                output.write_line(line)
                if self._run:  # pragma: no cover
                    self._run.output_lines.append(line)

            # Execution complete
            if self._run:  # pragma: no cover
                self._run.return_code = 0

            output.write_separator()
            output.write_status("Execution complete.", "green")
            self.notify("Agent execution complete", severity="information")

        except (OSError, RuntimeError, ValueError) as exc:
            if self._run:  # pragma: no cover
                self._run.return_code = 1
            output.write_separator()
            output.write_status(f"Error: {exc}", "red")
            self.notify(f"Agent error: {exc}", severity="error")

        finally:
            # Re-enable input
            task_input = self.query_one("#task-input", Input)
            task_input.disabled = False
            task_input.value = ""
            task_input.focus()

            # Stop timer
            if self._timer_task:
                self._timer_task.cancel()
                self._timer_task = None

    async def _update_elapsed(self) -> None:
        """Update the elapsed time display every second."""
        elapsed_display = self.query_one("#agent-elapsed-display", Static)
        try:
            while self._run and self._run.is_running:
                elapsed_display.update(f"[bold cyan]{self._run.elapsed_display}[/]")
                await asyncio.sleep(1)
            if self._run:  # pragma: no cover
                elapsed_display.update(f"[green]{self._run.elapsed_display}[/]")
        except asyncio.CancelledError:
            return

    def on_unmount(self) -> None:
        """Cancel background timer tasks during screen teardown."""
        if self._timer_task and not self._timer_task.done():
            self._timer_task.cancel()
        self._timer_task = None

    def action_go_back(self) -> None:
        """Return to the previous screen."""
        self.app.pop_screen()

    def action_clear_output(self) -> None:
        """Clear the output panel."""
        self.query_one(OutputPanel).clear()
