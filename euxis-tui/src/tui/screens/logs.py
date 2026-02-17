# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Enhanced Log Viewer with 2026 features.

Features:
- Live tailing (tail -f style) with auto-scroll
- Regex search/filter with highlighting
- Syntax highlighting for log levels (ERROR, WARN, INFO, DEBUG)
- Agent filtering for focused troubleshooting
- Streaming from disk (memory efficient)
- Merged timeline view for multi-agent debugging
"""

from __future__ import annotations

import os
import re
from datetime import datetime
from pathlib import Path
from typing import TYPE_CHECKING, Any

from textual.binding import Binding
from textual.containers import Container, Horizontal, Vertical
from textual.reactive import reactive
from textual.screen import Screen
from textual.widgets import Input, OptionList, Static, Switch
from textual.widgets.option_list import Option

from tui.core import EUXIS_HOME
from tui.core.runner import get_project_name
from tui.widgets.header import ETXHeader
from tui.widgets.shortcut_bar import ShortcutBar

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.app import EuxisApp


# Log level patterns and colors (2026 standard)
LOG_LEVEL_PATTERNS = {
    "ERROR": (r"\b(ERROR|FATAL|CRITICAL|EXCEPTION)\b", "bold red"),
    "WARN": (r"\b(WARN|WARNING)\b", "bold yellow"),
    "INFO": (r"\b(INFO)\b", "cyan"),
    "DEBUG": (r"\b(DEBUG|TRACE)\b", "dim"),
    "SUCCESS": (r"\b(SUCCESS|OK|PASS|COMPLETE)\b", "bold green"),
}

# Timestamp pattern for log parsing
TIMESTAMP_PATTERN = re.compile(r"(\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}:\d{2})")


def highlight_log_line(line: str, search_pattern: str | None = None) -> str:
    """Apply syntax highlighting to a log line.

    Args:
        line: Raw log line
        search_pattern: Optional regex pattern to highlight matches

    Returns:
        Rich-formatted log line
    """
    result = line

    # Highlight log levels
    for level, (pattern, color) in LOG_LEVEL_PATTERNS.items():
        result = re.sub(pattern, f"[{color}]\\1[/]", result, flags=re.IGNORECASE)

    # Highlight timestamps
    result = TIMESTAMP_PATTERN.sub(r"[dim]\1[/]", result)

    # Highlight search matches
    if search_pattern:
        try:
            result = re.sub(
                f"({search_pattern})",
                r"[bold on yellow]\1[/]",
                result,
                flags=re.IGNORECASE,
            )
        except re.error:
            pass  # Invalid regex, skip highlighting

    return result


class LogLine(Static):
    """A single log line with syntax highlighting."""

    def __init__(self, line: str, search_pattern: str | None = None, **kwargs: Any) -> None:
        highlighted = highlight_log_line(line, search_pattern)
        super().__init__(highlighted, **kwargs)


class LogContent(Vertical):
    """Scrollable log content with live tail support."""

    DEFAULT_CSS = """
    LogContent {
        height: 1fr;
        overflow-y: auto;
        background: $surface;
        padding: 0 1;
    }

    LogContent .log-line {
        height: auto;
        width: 100%;
    }

    LogContent .log-line-error {
        background: $error 10%;
    }
    """

    # Reactive properties
    auto_scroll: reactive[bool] = reactive(True)
    search_pattern: reactive[str] = reactive("")

    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self._lines: list[str] = []
        self._max_lines = 10000  # Memory limit
        self._tail_timer: object | None = None
        self._current_file: Path | None = None

    def clear(self) -> None:
        """Clear all log content."""
        self._lines = []
        self.remove_children()

    def add_line(self, line: str) -> None:
        """Add a single log line with highlighting."""
        self._lines.append(line)

        # Enforce memory limit
        if len(self._lines) > self._max_lines:
            self._lines = self._lines[-self._max_lines:]
            # Also trim widgets
            children = list(self.children)
            if len(children) > self._max_lines:
                for child in children[:-self._max_lines]:
                    child.remove()

        # Apply filter if active
        if self.search_pattern and not self._matches_filter(line):
            return

        # Determine CSS class based on log level
        css_class = "log-line"
        if re.search(r"\b(ERROR|FATAL|CRITICAL)\b", line, re.IGNORECASE):
            css_class = "log-line log-line-error"

        widget = LogLine(line, self.search_pattern, classes=css_class)
        self.mount(widget)

        # Auto-scroll to bottom
        if self.auto_scroll:
            self.scroll_end(animate=False)

    def add_lines(self, lines: list[str]) -> None:
        """Add multiple log lines efficiently."""
        for line in lines:
            self.add_line(line)

    def _matches_filter(self, line: str) -> bool:
        """Check if line matches current search filter."""
        if not self.search_pattern:
            return True
        try:
            return bool(re.search(self.search_pattern, line, re.IGNORECASE))
        except re.error:
            return self.search_pattern.lower() in line.lower()

    def watch_search_pattern(self) -> None:
        """Rebuild content when search pattern changes."""
        self._rebuild_with_filter()

    def _rebuild_with_filter(self) -> None:
        """Rebuild displayed content with current filter."""
        self.remove_children()
        for line in self._lines:
            if self._matches_filter(line):
                css_class = "log-line"
                if re.search(r"\b(ERROR|FATAL|CRITICAL)\b", line, re.IGNORECASE):
                    css_class = "log-line log-line-error"
                widget = LogLine(line, self.search_pattern, classes=css_class)
                self.mount(widget)

        if self.auto_scroll:
            self.scroll_end(animate=False)

    def load_file(self, path: Path, tail: bool = False) -> None:
        """Load log content from a file.

        Args:
            path: Path to log file
            tail: If True, start live tailing
        """
        self.clear()
        self._current_file = path

        if not path.exists():
            self.add_line("[dim]File not found[/]")
            return

        # Read existing content (streaming for large files)
        try:
            with open(path, "r", errors="replace") as f:
                # Read in chunks for memory efficiency
                chunk_size = 8192
                buffer = ""
                line_count = 0

                while True:
                    chunk = f.read(chunk_size)
                    if not chunk:
                        break

                    buffer += chunk
                    lines = buffer.split("\n")
                    buffer = lines[-1]  # Keep incomplete line

                    for line in lines[:-1]:
                        if line_count < self._max_lines:
                            self.add_line(line)
                            line_count += 1

                # Add final line
                if buffer and line_count < self._max_lines:
                    self.add_line(buffer)

                self._file_position = f.tell()

        except Exception as e:
            self.add_line(f"[red]Error reading file: {e}[/]")
            return

        # Start live tail if requested
        if tail:
            self.start_tail()

    def start_tail(self) -> None:
        """Start live tailing the current file."""
        if self._tail_timer is None and self._current_file:
            self._tail_timer = self.set_interval(0.5, self._check_for_updates)

    def stop_tail(self) -> None:
        """Stop live tailing."""
        if self._tail_timer:
            self._tail_timer.stop()
            self._tail_timer = None

    def _check_for_updates(self) -> None:
        """Check for new content in tailed file."""
        if not self._current_file or not self._current_file.exists():
            return

        try:
            with open(self._current_file, "r", errors="replace") as f:
                f.seek(self._file_position)
                new_content = f.read()
                if new_content:
                    for line in new_content.split("\n"):
                        if line:
                            self.add_line(line)
                    self._file_position = f.tell()
        except Exception:
            pass


class LogViewerScreen(Screen[None]):
    """Enhanced log viewer with tail, search, and filtering."""

    DEFAULT_CSS = """
    #log-viewer-container {
        layout: horizontal;
        height: 1fr;
    }

    #log-sidebar {
        width: 28;
        border-right: solid $surface;
        padding: 0;
    }

    #log-sidebar-header {
        height: 1;
        padding: 0 1;
        background: $panel;
        text-style: bold;
    }

    #log-list {
        height: 1fr;
    }

    #log-main {
        width: 1fr;
        padding: 0;
    }

    #log-toolbar {
        height: 3;
        layout: horizontal;
        padding: 0 1;
        background: $panel;
        border-bottom: solid $surface;
    }

    #log-search {
        width: 1fr;
        margin-right: 1;
    }

    #tail-toggle {
        width: auto;
        height: auto;
    }

    .toolbar-label {
        width: auto;
        padding: 1 1 0 0;
        color: $text-muted;
    }

    #log-status {
        height: 1;
        padding: 0 1;
        background: $panel;
        color: $text-muted;
    }
    """

    BINDINGS = [
        Binding("escape", "go_back", "Back"),
        Binding("ctrl+k", "app.command_palette", "Commands"),
        Binding("slash", "focus_search", "Search"),
        Binding("t", "toggle_tail", "Toggle Tail"),
        Binding("c", "clear_filter", "Clear Filter"),
        Binding("g", "scroll_top", "Top", show=False),
        Binding("G", "scroll_bottom", "Bottom", show=False),
    ]

    # Reactive properties
    tail_enabled: reactive[bool] = reactive(False)
    current_agent: reactive[str] = reactive("")

    def __init__(self, filter_agent: str | None = None, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self._initial_agent = filter_agent

    @property
    def euxis_app(self) -> EuxisApp:
        """Return the typed application instance."""
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        """Build the enhanced log viewer layout."""
        yield ETXHeader(id="header")

        with Container(id="log-viewer-container"):
            # Sidebar: Agent list
            with Vertical(id="log-sidebar"):
                yield Static("Agents", id="log-sidebar-header")
                yield OptionList(id="log-list")

            # Main: Log content with toolbar
            with Vertical(id="log-main"):
                # Toolbar: Search + Tail toggle
                with Horizontal(id="log-toolbar"):
                    yield Input(
                        placeholder="Search (regex)...",
                        id="log-search",
                    )
                    yield Static("Tail", classes="toolbar-label")
                    yield Switch(value=False, id="tail-toggle")

                # Log content
                yield LogContent(id="log-content")

                # Status bar
                yield Static("Select an agent to view logs", id="log-status")

        yield ShortcutBar([
            ("Esc", "Back"),
            ("/", "Search"),
            ("t", "Tail"),
            ("c", "Clear"),
            ("g/G", "Top/Bottom"),
        ])

    def on_mount(self) -> None:
        """Configure header and load log list."""
        header = self.query_one(ETXHeader)
        header.project = self.euxis_app.project_name
        header.branch = self.euxis_app.git_branch or ""
        header.provider = self.euxis_app.config.default_provider

        self._load_agent_list()

        # Auto-select initial agent if specified
        if self._initial_agent:
            self.call_later(lambda: self._select_agent(self._initial_agent))

    def _load_agent_list(self) -> None:
        """Load list of agents with logs."""
        option_list = self.query_one("#log-list", OptionList)
        project = get_project_name()

        # Check multiple log locations
        log_sources = [
            EUXIS_HOME / "data" / "projects" / project,
            EUXIS_HOME / "euxis-runtime" / "logs",
            EUXIS_HOME / "euxis-runtime" / "history",
        ]

        agents_found: dict[str, int] = {}

        for source in log_sources:
            if not source.exists():
                continue

            for entry in source.iterdir():
                if entry.is_dir():
                    # Count log files
                    log_files = list(entry.glob("*.log")) + list(entry.glob("*.md")) + list(entry.glob("*.jsonl"))
                    if log_files:
                        agent_id = entry.name
                        agents_found[agent_id] = agents_found.get(agent_id, 0) + len(log_files)

        if not agents_found:
            option_list.add_option(Option("No logs found", disabled=True))
            return

        # Add sorted agents to list
        for agent_id in sorted(agents_found.keys()):
            count = agents_found[agent_id]
            option_list.add_option(
                Option(f"{agent_id} ({count})", id=agent_id)
            )

    def _select_agent(self, agent_id: str) -> None:
        """Programmatically select an agent."""
        option_list = self.query_one("#log-list", OptionList)
        for i, option in enumerate(option_list._options):
            if option.id == agent_id:
                option_list.highlighted = i
                self._load_agent_logs(agent_id)
                break

    def on_option_list_option_selected(self, event: OptionList.OptionSelected) -> None:
        """Load logs for selected agent."""
        agent_id = event.option.id
        if agent_id:
            self._load_agent_logs(agent_id)

    def _load_agent_logs(self, agent_id: str) -> None:
        """Load logs for a specific agent."""
        self.current_agent = agent_id
        content = self.query_one("#log-content", LogContent)
        status = self.query_one("#log-status", Static)

        content.stop_tail()
        content.clear()

        project = get_project_name()

        # Find most recent log file for this agent
        log_paths = [
            EUXIS_HOME / "data" / "projects" / project / agent_id / "output",
            EUXIS_HOME / "euxis-runtime" / "logs" / agent_id,
            EUXIS_HOME / "euxis-runtime" / "history" / agent_id,
        ]

        latest_file: Path | None = None
        latest_time: float = 0

        for log_dir in log_paths:
            if not log_dir.exists():
                continue
            for log_file in log_dir.glob("*"):
                if log_file.is_file() and log_file.suffix in (".log", ".md", ".jsonl", ".txt"):
                    mtime = log_file.stat().st_mtime
                    if mtime > latest_time:
                        latest_time = mtime
                        latest_file = log_file

        if not latest_file:
            content.add_line(f"[yellow]No logs found for {agent_id}[/]")
            status.update(f"Agent: {agent_id} | No logs available")
            return

        # Load the file
        content.load_file(latest_file, tail=self.tail_enabled)

        # Update status
        file_size = latest_file.stat().st_size
        mod_time = datetime.fromtimestamp(latest_time).strftime("%Y-%m-%d %H:%M:%S")
        size_str = f"{file_size / 1024:.1f}KB" if file_size > 1024 else f"{file_size}B"
        status.update(
            f"Agent: {agent_id} | {latest_file.name} | {size_str} | Modified: {mod_time}"
        )

    def on_input_changed(self, event: Input.Changed) -> None:
        """Update search filter when input changes."""
        if event.input.id == "log-search":
            content = self.query_one("#log-content", LogContent)
            content.search_pattern = event.value

    def on_switch_changed(self, event: Switch.Changed) -> None:
        """Toggle live tail mode."""
        if event.switch.id == "tail-toggle":
            self.tail_enabled = event.value
            content = self.query_one("#log-content", LogContent)
            if event.value:
                content.start_tail()
                self.notify("Live tail enabled", timeout=2)
            else:
                content.stop_tail()
                self.notify("Live tail disabled", timeout=2)

    def action_go_back(self) -> None:
        """Return to the previous screen."""
        # Stop tail before leaving
        content = self.query_one("#log-content", LogContent)
        content.stop_tail()
        self.app.pop_screen()

    def action_focus_search(self) -> None:
        """Focus the search input."""
        self.query_one("#log-search", Input).focus()

    def action_toggle_tail(self) -> None:
        """Toggle live tail mode."""
        switch = self.query_one("#tail-toggle", Switch)
        switch.value = not switch.value

    def action_clear_filter(self) -> None:
        """Clear the search filter."""
        search = self.query_one("#log-search", Input)
        search.value = ""

    def action_scroll_top(self) -> None:
        """Scroll to top of logs."""
        content = self.query_one("#log-content", LogContent)
        content.auto_scroll = False
        content.scroll_home()

    def action_scroll_bottom(self) -> None:
        """Scroll to bottom of logs."""
        content = self.query_one("#log-content", LogContent)
        content.auto_scroll = True
        content.scroll_end()
