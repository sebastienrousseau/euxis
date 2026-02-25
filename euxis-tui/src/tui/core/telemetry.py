# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Telemetry and observability for Euxis TUI.

2026 Patterns:
- OTLP-Friendly: Export metrics in OpenTelemetry format for SRE integration
- Haptic Feedback: macOS terminal notifications for budget alerts
- Audit Trail: Track delegation chains for social transparency
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import time
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import TYPE_CHECKING, Any

from tui.core import EUXIS_HOME

if TYPE_CHECKING:
    pass


# =============================================================================
# AUDIT TRAIL — Social Transparency (2026 Pattern)
# =============================================================================


@dataclass
class DelegationEvent:
    """A single delegation in the audit trail."""

    timestamp: str
    from_agent: str
    to_agent: str
    task: str
    reason: str = ""


@dataclass
class AuditTrail:
    """Track delegation chains for social transparency.

    Shows which supervisor agent originally delegated the task that failed,
    enabling root cause analysis across the agent hierarchy.
    """

    agent_id: str
    events: list[DelegationEvent] = field(default_factory=list)
    error_timestamp: str | None = None
    error_message: str | None = None

    def add_delegation(
        self,
        from_agent: str,
        to_agent: str,
        task: str,
        reason: str = "",
    ) -> None:
        """Record a delegation event."""
        self.events.append(
            DelegationEvent(
                timestamp=datetime.now().isoformat(),
                from_agent=from_agent,
                to_agent=to_agent,
                task=task[:200],
                reason=reason,
            )
        )

    def get_root_delegator(self) -> str | None:
        """Get the original supervisor that started the delegation chain."""
        if not self.events:
            return None
        return self.events[0].from_agent

    def get_chain_summary(self) -> str:
        """Get a human-readable summary of the delegation chain."""
        if not self.events:
            return "No delegation chain recorded"

        chain = [self.events[0].from_agent]
        for event in self.events:
            chain.append(event.to_agent)

        return " → ".join(chain)

    def to_dict(self) -> dict[str, Any]:
        """Export audit trail as dictionary for OTLP or logging."""
        return {
            "agent_id": self.agent_id,
            "root_delegator": self.get_root_delegator(),
            "chain": self.get_chain_summary(),
            "events": [
                {
                    "timestamp": e.timestamp,
                    "from": e.from_agent,
                    "to": e.to_agent,
                    "task": e.task,
                    "reason": e.reason,
                }
                for e in self.events
            ],
            "error_timestamp": self.error_timestamp,
            "error_message": self.error_message,
        }


def load_audit_trail(agent_id: str) -> AuditTrail:
    """Load audit trail from mesh state for an agent."""
    state_file = EUXIS_HOME / "euxis-runtime" / "mesh" / "state.json"
    trail = AuditTrail(agent_id=agent_id)

    try:
        with open(state_file) as f:
            state = json.load(f)
        agent_state = state.get("agents", {}).get(agent_id, {})

        # Parse delegation history if present
        delegations = agent_state.get("delegation_history", [])
        for d in delegations:
            trail.events.append(
                DelegationEvent(
                    timestamp=d.get("timestamp", ""),
                    from_agent=d.get("from", ""),
                    to_agent=d.get("to", agent_id),
                    task=d.get("task", ""),
                    reason=d.get("reason", ""),
                )
            )

        # Capture error context
        if agent_state.get("status") == "error":
            trail.error_timestamp = agent_state.get("last_seen")
            trail.error_message = agent_state.get("last_error")

    except (FileNotFoundError, json.JSONDecodeError, KeyError):
        pass

    return trail


# =============================================================================
# OTLP-FRIENDLY TELEMETRY — SRE Integration (2026 Pattern)
# =============================================================================


