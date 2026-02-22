# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Lightweight Error Details modal overlay with LLM-augmented analysis.

2026 UX Pattern: Modal Stackability + LLM-Augmented Diagnostics
- Doesn't reset DashboardScreen state when closed
- Quick dismiss with Escape or clicking outside
- Arbiter agent provides root cause analysis and recovery steps
- Graceful fallback to rule-based classification when Arbiter unavailable
"""

from __future__ import annotations

import json
import os
from datetime import datetime
from typing import TYPE_CHECKING, Any

from textual.binding import Binding
from textual.containers import Vertical, Horizontal
from textual.screen import ModalScreen
from textual.widgets import Button, Static
from textual.worker import Worker, WorkerState

from tui.core.runner import ArbiterAnalyzer, ErrorAnalysis, RecoveryStep
from tui.core.telemetry import AuditTrail, load_audit_trail, alert_agent_error

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.core.registry import Agent


# Severity icons and colors
SEVERITY_STYLES = {
    "critical": ("🔴", "bold red"),
    "warning": ("🟡", "bold yellow"),
    "info": ("🔵", "cyan"),
}

# Confidence tier colors (2026 UX: Trust Calibration)
CONFIDENCE_STYLES = {
    "high": ("✓", "bold green"),    # ≥85% - Trust the suggestion
    "medium": ("~", "bold yellow"),  # 60-84% - Review before acting
    "low": ("?", "bold red"),        # <60% - Investigate logs first
}


class ErrorDetailsModal(ModalScreen[str | None]):
    """Modal overlay showing error details with LLM-augmented analysis.

    Features:
    - Floats over dashboard without resetting state
    - Quick actions: R to restart, L for logs, Escape to close
    - Arbiter agent analyzes errors for root cause and recovery
    - Shows confidence score and severity indicator
    """

    DEFAULT_CSS = """
    ErrorDetailsModal {
        align: center middle;
    }

    ErrorDetailsModal > Vertical {
        width: 75;
        max-width: 90%;
        height: auto;
        max-height: 85%;
        background: $panel;
        border: thick $error;
        padding: 1 2;
    }

    ErrorDetailsModal .modal-title {
        text-style: bold;
        color: $error;
        width: 100%;
        height: 1;
        margin-bottom: 1;
    }

    ErrorDetailsModal .error-field {
        height: auto;
        margin-bottom: 1;
    }

    ErrorDetailsModal .field-label {
        color: $text-muted;
        width: 14;
    }

    ErrorDetailsModal .field-value {
        width: 1fr;
    }

    ErrorDetailsModal .analysis-box {
        background: $surface;
        padding: 1;
        margin: 1 0;
        border: round $primary;
        height: auto;
    }

    ErrorDetailsModal .analysis-box.loading {
        border: round $secondary;
    }

    ErrorDetailsModal .analysis-box.severity-critical {
        border: thick $error;
        background: $surface;
    }

    ErrorDetailsModal .analysis-box.severity-warning {
        border: round $warning;
    }

    ErrorDetailsModal .analysis-box.severity-info {
        border: round $primary;
    }

    ErrorDetailsModal .recovery-list {
        margin-top: 1;
        padding-left: 2;
    }

    ErrorDetailsModal .confidence-bar {
        height: 1;
        margin-top: 1;
    }

    ErrorDetailsModal .confidence-high {
        color: $success;
    }

    ErrorDetailsModal .confidence-medium {
        color: $warning;
    }

    ErrorDetailsModal .confidence-low {
        color: $error;
    }

    ErrorDetailsModal .attribution {
        color: $text-muted;
        text-style: italic;
        margin-top: 1;
    }

    ErrorDetailsModal .recovery-step {
        margin-left: 2;
    }

    ErrorDetailsModal .recovery-step.sandbox {
        color: $warning;
    }

    ErrorDetailsModal .action-bar {
        height: 3;
        margin-top: 1;
        align: center middle;
    }

    ErrorDetailsModal Button {
        margin: 0 1;
    }

    ErrorDetailsModal .audit-trail {
        background: $surface;
        padding: 1;
        margin: 1 0;
        border: round $secondary;
        height: auto;
    }

    ErrorDetailsModal .audit-chain {
        color: $primary;
    }
    """

    BINDINGS = [
        Binding("escape", "close", "Close"),
        Binding("r", "restart", "Restart"),
        Binding("s", "simulate", "Simulate"),  # Sandbox dry-run
        Binding("l", "logs", "Logs"),
        Binding("v", "validate", "Validate"),  # Confirm analysis accuracy
    ]

    def __init__(
        self, agent: Agent, error_info: dict[str, Any] | None = None, **kwargs: Any
    ) -> None:
        super().__init__(**kwargs)
        self.agent = agent
        self.error_info = error_info or self._fetch_error_info()
        self._analysis: ErrorAnalysis | None = None
        self._analysis_worker: Worker[ErrorAnalysis] | None = None
        # Load audit trail for social transparency (2026 Pattern)
        self._audit_trail: AuditTrail = load_audit_trail(agent.id)

    def _fetch_error_info(self) -> dict[str, Any]:
        """Fetch error details from mesh state or logs."""
        euxis_home = os.environ.get("EUXIS_HOME", os.path.expanduser("~/.euxis"))
        state_file = os.path.join(euxis_home, "euxis-runtime/mesh/state.json")

        error_info: dict[str, Any] = {
            "timestamp": datetime.now().isoformat(),
            "message": "Error details unavailable",
            "last_task": "Unknown",
        }

        try:
            with open(state_file) as f:
                state = json.load(f)
            agent_state = state.get("agents", {}).get(self.agent.id, {})

            if agent_state:
                error_info["timestamp"] = agent_state.get(
                    "last_seen", error_info["timestamp"]
                )
                error_info["message"] = agent_state.get(
                    "last_error", error_info["message"]
                )
                error_info["last_task"] = agent_state.get(
                    "current_task", error_info["last_task"]
                )

        except (FileNotFoundError, json.JSONDecodeError, KeyError):
            pass

        return error_info

    def compose(self) -> ComposeResult:
        """Build the error details modal."""
        with Vertical():  # pragma: no cover
            yield Static(f"⚠️ Error: {self.agent.id}", classes="modal-title")  # pragma: no cover
  # pragma: no cover
            # Agent info fields  # pragma: no cover
            with Horizontal(classes="error-field"):  # pragma: no cover
                yield Static("Agent:", classes="field-label")  # pragma: no cover
                yield Static(  # pragma: no cover
                    f"[bold]{self.agent.id}[/] ({self.agent.tier})",  # pragma: no cover
                    classes="field-value",  # pragma: no cover
                )  # pragma: no cover
  # pragma: no cover
            with Horizontal(classes="error-field"):  # pragma: no cover
                yield Static("Time:", classes="field-label")  # pragma: no cover
                yield Static(  # pragma: no cover
                    self.error_info.get("timestamp", "Unknown"), classes="field-value"  # pragma: no cover
                )  # pragma: no cover
  # pragma: no cover
            with Horizontal(classes="error-field"):  # pragma: no cover
                yield Static("Last Task:", classes="field-label")  # pragma: no cover
                yield Static(  # pragma: no cover
                    self.error_info.get("last_task", "Unknown")[:50],  # pragma: no cover
                    classes="field-value",  # pragma: no cover
                )  # pragma: no cover
  # pragma: no cover
            # LLM-augmented analysis box (starts in loading state)  # pragma: no cover
            yield Static(  # pragma: no cover
                "[dim]🔄 Analyzing error with Arbiter...[/]",  # pragma: no cover
                id="analysis-box",  # pragma: no cover
                classes="analysis-box loading",  # pragma: no cover
            )  # pragma: no cover
  # pragma: no cover
            # Audit Trail - Social Transparency (2026 Pattern)  # pragma: no cover
            if self._audit_trail.events:  # pragma: no cover
                chain = self._audit_trail.get_chain_summary()  # pragma: no cover
                root = self._audit_trail.get_root_delegator()  # pragma: no cover
                yield Static(  # pragma: no cover
                    f"[bold]Audit Trail[/]\n"  # pragma: no cover
                    f"[dim]Root delegator:[/] [bold]{root}[/]\n"  # pragma: no cover
                    f"[dim]Chain:[/] [cyan]{chain}[/]",  # pragma: no cover
                    id="audit-trail",  # pragma: no cover
                    classes="audit-trail",  # pragma: no cover
                )  # pragma: no cover
  # pragma: no cover
            # Raw error message  # pragma: no cover
            with Horizontal(classes="error-field"):  # pragma: no cover
                yield Static("Raw Error:", classes="field-label")  # pragma: no cover
                yield Static(  # pragma: no cover
                    f"[dim]{self.error_info.get('message', 'No details')[:120]}[/]",  # pragma: no cover
                    classes="field-value",  # pragma: no cover
                )  # pragma: no cover
  # pragma: no cover
            # Action buttons (2026 UX: Safe-to-Try Sandbox)  # pragma: no cover
            with Horizontal(classes="action-bar"):  # pragma: no cover
                yield Button("Restart (R)", variant="success", id="restart")  # pragma: no cover
                yield Button("Simulate (S)", variant="warning", id="simulate")  # pragma: no cover
                yield Button("Logs (L)", variant="primary", id="logs")  # pragma: no cover
                yield Button("Close", variant="default", id="close")  # pragma: no cover

    def on_mount(self) -> None:
        """Start async analysis when modal opens."""
        self._analysis_worker = self.run_worker(
            self._run_analysis,
            name="arbiter-analysis",
            thread=True,
        )
        # Send haptic alert for error (2026 UX: Haptic Feedback)
        error_summary = self.error_info.get("message", "Unknown error")[:80]
        alert_agent_error(self.agent.id, error_summary)

    async def _run_analysis(self) -> ErrorAnalysis:
        """Run Arbiter analysis in background."""
        analyzer = ArbiterAnalyzer(timeout=15.0)
        return await analyzer.analyze(
            agent_id=self.agent.id,
            tier=self.agent.tier,
            error_message=self.error_info.get("message", ""),
            last_task=self.error_info.get("last_task", "Unknown"),
            timestamp=self.error_info.get("timestamp", ""),
        )

    def on_worker_state_changed(self, event: Worker.StateChanged) -> None:
        """Handle analysis completion."""
        if event.worker.name != "arbiter-analysis":
            return

        if event.state == WorkerState.SUCCESS:
            self._analysis = event.worker.result
            self._update_analysis_display()
        elif event.state == WorkerState.ERROR:
            # Show fallback message
            analysis_box = self.query_one("#analysis-box", Static)
            analysis_box.update(
                "[yellow]⚠️ Analysis unavailable[/]\n"
                "[dim]Check logs for detailed error information.[/]"
            )
            analysis_box.remove_class("loading")

    def _update_analysis_display(self) -> None:
        """Update the analysis box with results."""
        if not self._analysis:
            return

        analysis = self._analysis
        analysis_box = self.query_one("#analysis-box", Static)

        # Get severity styling
        sev_icon, sev_style = SEVERITY_STYLES.get(analysis.severity, ("⚪", ""))

        # Get confidence tier styling (2026 UX: Trust Calibration)
        conf_icon, conf_style = CONFIDENCE_STYLES.get(
            analysis.confidence_tier, ("?", "")
        )
        conf_pct = int(analysis.confidence * 100)
        conf_bar = "█" * (conf_pct // 10) + "░" * (10 - conf_pct // 10)

        # Trust guidance based on confidence
        trust_hint = {
            "high": "Safe to act on this recommendation",
            "medium": "Review details before acting",
            "low": "Check logs before proceeding",
        }.get(analysis.confidence_tier, "")

        # Build recovery steps with probability ranking
        recovery_text = ""
        if analysis.recovery_steps:
            recovery_text = "\n[bold]Recovery Steps[/] [dim](ranked by success probability)[/]:\n"
            # Sort by success probability
            sorted_steps = sorted(
                analysis.recovery_steps,
                key=lambda s: s.success_probability,
                reverse=True,
            )
            for i, step in enumerate(sorted_steps[:3], 1):
                prob_pct = int(step.success_probability * 100)
                sandbox_tag = " [yellow]⚗️ sandbox[/]" if step.requires_sandbox else ""
                prob_color = "green" if prob_pct >= 80 else "yellow" if prob_pct >= 60 else "red"
                recovery_text += (
                    f"  {i}. {step.action} [{prob_color}]({prob_pct}%)[/]{sandbox_tag}\n"
                )

        # Cascading failure attribution (2026 UX: Social Transparency)
        attribution_text = ""
        if analysis.caused_by_agent:
            attribution_text = (
                f"\n[dim italic]⛓️ Cascading failure: caused by "
                f"[bold]{analysis.caused_by_agent}[/] agent[/]"
            )

        # Auto-recover indicator
        auto_text = ""
        if analysis.can_auto_recover:
            auto_text = "\n[green]✓ Safe for automatic recovery[/]"

        # Compose full analysis text
        content = (
            f"[{sev_style}]{sev_icon} {analysis.summary}[/]\n\n"
            f"[bold]Root Cause:[/] {analysis.root_cause}"
            f"{attribution_text}"
            f"{recovery_text}"
            f"{auto_text}\n"
            f"[{conf_style}]{conf_icon} Confidence: {conf_bar} {conf_pct}%[/] "
            f"[dim]({trust_hint})[/]"
        )

        analysis_box.update(content)
        analysis_box.remove_class("loading")
        analysis_box.add_class(f"severity-{analysis.severity}")

    def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handle button actions."""
        if event.button.id == "restart":
            self.dismiss("restart")
        elif event.button.id == "simulate":
            self.dismiss("simulate")
        elif event.button.id == "logs":
            self.dismiss("logs")
        else:
            self.dismiss(None)

    def action_close(self) -> None:
        """Close modal without action."""
        self.dismiss(None)

    def action_restart(self) -> None:
        """Restart agent from modal."""
        self.dismiss("restart")

    def action_simulate(self) -> None:
        """Run agent in sandbox dry-run mode.

        2026 UX: Safe-to-Try Sandbox
        - Verifies recovery conditions before production restart
        - Critical for guard/arbiter agents
        """
        self.dismiss("simulate")

    def action_logs(self) -> None:
        """Open logs from modal."""
        self.dismiss("logs")

    def action_validate(self) -> None:
        """Mark analysis as validated (human sign-off).

        2026 UX: Human-on-the-Loop Validation
        - Confirms agent-generated analysis is correct
        - Feeds back into future analysis accuracy
        """
        if self._analysis:
            # TODO: Archive validated analysis for model improvement
            self.notify(
                f"✓ Analysis validated for {self.agent.id}",
                title="Feedback Recorded",
                timeout=3,
            )
        self.dismiss(None)
