# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Playbook browser and launcher screen."""

from __future__ import annotations

import json
from typing import TYPE_CHECKING

from textual.containers import Container, Horizontal
from textual.screen import Screen
from textual.widgets import OptionList
from textual.widgets.option_list import Option

from tui.core import EUXIS_HOME
from tui.widgets.header import ETXHeader
from tui.widgets.output_panel import OutputPanel
from tui.widgets.shortcut_bar import ShortcutBar

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.app import EuxisApp


class PlaybookScreen(Screen[None]):
    """Browse and inspect available playbooks."""

    BINDINGS = [
        ("escape", "go_back", "Back"),
        ("ctrl+k", "app.command_palette", "Commands"),
    ]

    @property
    def euxis_app(self) -> EuxisApp:
        """Return the typed application instance."""
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        """Build the playbook browser layout."""
        yield ETXHeader(id="header")
        with Container(id="log-viewer"), Horizontal():
            yield OptionList(id="log-list")
            yield OutputPanel(id="log-content")

        yield ShortcutBar()

    def on_mount(self) -> None:
        """Configure header and load playbook list."""
        header = self.query_one(ETXHeader)
        header.project = self.euxis_app.project_name
        header.branch = self.euxis_app.git_branch or ""

        self._load_playbooks()

    def _load_playbooks(self) -> None:
        option_list = self.query_one("#log-list", OptionList)
        playbook_dir = EUXIS_HOME / "euxis-core" / "config" / "playbooks"

        if not playbook_dir.exists():
            option_list.add_option(Option("No playbooks found", disabled=True))
            return

        for pb_file in sorted(playbook_dir.glob("*.json")):
            try:
                data = json.loads(pb_file.read_text())
                name = data.get("name", pb_file.stem)
                option_list.add_option(Option(name, id=pb_file.stem))
            except (json.JSONDecodeError, KeyError):
                option_list.add_option(Option(pb_file.stem, id=pb_file.stem))

    def on_option_list_option_selected(self, event: OptionList.OptionSelected) -> None:
        """Display the selected playbook's details and gate structure."""
        pb_id = event.option.id
        if not pb_id:
            return

        output = self.query_one("#log-content", OutputPanel)
        output.clear()

        pb_path = EUXIS_HOME / "euxis-core" / "config" / "playbooks" / f"{pb_id}.json"
        if not pb_path.exists():
            output.write_status("Playbook not found", "red")
            return

        try:
            data = json.loads(pb_path.read_text())
        except json.JSONDecodeError:
            output.write_status("Invalid playbook JSON", "red")
            return

        # Display playbook details
        output.write_status(f"Playbook: {data.get('name', pb_id)}", "cyan")
        output.write_status(f"Version: {data.get('version', 'unknown')}", "dim")

        if "description" in data:
            output.write_line("")
            output.write_line(data["description"])

        # Show gates
        gates = data.get("gates", [])
        if gates:
            output.write_separator()
            output.write_status(f"{len(gates)} Quality Gates:", "yellow")
            for gate in gates:
                gate_id = gate.get("id", "?")
                gate_name = gate.get("name", "Unknown")
                checkpoint = gate.get("checkpoint", "")
                delegates = gate.get("delegates", [])
                delegate_names = [d.get("agent", "?") for d in delegates]

                output.write_line(f"  Gate {gate_id}: {gate_name}")
                if checkpoint:
                    output.write_line(f"    Checkpoint: {checkpoint}")
                if delegate_names:
                    output.write_line(f"    Delegates: {', '.join(delegate_names)}")

    def action_go_back(self) -> None:
        """Return to the previous screen."""
        self.app.pop_screen()