@dataclass
class AgentMetrics:
    """Metrics for a single agent, OTLP-compatible."""

    agent_id: str
    tier: str
    supervision_tier: str  # supervisor, coordinator, specialist

    # Cost metrics
    token_cost_usd: float = 0.0
    token_budget_usd: float = 2.0
    budget_utilization_pct: float = 0.0

    # Performance metrics
    latency_ms: int = 0
    request_count: int = 0
    error_count: int = 0

    # State
    status: str = "idle"  # idle, active, error
    last_updated: str = ""

    def to_otlp_attributes(self) -> dict[str, Any]:
        """Export as OTLP-compatible attributes."""
        return {
            "euxis.agent.id": self.agent_id,
            "euxis.agent.tier": self.tier,
            "euxis.agent.supervision_tier": self.supervision_tier,
            "euxis.agent.status": self.status,
        }

    def to_otlp_metrics(self) -> list[dict[str, Any]]:
        """Export as OTLP-compatible metric data points."""
        timestamp = int(time.time() * 1_000_000_000)  # nanoseconds
        attrs = self.to_otlp_attributes()

        return [
            {
                "name": "euxis.agent.token_cost_usd",
                "type": "gauge",
                "value": self.token_cost_usd,
                "attributes": attrs,
                "timestamp": timestamp,
            },
            {
                "name": "euxis.agent.budget_utilization_pct",
                "type": "gauge",
                "value": self.budget_utilization_pct,
                "attributes": attrs,
                "timestamp": timestamp,
            },
            {
                "name": "euxis.agent.latency_ms",
                "type": "gauge",
                "value": self.latency_ms,
                "attributes": attrs,
                "timestamp": timestamp,
            },
            {
                "name": "euxis.agent.request_count",
                "type": "counter",
                "value": self.request_count,
                "attributes": attrs,
                "timestamp": timestamp,
            },
            {
                "name": "euxis.agent.error_count",
                "type": "counter",
                "value": self.error_count,
                "attributes": attrs,
                "timestamp": timestamp,
            },
        ]


class TelemetryExporter:
    """Export agent metrics in OTLP-compatible format.

    Supports:
    - File export (JSON lines) for log ingestion
    - OTLP HTTP endpoint (if configured)
    - Console output for debugging
    """

    def __init__(
        self,
        export_path: Path | None = None,
        otlp_endpoint: str | None = None,
    ) -> None:
        self.export_path = export_path or (EUXIS_HOME / "data" / "telemetry.jsonl")
        self.otlp_endpoint = otlp_endpoint or os.environ.get("OTEL_EXPORTER_OTLP_ENDPOINT")
        self._metrics_cache: dict[str, AgentMetrics] = {}

    def update_agent_metrics(self, metrics: AgentMetrics) -> None:
        """Update metrics for an agent."""
        metrics.last_updated = datetime.now().isoformat()
        metrics.budget_utilization_pct = (
            (metrics.token_cost_usd / metrics.token_budget_usd * 100)
            if metrics.token_budget_usd > 0
            else 0
        )
        self._metrics_cache[metrics.agent_id] = metrics

    def export_to_file(self) -> None:
        """Export all metrics to JSONL file."""
        self.export_path.parent.mkdir(parents=True, exist_ok=True)

        with open(self.export_path, "a") as f:
            for metrics in self._metrics_cache.values():
                for metric_point in metrics.to_otlp_metrics():
                    f.write(json.dumps(metric_point) + "\n")

    def get_fleet_summary(self) -> dict[str, Any]:
        """Get aggregate fleet metrics."""
        total_cost = sum(m.token_cost_usd for m in self._metrics_cache.values())
        total_requests = sum(m.request_count for m in self._metrics_cache.values())
        total_errors = sum(m.error_count for m in self._metrics_cache.values())
        avg_latency = (
            sum(m.latency_ms for m in self._metrics_cache.values())
            / len(self._metrics_cache)
            if self._metrics_cache
            else 0
        )

        budget_warnings = sum(
            1 for m in self._metrics_cache.values() if m.budget_utilization_pct >= 75
        )

        return {
            "total_agents": len(self._metrics_cache),
            "total_cost_usd": total_cost,
            "total_requests": total_requests,
            "total_errors": total_errors,
            "avg_latency_ms": avg_latency,
            "budget_warnings": budget_warnings,
            "timestamp": datetime.now().isoformat(),
        }


