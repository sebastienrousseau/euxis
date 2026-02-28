# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Euxis TUI Approvals Screen (HITL Gatekeeper) with Gateway Integration."""

from __future__ import annotations

import json
import logging
from typing import Any, Dict, List

import httpx
from textual import work
from textual.app import ComposeResult
from textual.containers import Container, Horizontal, Vertical
from textual.screen import Screen
from textual.widgets import Button, Header, Footer, Static, DataTable, Label

LOGGER = logging.getLogger("euxis.tui.approvals")

class ApprovalsScreen(Screen):
    """Screen for reviewing and approving pending agent actions."""

    BINDINGS = [
        ("y", "approve_selected", "Approve (Y)"),
        ("n", "reject_selected", "Reject (N)"),
        ("r", "refresh", "Refresh"),
        ("escape", "app.pop_screen", "Back"),
    ]

    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.gateway_url = "http://127.0.0.2:18789" # Default gateway address
        self.pending_data: Dict[str, Any] = {}

    def compose(self) -> ComposeResult:
        yield Header()
        yield Container(
            Vertical(
                Label("Pending Agent Approvals", id="title"),
                DataTable(id="approvals_table"),
                Static("Select a run to see details.", id="approval_details"),
                Horizontal(
                    Button("Approve", variant="success", id="approve_btn"),
                    Button("Reject", variant="error", id="reject_btn"),
                    id="approval_actions"
                ),
                id="approvals_container"
            )
        )
        yield Footer()

    def on_mount(self) -> None:
        table = self.query_one("#approvals_table", DataTable)
        table.add_columns("Run ID", "Agent", "Requested", "Task Preview")
        table.cursor_type = "row"
        self.refresh_approvals()

    @work(exclusive=True, thread=True)
    async def refresh_approvals(self) -> None:
        """Fetch pending approvals from the Euxis Gateway."""
        try:
            async with httpx.AsyncClient() as client:
                response = await client.get(f"{self.gateway_url}/approvals", timeout=5)
                if response.status_code == 200:
                    data = response.json()
                    self.pending_data = data.get("approvals", {})
                    self.call_from_thread(self._update_table)
                else:
                    self.notify(f"Failed to fetch approvals: {response.status_code}", severity="error")
        except Exception as e:
            self.notify(f"Gateway connection error: {e}", severity="error")

    def _update_table(self) -> None:
        table = self.query_one("#approvals_table", DataTable)
        table.clear()
        for run_id, info in self.pending_data.items():
            content_preview = info.get("content", "")[:30] + "..."
            table.add_row(
                run_id, 
                info.get("agent_id", "unknown"),
                info.get("requested_at", "unknown"),
                content_preview,
                key=run_id
            )

    def on_data_table_row_selected(self, event: DataTable.RowSelected) -> None:
        """Update detail view when a row is selected."""
        run_id = str(event.row_key.value)
        info = self.pending_data.get(run_id, {})
        details = self.query_one("#approval_details", Static)
        
        details_text = (
            f"[bold]Run ID:[/] {run_id}\n"
            f"[bold]Agent:[/] {info.get('agent_id')}\n"
            f"[bold]Requested:[/] {info.get('requested_at')}\n"
            f"[bold]Task Goal:[/]\n{info.get('content')}\n\n"
            f"[bold]Identity Context:[/] {json.dumps(info.get('meta', {}))}"
        )
        details.update(details_text)

    def action_approve_selected(self) -> None:
        table = self.query_one("#approvals_table", DataTable)
        if table.cursor_row is not None:
            row_key = table.coordinate_to_cell_key(table.cursor_coordinate).row_key
            run_id = str(row_key.value)
            self.process_decision(run_id, "approve")

    def action_reject_selected(self) -> None:
        table = self.query_one("#approvals_table", DataTable)
        if table.cursor_row is not None:
            row_key = table.coordinate_to_cell_key(table.cursor_coordinate).row_key
            run_id = str(row_key.value)
            self.process_decision(run_id, "reject")

    def on_button_pressed(self, event: Button.Pressed) -> None:
        table = self.query_one("#approvals_table", DataTable)
        if table.cursor_row is None:
            return
            
        row_key = table.coordinate_to_cell_key(table.cursor_coordinate).row_key
        run_id = str(row_key.value)
        
        if event.button.id == "approve_btn":
            self.process_decision(run_id, "approve")
        elif event.button.id == "reject_btn":
            self.process_decision(run_id, "reject")

    @work(exclusive=True, thread=True)
    async def process_decision(self, run_id: str, decision: str) -> None:
        """Send approval/rejection decision to the gateway."""
        try:
            async with httpx.AsyncClient() as client:
                url = f"{self.gateway_url}/approvals/{run_id}/{decision}"
                response = await client.post(url, timeout=5)
                if response.status_code == 200:
                    self.notify(f"Successfully {decision}d {run_id}")
                    self.refresh_approvals()
                else:
                    self.notify(f"Failed to {decision} {run_id}: {response.status_code}", severity="error")
        except Exception as e:
            self.notify(f"Error sending decision: {e}", severity="error")

    def action_refresh(self) -> None:
        self.refresh_approvals()
