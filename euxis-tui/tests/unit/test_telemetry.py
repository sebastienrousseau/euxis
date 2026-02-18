# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Unit tests for tui/core/telemetry.py — telemetry, audit, haptic alerts.

Tests cover: DelegationEvent, AuditTrail, load_audit_trail, AgentMetrics,
TelemetryExporter, get_exporter, _detect_macos_terminal,
_detect_linux_terminal, send_haptic_alert, _send_macos_notification,
_send_linux_notification, alert_budget_exceeded, alert_agent_error,
alert_cascade_failure.
"""

from __future__ import annotations

import json
import os
import unittest
from datetime import datetime
from io import StringIO
from pathlib import Path
from unittest.mock import MagicMock, Mock, mock_open, patch

from tui.core.telemetry import (
    AgentMetrics,
    AuditTrail,
    DelegationEvent,
    TelemetryExporter,
    _detect_linux_terminal,
    _detect_macos_terminal,
    _send_linux_notification,
    _send_macos_notification,
    alert_agent_error,
    alert_budget_exceeded,
    alert_cascade_failure,
    get_exporter,
    load_audit_trail,
    send_haptic_alert,
)


# =============================================================================
# DelegationEvent
# =============================================================================


class TestDelegationEvent(unittest.TestCase):
    """Tests for the DelegationEvent dataclass."""

    def test_construction_required_fields(self):
        event = DelegationEvent(
            timestamp="2026-01-01T00:00:00",
            from_agent="supervisor",
            to_agent="worker",
            task="review code",
        )
        assert event.timestamp == "2026-01-01T00:00:00"
        assert event.from_agent == "supervisor"
        assert event.to_agent == "worker"
        assert event.task == "review code"

    def test_default_reason_empty(self):
        event = DelegationEvent(
            timestamp="t", from_agent="a", to_agent="b", task="x"
        )
        assert event.reason == ""

    def test_reason_when_provided(self):
        event = DelegationEvent(
            timestamp="t", from_agent="a", to_agent="b",
            task="x", reason="escalation"
        )
        assert event.reason == "escalation"


# =============================================================================
# AuditTrail
# =============================================================================


class TestAuditTrail(unittest.TestCase):
    """Tests for AuditTrail add_delegation, get_root_delegator, get_chain_summary, to_dict."""

    def test_construction(self):
        trail = AuditTrail(agent_id="agent-1")
        assert trail.agent_id == "agent-1"
        assert trail.events == []
        assert trail.error_timestamp is None
        assert trail.error_message is None

    def test_add_delegation_appends_event(self):
        trail = AuditTrail(agent_id="agent-1")
        trail.add_delegation("supervisor", "worker", "build artifact")
        assert len(trail.events) == 1
        assert trail.events[0].from_agent == "supervisor"
        assert trail.events[0].to_agent == "worker"
        assert trail.events[0].task == "build artifact"

    def test_add_delegation_truncates_task_to_200_chars(self):
        trail = AuditTrail(agent_id="agent-1")
        long_task = "x" * 300
        trail.add_delegation("a", "b", long_task)
        assert len(trail.events[0].task) == 200

    def test_add_delegation_sets_timestamp(self):
        trail = AuditTrail(agent_id="agent-1")
        trail.add_delegation("a", "b", "task")
        # Timestamp should be ISO format
        assert "T" in trail.events[0].timestamp

    def test_add_delegation_with_reason(self):
        trail = AuditTrail(agent_id="agent-1")
        trail.add_delegation("a", "b", "task", reason="escalation")
        assert trail.events[0].reason == "escalation"

    def test_add_delegation_multiple(self):
        trail = AuditTrail(agent_id="agent-1")
        trail.add_delegation("a", "b", "t1")
        trail.add_delegation("b", "c", "t2")
        trail.add_delegation("c", "d", "t3")
        assert len(trail.events) == 3

    def test_get_root_delegator_no_events(self):
        trail = AuditTrail(agent_id="agent-1")
        assert trail.get_root_delegator() is None

    def test_get_root_delegator_single_event(self):
        trail = AuditTrail(agent_id="agent-1")
        trail.add_delegation("supervisor", "worker", "task")
        assert trail.get_root_delegator() == "supervisor"

    def test_get_root_delegator_multiple_events(self):
        trail = AuditTrail(agent_id="agent-1")
        trail.add_delegation("boss", "mid", "task1")
        trail.add_delegation("mid", "worker", "task2")
        # Root is always the first event's from_agent
        assert trail.get_root_delegator() == "boss"

    def test_get_chain_summary_no_events(self):
        trail = AuditTrail(agent_id="agent-1")
        assert trail.get_chain_summary() == "No delegation chain recorded"

    def test_get_chain_summary_single_event(self):
        trail = AuditTrail(agent_id="agent-1")
        trail.add_delegation("supervisor", "worker", "task")
        summary = trail.get_chain_summary()
        assert summary == "supervisor \u2192 worker"

    def test_get_chain_summary_multiple_events(self):
        trail = AuditTrail(agent_id="agent-1")
        trail.add_delegation("boss", "mid", "t1")
        trail.add_delegation("mid", "worker", "t2")
        summary = trail.get_chain_summary()
        assert summary == "boss \u2192 mid \u2192 worker"

    def test_to_dict_no_events(self):
        trail = AuditTrail(agent_id="agent-1")
        d = trail.to_dict()
        assert d["agent_id"] == "agent-1"
        assert d["root_delegator"] is None
        assert d["chain"] == "No delegation chain recorded"
        assert d["events"] == []
        assert d["error_timestamp"] is None
        assert d["error_message"] is None

    def test_to_dict_with_events(self):
        trail = AuditTrail(agent_id="agent-1")
        trail.add_delegation("a", "b", "build")
        d = trail.to_dict()
        assert len(d["events"]) == 1
        assert d["events"][0]["from"] == "a"
        assert d["events"][0]["to"] == "b"
        assert d["events"][0]["task"] == "build"
        assert d["root_delegator"] == "a"

    def test_to_dict_with_error(self):
        trail = AuditTrail(
            agent_id="agent-1",
            error_timestamp="2026-01-01T12:00:00",
            error_message="OOM",
        )
        d = trail.to_dict()
        assert d["error_timestamp"] == "2026-01-01T12:00:00"
        assert d["error_message"] == "OOM"


# =============================================================================
# load_audit_trail
# =============================================================================


class TestLoadAuditTrail(unittest.TestCase):
    """Tests for the load_audit_trail function."""

    def _make_state(self, agent_id, delegations=None, status="active", last_error=None, last_seen=None):
        agent_data = {}
        if delegations:
            agent_data["delegation_history"] = delegations
        if status:
            agent_data["status"] = status
        if last_error:
            agent_data["last_error"] = last_error
        if last_seen:
            agent_data["last_seen"] = last_seen
        return {"agents": {agent_id: agent_data}}

    @patch("builtins.open", side_effect=FileNotFoundError)
    def test_file_not_found_returns_empty_trail(self, mock_file):
        trail = load_audit_trail("agent-1")
        assert trail.agent_id == "agent-1"
        assert trail.events == []

    @patch("builtins.open")
    def test_loads_delegation_history(self, mock_file):
        state = self._make_state("agent-1", delegations=[
            {"timestamp": "t1", "from": "boss", "to": "agent-1", "task": "build", "reason": "urgent"},
        ])
        mock_file.return_value.__enter__ = lambda s: StringIO(json.dumps(state))
        mock_file.return_value.__exit__ = Mock(return_value=False)

        trail = load_audit_trail("agent-1")
        assert len(trail.events) == 1
        assert trail.events[0].from_agent == "boss"
        assert trail.events[0].to_agent == "agent-1"
        assert trail.events[0].task == "build"
        assert trail.events[0].reason == "urgent"

    @patch("builtins.open")
    def test_error_state_captured(self, mock_file):
        state = self._make_state(
            "agent-1", status="error",
            last_error="OOM", last_seen="2026-01-01T00:00:00",
        )
        mock_file.return_value.__enter__ = lambda s: StringIO(json.dumps(state))
        mock_file.return_value.__exit__ = Mock(return_value=False)

        trail = load_audit_trail("agent-1")
        assert trail.error_timestamp == "2026-01-01T00:00:00"
        assert trail.error_message == "OOM"

    @patch("builtins.open")
    def test_active_status_no_error(self, mock_file):
        state = self._make_state("agent-1", status="active")
        mock_file.return_value.__enter__ = lambda s: StringIO(json.dumps(state))
        mock_file.return_value.__exit__ = Mock(return_value=False)

        trail = load_audit_trail("agent-1")
        assert trail.error_timestamp is None
        assert trail.error_message is None

    @patch("builtins.open")
    def test_json_decode_error_returns_empty_trail(self, mock_file):
        mock_file.return_value.__enter__ = lambda s: StringIO("not valid json{{{")
        mock_file.return_value.__exit__ = Mock(return_value=False)

        trail = load_audit_trail("agent-1")
        assert trail.agent_id == "agent-1"
        assert trail.events == []

    @patch("builtins.open")
    def test_agent_not_in_state_returns_empty(self, mock_file):
        state = {"agents": {"other-agent": {}}}
        mock_file.return_value.__enter__ = lambda s: StringIO(json.dumps(state))
        mock_file.return_value.__exit__ = Mock(return_value=False)

        trail = load_audit_trail("agent-1")
        assert trail.events == []

    @patch("builtins.open")
    def test_missing_delegation_fields_use_defaults(self, mock_file):
        state = self._make_state("agent-1", delegations=[
            {"timestamp": "t1"},  # missing from, to, task, reason
        ])
        mock_file.return_value.__enter__ = lambda s: StringIO(json.dumps(state))
        mock_file.return_value.__exit__ = Mock(return_value=False)

        trail = load_audit_trail("agent-1")
        assert len(trail.events) == 1
        assert trail.events[0].from_agent == ""
        assert trail.events[0].to_agent == "agent-1"  # default
        assert trail.events[0].task == ""
        assert trail.events[0].reason == ""


# =============================================================================
# AgentMetrics
# =============================================================================


class TestAgentMetrics(unittest.TestCase):
    """Tests for AgentMetrics dataclass and OTLP export."""

    def _make_metrics(self, **kwargs):
        defaults = {
            "agent_id": "test-agent",
            "tier": "core",
            "supervision_tier": "specialist",
        }
        defaults.update(kwargs)
        return AgentMetrics(**defaults)

    def test_construction_defaults(self):
        m = self._make_metrics()
        assert m.agent_id == "test-agent"
        assert m.tier == "core"
        assert m.supervision_tier == "specialist"
        assert m.token_cost_usd == 0.0
        assert m.token_budget_usd == 2.0
        assert m.budget_utilization_pct == 0.0
        assert m.latency_ms == 0
        assert m.request_count == 0
        assert m.error_count == 0
        assert m.status == "idle"
        assert m.last_updated == ""

    def test_to_otlp_attributes(self):
        m = self._make_metrics(status="active")
        attrs = m.to_otlp_attributes()
        assert attrs["euxis.agent.id"] == "test-agent"
        assert attrs["euxis.agent.tier"] == "core"
        assert attrs["euxis.agent.supervision_tier"] == "specialist"
        assert attrs["euxis.agent.status"] == "active"

    @patch("time.time", return_value=1700000000.0)
    def test_to_otlp_metrics_returns_five_points(self, mock_time):
        m = self._make_metrics()
        metrics = m.to_otlp_metrics()
        assert len(metrics) == 5

    @patch("time.time", return_value=1700000000.0)
    def test_to_otlp_metrics_names(self, mock_time):
        m = self._make_metrics()
        metrics = m.to_otlp_metrics()
        names = [mp["name"] for mp in metrics]
        assert "euxis.agent.token_cost_usd" in names
        assert "euxis.agent.budget_utilization_pct" in names
        assert "euxis.agent.latency_ms" in names
        assert "euxis.agent.request_count" in names
        assert "euxis.agent.error_count" in names

    @patch("time.time", return_value=1700000000.0)
    def test_to_otlp_metrics_timestamp_nanoseconds(self, mock_time):
        m = self._make_metrics()
        metrics = m.to_otlp_metrics()
        expected_ns = int(1700000000.0 * 1_000_000_000)
        for mp in metrics:
            assert mp["timestamp"] == expected_ns

    @patch("time.time", return_value=1700000000.0)
    def test_to_otlp_metrics_values(self, mock_time):
        m = self._make_metrics(
            token_cost_usd=1.5,
            budget_utilization_pct=75.0,
            latency_ms=200,
            request_count=10,
            error_count=2,
        )
        metrics = m.to_otlp_metrics()
        values = {mp["name"]: mp["value"] for mp in metrics}
        assert values["euxis.agent.token_cost_usd"] == 1.5
        assert values["euxis.agent.budget_utilization_pct"] == 75.0
        assert values["euxis.agent.latency_ms"] == 200
        assert values["euxis.agent.request_count"] == 10
        assert values["euxis.agent.error_count"] == 2

    @patch("time.time", return_value=1700000000.0)
    def test_to_otlp_metrics_types(self, mock_time):
        m = self._make_metrics()
        metrics = m.to_otlp_metrics()
        types = {mp["name"]: mp["type"] for mp in metrics}
        assert types["euxis.agent.token_cost_usd"] == "gauge"
        assert types["euxis.agent.request_count"] == "counter"
        assert types["euxis.agent.error_count"] == "counter"

    @patch("time.time", return_value=1700000000.0)
    def test_to_otlp_metrics_include_attributes(self, mock_time):
        m = self._make_metrics()
        metrics = m.to_otlp_metrics()
        for mp in metrics:
            assert "attributes" in mp
            assert mp["attributes"]["euxis.agent.id"] == "test-agent"


# =============================================================================
# TelemetryExporter
# =============================================================================


class TestTelemetryExporter(unittest.TestCase):
    """Tests for TelemetryExporter class."""

    def test_construction_defaults(self):
        with patch.dict("os.environ", {}, clear=False):
            exporter = TelemetryExporter()
        assert exporter.export_path is not None
        assert exporter._metrics_cache == {}

    def test_construction_custom_path(self):
        p = Path("/tmp/test-telemetry.jsonl")
        exporter = TelemetryExporter(export_path=p)
        assert exporter.export_path == p

    def test_construction_custom_endpoint(self):
        exporter = TelemetryExporter(otlp_endpoint="http://localhost:4317")
        assert exporter.otlp_endpoint == "http://localhost:4317"

    def test_construction_endpoint_from_env(self):
        with patch.dict("os.environ", {"OTEL_EXPORTER_OTLP_ENDPOINT": "http://otel:4317"}):
            exporter = TelemetryExporter()
        assert exporter.otlp_endpoint == "http://otel:4317"

    def test_update_agent_metrics(self):
        exporter = TelemetryExporter()
        m = AgentMetrics(agent_id="a1", tier="core", supervision_tier="specialist")
        exporter.update_agent_metrics(m)
        assert "a1" in exporter._metrics_cache
        assert exporter._metrics_cache["a1"] is m

    def test_update_agent_metrics_sets_last_updated(self):
        exporter = TelemetryExporter()
        m = AgentMetrics(agent_id="a1", tier="core", supervision_tier="specialist")
        exporter.update_agent_metrics(m)
        assert m.last_updated != ""
        # Should be ISO format
        assert "T" in m.last_updated

    def test_update_agent_metrics_calculates_budget_utilization(self):
        exporter = TelemetryExporter()
        m = AgentMetrics(
            agent_id="a1", tier="core", supervision_tier="specialist",
            token_cost_usd=1.0, token_budget_usd=2.0,
        )
        exporter.update_agent_metrics(m)
        assert m.budget_utilization_pct == 50.0

    def test_update_agent_metrics_zero_budget(self):
        exporter = TelemetryExporter()
        m = AgentMetrics(
            agent_id="a1", tier="core", supervision_tier="specialist",
            token_cost_usd=1.0, token_budget_usd=0.0,
        )
        exporter.update_agent_metrics(m)
        assert m.budget_utilization_pct == 0

    def test_update_agent_metrics_replaces_existing(self):
        exporter = TelemetryExporter()
        m1 = AgentMetrics(agent_id="a1", tier="core", supervision_tier="specialist")
        m2 = AgentMetrics(agent_id="a1", tier="fleet", supervision_tier="coordinator")
        exporter.update_agent_metrics(m1)
        exporter.update_agent_metrics(m2)
        assert exporter._metrics_cache["a1"] is m2

    @patch("builtins.open", new_callable=mock_open)
    @patch("pathlib.Path.mkdir")
    @patch("time.time", return_value=1700000000.0)
    def test_export_to_file(self, mock_time, mock_mkdir, mock_file):
        exporter = TelemetryExporter(export_path=Path("/tmp/test.jsonl"))
        m = AgentMetrics(agent_id="a1", tier="core", supervision_tier="specialist")
        exporter.update_agent_metrics(m)
        exporter.export_to_file()

        mock_mkdir.assert_called_once_with(parents=True, exist_ok=True)
        mock_file.assert_called_once_with(Path("/tmp/test.jsonl"), "a")
        # Should write 5 metric lines (one per OTLP metric)
        handle = mock_file()
        assert handle.write.call_count == 5

    @patch("builtins.open", new_callable=mock_open)
    @patch("pathlib.Path.mkdir")
    @patch("time.time", return_value=1700000000.0)
    def test_export_to_file_empty_cache(self, mock_time, mock_mkdir, mock_file):
        exporter = TelemetryExporter(export_path=Path("/tmp/test.jsonl"))
        exporter.export_to_file()
        handle = mock_file()
        handle.write.assert_not_called()

    def test_get_fleet_summary_empty(self):
        exporter = TelemetryExporter()
        summary = exporter.get_fleet_summary()
        assert summary["total_agents"] == 0
        assert summary["total_cost_usd"] == 0
        assert summary["total_requests"] == 0
        assert summary["total_errors"] == 0
        assert summary["avg_latency_ms"] == 0
        assert summary["budget_warnings"] == 0
        assert "timestamp" in summary

    def test_get_fleet_summary_multiple_agents(self):
        exporter = TelemetryExporter()
        m1 = AgentMetrics(
            agent_id="a1", tier="core", supervision_tier="specialist",
            token_cost_usd=1.0, latency_ms=100, request_count=5, error_count=1,
        )
        m2 = AgentMetrics(
            agent_id="a2", tier="fleet", supervision_tier="coordinator",
            token_cost_usd=2.0, latency_ms=200, request_count=10, error_count=3,
        )
        exporter.update_agent_metrics(m1)
        exporter.update_agent_metrics(m2)

        summary = exporter.get_fleet_summary()
        assert summary["total_agents"] == 2
        assert summary["total_cost_usd"] == 3.0
        assert summary["total_requests"] == 15
        assert summary["total_errors"] == 4
        assert summary["avg_latency_ms"] == 150.0

    def test_get_fleet_summary_budget_warnings(self):
        exporter = TelemetryExporter()
        # Agent at 80% — should trigger warning (>= 75%)
        m1 = AgentMetrics(
            agent_id="a1", tier="core", supervision_tier="specialist",
            token_cost_usd=1.6, token_budget_usd=2.0,
        )
        # Agent at 50% — no warning
        m2 = AgentMetrics(
            agent_id="a2", tier="fleet", supervision_tier="coordinator",
            token_cost_usd=1.0, token_budget_usd=2.0,
        )
        exporter.update_agent_metrics(m1)
        exporter.update_agent_metrics(m2)

        summary = exporter.get_fleet_summary()
        assert summary["budget_warnings"] == 1

    def test_get_fleet_summary_all_above_threshold(self):
        exporter = TelemetryExporter()
        for i in range(3):
            m = AgentMetrics(
                agent_id=f"a{i}", tier="core", supervision_tier="specialist",
                token_cost_usd=1.8, token_budget_usd=2.0,
            )
            exporter.update_agent_metrics(m)

        summary = exporter.get_fleet_summary()
        assert summary["budget_warnings"] == 3


# =============================================================================
# get_exporter (singleton)
# =============================================================================


class TestGetExporter(unittest.TestCase):
    """Tests for the get_exporter singleton function."""

    def test_returns_telemetry_exporter(self):
        # Reset singleton
        import tui.core.telemetry as tel_mod
        tel_mod._exporter = None
        exporter = get_exporter()
        assert isinstance(exporter, TelemetryExporter)

    def test_singleton_returns_same_instance(self):
        import tui.core.telemetry as tel_mod
        tel_mod._exporter = None
        e1 = get_exporter()
        e2 = get_exporter()
        assert e1 is e2

    def test_singleton_creates_on_first_call(self):
        import tui.core.telemetry as tel_mod
        tel_mod._exporter = None
        assert tel_mod._exporter is None
        get_exporter()
        assert tel_mod._exporter is not None

    def tearDown(self):
        # Reset singleton after each test
        import tui.core.telemetry as tel_mod
        tel_mod._exporter = None


# =============================================================================
# _detect_macos_terminal
# =============================================================================


class TestDetectMacosTerminal(unittest.TestCase):
    """Tests for _detect_macos_terminal."""

    @patch("sys.platform", "linux")
    def test_returns_none_on_linux(self):
        assert _detect_macos_terminal() is None

    @patch("sys.platform", "win32")
    def test_returns_none_on_windows(self):
        assert _detect_macos_terminal() is None

    @patch("sys.platform", "darwin")
    def test_returns_none_for_unknown_terminal(self):
        with patch.dict("os.environ", {"TERM_PROGRAM": "Terminal.app"}):
            assert _detect_macos_terminal() is None

    @patch("sys.platform", "darwin")
    def test_detects_iterm(self):
        with patch.dict("os.environ", {"TERM_PROGRAM": "iTerm.app"}):
            assert _detect_macos_terminal() == "iterm.app"

    @patch("sys.platform", "darwin")
    def test_detects_ghostty(self):
        with patch.dict("os.environ", {"TERM_PROGRAM": "ghostty"}):
            assert _detect_macos_terminal() == "ghostty"

    @patch("sys.platform", "darwin")
    def test_detects_wezterm(self):
        with patch.dict("os.environ", {"TERM_PROGRAM": "WezTerm"}):
            assert _detect_macos_terminal() == "wezterm"

    @patch("sys.platform", "darwin")
    def test_detects_kitty(self):
        with patch.dict("os.environ", {"TERM_PROGRAM": "kitty"}):
            assert _detect_macos_terminal() == "kitty"

    @patch("sys.platform", "darwin")
    def test_empty_term_program_returns_none(self):
        with patch.dict("os.environ", {"TERM_PROGRAM": ""}):
            assert _detect_macos_terminal() is None

    @patch("sys.platform", "darwin")
    def test_no_term_program_returns_none(self):
        with patch.dict("os.environ", {}, clear=True):
            assert _detect_macos_terminal() is None


# =============================================================================
# _detect_linux_terminal
# =============================================================================


class TestDetectLinuxTerminal(unittest.TestCase):
    """Tests for _detect_linux_terminal."""

    @patch("sys.platform", "darwin")
    def test_returns_none_on_macos(self):
        assert _detect_linux_terminal() is None

    @patch("sys.platform", "win32")
    def test_returns_none_on_windows(self):
        assert _detect_linux_terminal() is None

    @patch("sys.platform", "linux")
    def test_detects_ghostty(self):
        with patch.dict("os.environ", {"TERM_PROGRAM": "ghostty"}):
            assert _detect_linux_terminal() == "ghostty"

    @patch("sys.platform", "linux")
    def test_detects_wezterm(self):
        with patch.dict("os.environ", {"TERM_PROGRAM": "WezTerm"}):
            assert _detect_linux_terminal() == "wezterm"

    @patch("sys.platform", "linux")
    def test_detects_kitty(self):
        with patch.dict("os.environ", {"TERM_PROGRAM": "kitty"}):
            assert _detect_linux_terminal() == "kitty"

    @patch("sys.platform", "linux")
    @patch("subprocess.run")
    def test_detects_notify_send(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0)
        with patch.dict("os.environ", {"TERM_PROGRAM": ""}):
            result = _detect_linux_terminal()
        assert result == "linux-notify"

    @patch("sys.platform", "linux")
    @patch("subprocess.run", side_effect=FileNotFoundError)
    def test_no_notify_send_returns_none(self, mock_run):
        with patch.dict("os.environ", {"TERM_PROGRAM": ""}):
            result = _detect_linux_terminal()
        assert result is None

    @patch("sys.platform", "linux")
    @patch("subprocess.run")
    def test_notify_send_check_failure_returns_none(self, mock_run):
        import subprocess
        mock_run.side_effect = subprocess.CalledProcessError(1, "which")
        with patch.dict("os.environ", {"TERM_PROGRAM": ""}):
            result = _detect_linux_terminal()
        assert result is None

    @patch("sys.platform", "linux")
    def test_unknown_terminal_without_notify_send(self):
        with patch.dict("os.environ", {"TERM_PROGRAM": "alacritty"}), \
             patch("subprocess.run", side_effect=FileNotFoundError):
            result = _detect_linux_terminal()
        assert result is None


# =============================================================================
# send_haptic_alert
# =============================================================================


class TestSendHapticAlert(unittest.TestCase):
    """Tests for send_haptic_alert dispatching."""

    @patch("tui.core.telemetry._send_macos_notification", return_value=True)
    @patch("tui.core.telemetry._detect_macos_terminal", return_value="iterm.app")
    def test_macos_terminal_sends_macos_notification(self, mock_detect, mock_send):
        result = send_haptic_alert("Title", "Message")
        assert result is True
        mock_send.assert_called_once_with("Title", "Message", "warning", True)

    @patch("tui.core.telemetry._send_linux_notification", return_value=True)
    @patch("tui.core.telemetry._detect_linux_terminal", return_value="linux-notify")
    @patch("tui.core.telemetry._detect_macos_terminal", return_value=None)
    def test_linux_notify_sends_linux_notification(self, mock_mac, mock_detect, mock_send):
        result = send_haptic_alert("Title", "Message", severity="error")
        assert result is True
        mock_send.assert_called_once_with("Title", "Message", "error")

    @patch("tui.core.telemetry._detect_linux_terminal", return_value=None)
    @patch("tui.core.telemetry._detect_macos_terminal", return_value=None)
    def test_fallback_bell_with_sound(self, mock_mac, mock_linux):
        captured = StringIO()
        with patch("sys.stdout", captured):
            result = send_haptic_alert("Title", "Message", sound=True)
        assert result is True
        assert "\a" in captured.getvalue()

    @patch("tui.core.telemetry._detect_linux_terminal", return_value=None)
    @patch("tui.core.telemetry._detect_macos_terminal", return_value=None)
    def test_no_sound_returns_false(self, mock_mac, mock_linux):
        result = send_haptic_alert("Title", "Message", sound=False)
        assert result is False

    @patch("tui.core.telemetry._send_macos_notification", return_value=True)
    @patch("tui.core.telemetry._detect_macos_terminal", return_value="ghostty")
    def test_custom_severity(self, mock_detect, mock_send):
        send_haptic_alert("T", "M", severity="error", sound=False)
        mock_send.assert_called_once_with("T", "M", "error", False)

    @patch("tui.core.telemetry._detect_linux_terminal", return_value="kitty")
    @patch("tui.core.telemetry._detect_macos_terminal", return_value=None)
    def test_linux_non_notify_terminal_falls_through(self, mock_mac, mock_linux):
        """A linux terminal that is not 'linux-notify' should fall to bell."""
        captured = StringIO()
        with patch("sys.stdout", captured):
            result = send_haptic_alert("Title", "Message", sound=True)
        assert result is True


# =============================================================================
# _send_macos_notification
# =============================================================================


class TestSendMacosNotification(unittest.TestCase):
    """Tests for _send_macos_notification."""

    @patch("subprocess.run")
    def test_success_returns_true(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0)
        result = _send_macos_notification("Title", "Message", "warning", True)
        assert result is True

    @patch("subprocess.run")
    def test_calls_osascript(self, mock_run):
        _send_macos_notification("Title", "Message", "info", False)
        args = mock_run.call_args[0][0]
        assert args[0] == "osascript"
        assert args[1] == "-e"

    @patch("subprocess.run")
    def test_sound_included_when_true(self, mock_run):
        _send_macos_notification("Title", "Message", "error", True)
        script = mock_run.call_args[0][0][2]
        assert "sound name" in script
        assert "Basso" in script

    @patch("subprocess.run")
    def test_sound_excluded_when_false(self, mock_run):
        _send_macos_notification("Title", "Message", "error", False)
        script = mock_run.call_args[0][0][2]
        assert "sound name" not in script

    @patch("subprocess.run")
    def test_warning_sound_is_purr(self, mock_run):
        _send_macos_notification("Title", "Message", "warning", True)
        script = mock_run.call_args[0][0][2]
        assert "Purr" in script

    @patch("subprocess.run")
    def test_info_sound_is_pop(self, mock_run):
        _send_macos_notification("Title", "Message", "info", True)
        script = mock_run.call_args[0][0][2]
        assert "Pop" in script

    @patch("subprocess.run")
    def test_unknown_severity_uses_pop(self, mock_run):
        _send_macos_notification("Title", "Message", "critical", True)
        script = mock_run.call_args[0][0][2]
        assert "Pop" in script

    @patch("subprocess.run", side_effect=FileNotFoundError)
    def test_file_not_found_returns_false(self, mock_run):
        result = _send_macos_notification("Title", "Message", "warning", True)
        assert result is False

    @patch("subprocess.run")
    def test_subprocess_error_returns_false(self, mock_run):
        import subprocess
        mock_run.side_effect = subprocess.SubprocessError("timeout")
        result = _send_macos_notification("Title", "Message", "warning", True)
        assert result is False

    @patch("subprocess.run")
    def test_timeout_set_to_2(self, mock_run):
        _send_macos_notification("Title", "Message", "info", False)
        kwargs = mock_run.call_args[1]
        assert kwargs.get("timeout") == 2


# =============================================================================
# _send_linux_notification
# =============================================================================


class TestSendLinuxNotification(unittest.TestCase):
    """Tests for _send_linux_notification."""

    @patch("subprocess.run")
    def test_success_returns_true(self, mock_run):
        mock_run.return_value = MagicMock(returncode=0)
        result = _send_linux_notification("Title", "Message", "warning")
        assert result is True

    @patch("subprocess.run")
    def test_calls_notify_send(self, mock_run):
        _send_linux_notification("Title", "Message", "info")
        args = mock_run.call_args[0][0]
        assert args[0] == "notify-send"

    @patch("subprocess.run")
    def test_error_urgency_critical(self, mock_run):
        _send_linux_notification("Title", "Message", "error")
        args = mock_run.call_args[0][0]
        urgency_idx = args.index("--urgency") + 1
        assert args[urgency_idx] == "critical"

    @patch("subprocess.run")
    def test_warning_urgency_normal(self, mock_run):
        _send_linux_notification("Title", "Message", "warning")
        args = mock_run.call_args[0][0]
        urgency_idx = args.index("--urgency") + 1
        assert args[urgency_idx] == "normal"

    @patch("subprocess.run")
    def test_info_urgency_low(self, mock_run):
        _send_linux_notification("Title", "Message", "info")
        args = mock_run.call_args[0][0]
        urgency_idx = args.index("--urgency") + 1
        assert args[urgency_idx] == "low"

    @patch("subprocess.run")
    def test_unknown_severity_defaults_to_normal(self, mock_run):
        _send_linux_notification("Title", "Message", "unknown_sev")
        args = mock_run.call_args[0][0]
        urgency_idx = args.index("--urgency") + 1
        assert args[urgency_idx] == "normal"

    @patch("subprocess.run")
    def test_app_name_euxis(self, mock_run):
        _send_linux_notification("Title", "Message", "info")
        args = mock_run.call_args[0][0]
        app_name_idx = args.index("--app-name") + 1
        assert args[app_name_idx] == "Euxis"

    @patch("subprocess.run")
    def test_title_and_message_in_args(self, mock_run):
        _send_linux_notification("My Title", "My Message", "info")
        args = mock_run.call_args[0][0]
        assert "My Title" in args
        assert "My Message" in args

    @patch("subprocess.run", side_effect=FileNotFoundError)
    def test_file_not_found_returns_false(self, mock_run):
        result = _send_linux_notification("Title", "Message", "warning")
        assert result is False

    @patch("subprocess.run")
    def test_subprocess_error_returns_false(self, mock_run):
        import subprocess
        mock_run.side_effect = subprocess.SubprocessError("timeout")
        result = _send_linux_notification("Title", "Message", "warning")
        assert result is False

    @patch("subprocess.run")
    def test_timeout_set_to_2(self, mock_run):
        _send_linux_notification("Title", "Message", "info")
        kwargs = mock_run.call_args[1]
        assert kwargs.get("timeout") == 2


# =============================================================================
# alert_budget_exceeded, alert_agent_error, alert_cascade_failure
# =============================================================================


class TestAlertBudgetExceeded(unittest.TestCase):
    """Tests for alert_budget_exceeded."""

    @patch("tui.core.telemetry.send_haptic_alert")
    def test_calls_send_haptic_alert(self, mock_send):
        alert_budget_exceeded("agent-1", 1.5, 2.0)
        mock_send.assert_called_once()

    @patch("tui.core.telemetry.send_haptic_alert")
    def test_title_contains_agent_id(self, mock_send):
        alert_budget_exceeded("agent-1", 1.5, 2.0)
        title = mock_send.call_args[1].get("title") or mock_send.call_args[0][0]
        assert "agent-1" in title

    @patch("tui.core.telemetry.send_haptic_alert")
    def test_message_contains_percentage(self, mock_send):
        alert_budget_exceeded("agent-1", 1.5, 2.0)
        msg = mock_send.call_args[1].get("message") or mock_send.call_args[0][1]
        assert "75%" in msg

    @patch("tui.core.telemetry.send_haptic_alert")
    def test_severity_is_warning(self, mock_send):
        alert_budget_exceeded("agent-1", 1.5, 2.0)
        kwargs = mock_send.call_args[1]
        assert kwargs.get("severity") == "warning"

    @patch("tui.core.telemetry.send_haptic_alert")
    def test_sound_is_true(self, mock_send):
        alert_budget_exceeded("agent-1", 1.5, 2.0)
        kwargs = mock_send.call_args[1]
        assert kwargs.get("sound") is True

    @patch("tui.core.telemetry.send_haptic_alert")
    def test_zero_budget_shows_0_percent(self, mock_send):
        alert_budget_exceeded("agent-1", 1.5, 0.0)
        kwargs = mock_send.call_args[1]
        assert "0%" in kwargs["message"]


class TestAlertAgentError(unittest.TestCase):
    """Tests for alert_agent_error."""

    @patch("tui.core.telemetry.send_haptic_alert")
    def test_calls_send_haptic_alert(self, mock_send):
        alert_agent_error("agent-1", "Something went wrong")
        mock_send.assert_called_once()

    @patch("tui.core.telemetry.send_haptic_alert")
    def test_title_contains_agent_id(self, mock_send):
        alert_agent_error("agent-1", "Error details")
        kwargs = mock_send.call_args[1]
        assert "agent-1" in kwargs["title"]

    @patch("tui.core.telemetry.send_haptic_alert")
    def test_message_truncated_to_100(self, mock_send):
        long_error = "x" * 200
        alert_agent_error("agent-1", long_error)
        kwargs = mock_send.call_args[1]
        assert len(kwargs["message"]) <= 100

    @patch("tui.core.telemetry.send_haptic_alert")
    def test_severity_is_error(self, mock_send):
        alert_agent_error("agent-1", "err")
        kwargs = mock_send.call_args[1]
        assert kwargs.get("severity") == "error"

    @patch("tui.core.telemetry.send_haptic_alert")
    def test_sound_is_true(self, mock_send):
        alert_agent_error("agent-1", "err")
        kwargs = mock_send.call_args[1]
        assert kwargs.get("sound") is True


class TestAlertCascadeFailure(unittest.TestCase):
    """Tests for alert_cascade_failure."""

    @patch("tui.core.telemetry.send_haptic_alert")
    def test_calls_send_haptic_alert(self, mock_send):
        alert_cascade_failure("root-agent", "child-agent")
        mock_send.assert_called_once()

    @patch("tui.core.telemetry.send_haptic_alert")
    def test_title_is_cascade_failure(self, mock_send):
        alert_cascade_failure("root", "child")
        kwargs = mock_send.call_args[1]
        assert "Cascade Failure" in kwargs["title"]

    @patch("tui.core.telemetry.send_haptic_alert")
    def test_message_contains_agents(self, mock_send):
        alert_cascade_failure("root-agent", "child-agent")
        kwargs = mock_send.call_args[1]
        assert "child-agent" in kwargs["message"]
        assert "root-agent" in kwargs["message"]

    @patch("tui.core.telemetry.send_haptic_alert")
    def test_severity_is_error(self, mock_send):
        alert_cascade_failure("root", "child")
        kwargs = mock_send.call_args[1]
        assert kwargs.get("severity") == "error"

    @patch("tui.core.telemetry.send_haptic_alert")
    def test_sound_is_true(self, mock_send):
        alert_cascade_failure("root", "child")
        kwargs = mock_send.call_args[1]
        assert kwargs.get("sound") is True


if __name__ == "__main__":
    unittest.main()