# Singleton exporter
_exporter: TelemetryExporter | None = None


def get_exporter() -> TelemetryExporter:
    """Get the global telemetry exporter."""
    global _exporter
    if _exporter is None:
        _exporter = TelemetryExporter()
    return _exporter


# =============================================================================
# HAPTIC FEEDBACK — macOS Terminal Notifications (2026 Pattern)
# =============================================================================


def _detect_macos_terminal() -> str | None:
    """Detect if running in a macOS terminal that supports notifications."""
    if sys.platform != "darwin":
        return None

    term_program = os.environ.get("TERM_PROGRAM", "").lower()
    if term_program in ("iterm.app", "ghostty", "wezterm", "kitty"):
        return term_program
    return None


def _detect_linux_terminal() -> str | None:
    """Detect if running in a Linux terminal that supports notifications."""
    if sys.platform != "linux":
        return None

    # Check for common notification-capable terminals
    term_program = os.environ.get("TERM_PROGRAM", "").lower()
    if term_program in ("ghostty", "wezterm", "kitty"):
        return term_program

    # Check if notify-send is available
    try:
        subprocess.run(
            ["which", "notify-send"],
            capture_output=True,
            check=True,
        )
        return "linux-notify"
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    return None


def send_haptic_alert(
    title: str,
    message: str,
    severity: str = "warning",  # warning, error, info
    sound: bool = True,
) -> bool:
    """Send a haptic/notification alert on supported terminals.

    Returns True if notification was sent, False otherwise.
    """
    # Try macOS first
    macos_term = _detect_macos_terminal()
    if macos_term:
        return _send_macos_notification(title, message, severity, sound)

    # Try Linux
    linux_term = _detect_linux_terminal()
    if linux_term == "linux-notify":
        return _send_linux_notification(title, message, severity)

    # Fallback: terminal bell
    if sound:
        print("\a", end="", flush=True)  # ASCII bell
        return True

    return False


def _send_macos_notification(
    title: str,
    message: str,
    severity: str,
    sound: bool,
) -> bool:
    """Send notification via macOS osascript."""
    try:
        sound_name = {
            "error": "Basso",
            "warning": "Purr",
            "info": "Pop",
        }.get(severity, "Pop")

        script = f'''
        display notification "{message}" with title "{title}"''' + (
            f' sound name "{sound_name}"' if sound else ""
        )

        subprocess.run(
            ["osascript", "-e", script],
            capture_output=True,
            timeout=2,
        )
        return True
    except (subprocess.SubprocessError, FileNotFoundError):
        return False


def _send_linux_notification(
    title: str,
    message: str,
    severity: str,
) -> bool:
    """Send notification via notify-send on Linux."""
    try:
        urgency = {
            "error": "critical",
            "warning": "normal",
            "info": "low",
        }.get(severity, "normal")

        subprocess.run(
            [
                "notify-send",
                "--urgency", urgency,
                "--app-name", "Euxis",
                title,
                message,
            ],
            capture_output=True,
            timeout=2,
        )
        return True
    except (subprocess.SubprocessError, FileNotFoundError):
        return False


def alert_budget_exceeded(agent_id: str, cost: float, budget: float) -> None:
    """Send haptic alert when agent exceeds budget."""
    pct = int(cost / budget * 100) if budget > 0 else 0
    send_haptic_alert(
        title=f"Budget Alert: {agent_id}",
        message=f"Agent at {pct}% of ${budget:.2f} budget (${cost:.2f} spent)",
        severity="warning",
        sound=True,
    )


def alert_agent_error(agent_id: str, error_summary: str) -> None:
    """Send haptic alert when agent encounters error."""
    send_haptic_alert(
        title=f"Agent Error: {agent_id}",
        message=error_summary[:100],
        severity="error",
        sound=True,
    )


def alert_cascade_failure(root_agent: str, failed_agent: str) -> None:
    """Send haptic alert for cascading failure."""
    send_haptic_alert(
        title="Cascade Failure Detected",
        message=f"{failed_agent} failed due to {root_agent}",
        severity="error",
        sound=True,
    )
