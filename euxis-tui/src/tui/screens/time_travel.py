# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Time-Travel Debugger screen for mission replay and checkpoint recovery.

Provides durable execution visibility by allowing users to:
- Scrub through mission timeline with h/l keys
- View agent state at any point in time
- Resume failed missions from checkpoints
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, Any

from textual.binding import Binding
from textual.containers import Container, Horizontal, Vertical, VerticalScroll
from textual.reactive import reactive
from textual.screen import Screen
from textual.widgets import Button, Label, ListItem, ListView, ProgressBar, Static

from tui.i18n import _
from tui.widgets.header import ETXHeader
from tui.widgets.shortcut_bar import ShortcutBar

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.app import EuxisApp


@dataclass
class MissionEvent:
    """A single event from the flight recorder."""

    type: str
    timestamp: str
    agent: str
    session: str
    data: dict[str, Any]
    index: int = 0


@dataclass
class MissionInfo:
    """Metadata about a recorded mission."""

    mission_id: str
    description: str
    started: str
    completed: str | None
    status: str
    event_count: int


def get_history_dir() -> Path:
    """Get the flight recorder history directory."""
    euxis_home = os.environ.get("EUXIS_HOME", os.path.expanduser("~/.euxis"))
    return Path(euxis_home) / "euxis-runtime" / "history"


def list_missions() -> list[MissionInfo]:
    """List all recorded missions."""
    history_dir = get_history_dir()
    if not history_dir.exists():
        return []

    missions = []
    for file in sorted(history_dir.glob("*.jsonl"), reverse=True):
        try:
            with open(file) as f:
                first_line = f.readline()
                if not first_line:
                    continue
                start = json.loads(first_line)

                # Find completion event
                f.seek(0)
                lines = f.readlines()
                event_count = len(lines)
                completed = None
                status = "in_progress"

                for line in reversed(lines):
                    try:
                        event = json.loads(line)
                        if event.get("type") == "mission_complete":
                            completed = event.get("timestamp")
                            status = event.get("status", "unknown")
                            break
                    except json.JSONDecodeError:
                        continue

                missions.append(MissionInfo(
                    mission_id=start.get("mission_id", file.stem),
                    description=start.get("description", ""),
                    started=start.get("timestamp", ""),
                    completed=completed,
                    status=status,
                    event_count=event_count,
                ))
        except (json.JSONDecodeError, IOError):
            continue

    return missions


def load_mission_events(mission_id: str) -> list[MissionEvent]:
    """Load all events for a mission."""
    history_dir = get_history_dir()
    file = history_dir / f"{mission_id}.jsonl"

    if not file.exists():
        return []

    events = []
    with open(file) as f:
        for idx, line in enumerate(f):
            try:
                data = json.loads(line)
                events.append(MissionEvent(
                    type=data.get("type", "unknown"),
                    timestamp=data.get("timestamp", ""),
                    agent=data.get("agent", ""),
                    session=data.get("session", ""),
                    data=data.get("data", {}),
                    index=idx,
                ))
            except json.JSONDecodeError:
                continue

    return events


class EventCard(Static):
    """A card displaying a single mission event."""

    def __init__(self, event: MissionEvent, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.event = event

    def compose(self) -> ComposeResult:
        """Render the event card."""
        type_colors = {
            "mission_start": "green",
            "mission_complete": "green" if self.event.data.get("status") == "success" else "red",
            "thought": "cyan",
            "tool_call": "yellow",
            "tool_result": "blue",
            "checkpoint": "magenta",
            "cost": "red",
            "error": "red",
            "agent_start": "cyan",
            "agent_complete": "green",
        }
        color = type_colors.get(self.event.type, "white")

        # Format timestamp (show just time portion)
        time_str = self.event.timestamp.split("T")[1][:8] if "T" in self.event.timestamp else self.event.timestamp

        yield Static(f"[{color}]{self.event.type}[/] [{time_str}]", classes="event-type")

        # Render event-specific content
        if self.event.type == "thought":
            content = self.event.data.get("content", "")[:100]
            yield Static(f'  [italic]"{content}"[/]', classes="event-content")
        elif self.event.type == "tool_call":
            tool = self.event.data.get("tool", "unknown")
            yield Static(f"  Tool: [bold]{tool}[/]", classes="event-content")
        elif self.event.type == "tool_result":
            tool = self.event.data.get("tool", "unknown")
            status = self.event.data.get("status", "unknown")
            color = "green" if status == "success" else "red"
            yield Static(f"  {tool} [{color}]{status}[/]", classes="event-content")
        elif self.event.type == "cost":
            cost = self.event.data.get("cost_usd", 0)
            model = self.event.data.get("model", "unknown")
            yield Static(f"  {model}: [red]${cost:.4f}[/]", classes="event-content")
        elif self.event.type == "checkpoint":
            name = self.event.data.get("name", "unnamed")
            yield Static(f"  Checkpoint: [magenta]{name}[/]", classes="event-content")
        elif self.event.type == "mission_complete":
            status = self.event.data.get("status", "unknown")
            summary = self.event.data.get("summary", "")[:60]
            color = "green" if status == "success" else "red"
            yield Static(f"  Status: [{color}]{status}[/]", classes="event-content")
            if summary:
                yield Static(f"  {summary}", classes="event-content")


class TimeTravelScreen(Screen[None]):
    """Time-Travel Debugger for mission replay and recovery."""

    BINDINGS = [
        Binding("escape", "go_back", "Back"),
        Binding("h", "scrub_left", "Earlier", show=False),
        Binding("l", "scrub_right", "Later", show=False),
        Binding("left", "scrub_left", "Earlier", show=False),
        Binding("right", "scrub_right", "Later", show=False),
        Binding("r", "retry_checkpoint", "Retry"),
        Binding("enter", "select_mission", "Select"),
    ]

    current_index: reactive[int] = reactive(0)
    total_events: reactive[int] = reactive(0)

    def __init__(self, mission_id: str | None = None, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.mission_id = mission_id
        self.events: list[MissionEvent] = []
        self.missions: list[MissionInfo] = []

    @property
    def euxis_app(self) -> EuxisApp:
        """Return typed app instance."""
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        """Build the time-travel UI."""
        yield ETXHeader(id="header")

        with Container(id="time-travel-container"):
            # Left panel: Mission list
            with Vertical(id="mission-list-panel"):
                yield Static("[bold]Recorded Missions[/]", classes="panel-title")
                yield ListView(id="mission-list")

            # Right panel: Event timeline
            with Vertical(id="event-panel"):
                yield Static("[bold]Mission Timeline[/]", classes="panel-title")

                # Timeline scrubber
                with Horizontal(id="timeline-scrubber"):
                    yield Static("", id="timeline-position")
                    yield ProgressBar(total=100, show_eta=False, id="timeline-bar")

                # Event list
                yield VerticalScroll(id="event-scroll")

                # Checkpoint actions
                with Horizontal(id="checkpoint-actions"):
                    yield Button("Retry from Checkpoint", id="btn-retry", variant="warning")
                    yield Button("Export Timeline", id="btn-export", variant="default")

        yield ShortcutBar([
            ("h/l", "Scrub"),
            ("r", "Retry"),
            ("Enter", "Select"),
            ("Esc", "Back"),
        ])

    def on_mount(self) -> None:
        """Load missions and configure UI."""
        header = self.query_one(ETXHeader)
        header.project = _("Time-Travel Debugger")

        self._load_missions()

        if self.mission_id:
            self._load_mission(self.mission_id)

    def _load_missions(self) -> None:
        """Load mission list."""
        self.missions = list_missions()
        mission_list = self.query_one("#mission-list", ListView)
        mission_list.clear()

        for mission in self.missions:
            status_icon = "" if mission.status == "success" else "" if mission.status == "error" else ""
            label = f"{status_icon} {mission.mission_id[:20]}"
            if mission.description:
                label += f" - {mission.description[:30]}"
            mission_list.append(ListItem(Label(label), id=f"mission-{mission.mission_id}"))

        if not self.missions:
            mission_list.append(ListItem(Label("[dim]No recorded missions[/]")))

    def _load_mission(self, mission_id: str) -> None:
        """Load events for a specific mission."""
        self.mission_id = mission_id
        self.events = load_mission_events(mission_id)
        self.total_events = len(self.events)
        self.current_index = 0

        self._render_events()
        self._update_timeline()

    def _render_events(self) -> None:
        """Render events around current index."""
        scroll = self.query_one("#event-scroll", VerticalScroll)
        scroll.remove_children()

        if not self.events:
            scroll.mount(Static("[dim]No events to display[/]"))
            return

        # Show events around current position (context window)
        start = max(0, self.current_index - 5)
        end = min(len(self.events), self.current_index + 15)

        for i, event in enumerate(self.events[start:end]):
            actual_idx = start + i
            card = EventCard(event, id=f"event-{actual_idx}")
            if actual_idx == self.current_index:
                card.add_class("current-event")
            scroll.mount(card)

    def _update_timeline(self) -> None:
        """Update timeline position display."""
        position = self.query_one("#timeline-position", Static)
        bar = self.query_one("#timeline-bar", ProgressBar)

        if self.total_events > 0:
            position.update(f"[bold]{self.current_index + 1}[/] / {self.total_events}")
            bar.progress = int((self.current_index / max(1, self.total_events - 1)) * 100)
        else:
            position.update("0 / 0")
            bar.progress = 0

    def watch_current_index(self, new_index: int) -> None:
        """React to timeline position changes."""
        self._render_events()
        self._update_timeline()

    def action_scrub_left(self) -> None:
        """Move earlier in timeline."""
        if self.current_index > 0:
            self.current_index -= 1

    def action_scrub_right(self) -> None:
        """Move later in timeline."""
        if self.current_index < self.total_events - 1:
            self.current_index += 1

    def action_select_mission(self) -> None:
        """Select highlighted mission from list."""
        mission_list = self.query_one("#mission-list", ListView)
        if mission_list.highlighted_child:
            item_id = mission_list.highlighted_child.id or ""
            if item_id.startswith("mission-"):
                mission_id = item_id[8:]  # Remove "mission-" prefix
                self._load_mission(mission_id)

    def action_retry_checkpoint(self) -> None:
        """Retry from the nearest checkpoint."""
        if not self.events:
            self.notify("No mission loaded", severity="warning")
            return

        # Find nearest checkpoint at or before current position
        checkpoint = None
        for i in range(self.current_index, -1, -1):
            if self.events[i].type == "checkpoint":
                checkpoint = self.events[i]
                break

        if checkpoint:
            name = checkpoint.data.get("name", "unnamed")
            self.notify(f"Would retry from checkpoint: {name}", title="Checkpoint Recovery")
            # TODO: Implement actual checkpoint recovery via mesh
        else:
            self.notify("No checkpoint found before current position", severity="warning")

    def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handle button clicks."""
        if event.button.id == "btn-retry":
            self.action_retry_checkpoint()
        elif event.button.id == "btn-export":
            if self.mission_id:
                history_dir = get_history_dir()
                file = history_dir / f"{self.mission_id}.jsonl"
                self.notify(f"Timeline: {file}", title="Export Path")

    def on_list_view_selected(self, event: ListView.Selected) -> None:
        """Handle mission selection from list."""
        item_id = event.item.id or ""
        if item_id.startswith("mission-"):
            mission_id = item_id[8:]
            self._load_mission(mission_id)

    def action_go_back(self) -> None:
        """Return to previous screen."""
        self.app.pop_screen()
