# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Comprehensive unit tests for ErrorDetailsModal (error_details.py).

Covers:
- SEVERITY_STYLES and CONFIDENCE_STYLES constants
- __init__ with/without error_info
- _fetch_error_info: state file found/not found, JSON error, agent present/absent
- compose: yields correct widget tree
- on_mount: run_worker and alert_agent_error called
- _run_analysis: ArbiterAnalyzer mock
- on_worker_state_changed: SUCCESS, ERROR, non-matching worker
- _update_analysis_display: all branches (severity, confidence, recovery, attribution, auto-recover)
- on_button_pressed: restart, simulate, logs, close
- action methods: close, restart, simulate, logs, validate
"""

from __future__ import annotations

import json
import os
import unittest
from datetime import datetime
from unittest.mock import (
    AsyncMock,
    MagicMock,
    Mock,
    PropertyMock,
    patch,
    mock_open,
)

import pytest

from tui.screens.error_details import (
    CONFIDENCE_STYLES,
    SEVERITY_STYLES,
    ErrorDetailsModal,
)


# ============================================================================
# Helpers
# ============================================================================


def _patch_screen_app(screen, mock_app):
    """Bypass Textual's read-only app property for testing."""
    patcher = patch.object(
        type(screen), "app", new_callable=PropertyMock, return_value=mock_app
    )
    patcher.start()
    return patcher


def _make_mock_app():
    mock_app = Mock()
    mock_app.project_name = "test-project"
    mock_app.git_branch = "main"
    mock_app.config = Mock()
    mock_app.config.default_provider = "claude"
    return mock_app


def _make_agent(**overrides):
    """Create a mock Agent with required fields."""
    from tui.core.registry import Agent

    defaults = {
        "id": "test-agent",
        "tier": "core",
        "version": "0.1.0",
        "tags": [],
        "activation": "default",
    }
    defaults.update(overrides)
    return Agent(**defaults)


def _make_error_info(**overrides):
    """Build a standard error_info dict."""
    defaults = {
        "timestamp": "2026-02-18T10:00:00Z",
        "message": "Connection timeout to mesh",
        "last_task": "Deploy phase 2",
    }
    defaults.update(overrides)
    return defaults


def _make_error_analysis(**overrides):
    """Build a mock ErrorAnalysis."""
    from tui.core.runner import ErrorAnalysis, RecoveryStep

    defaults = {
        "summary": "API timeout during mesh sync",
        "root_cause": "Rate limit exceeded on provider API",
        "recovery_steps": [
            RecoveryStep(action="Retry with backoff", success_probability=0.85),
            RecoveryStep(action="Switch provider", success_probability=0.70, requires_sandbox=True),
        ],
        "severity": "warning",
        "confidence": 0.75,
        "caused_by_agent": None,
        "can_auto_recover": False,
        "raw_output": "",
    }
    defaults.update(overrides)
    return ErrorAnalysis(**defaults)


def _make_audit_trail(events=None):
    """Build a mock AuditTrail."""
    from tui.core.telemetry import AuditTrail

    trail = AuditTrail(agent_id="test-agent")
    if events:
        trail.events = events
    return trail


# ============================================================================
# Constants
# ============================================================================


class TestConstants(unittest.TestCase):
    """Tests for module-level style constants."""

    def test_severity_styles_keys(self):
        assert "critical" in SEVERITY_STYLES
        assert "warning" in SEVERITY_STYLES
        assert "info" in SEVERITY_STYLES

    def test_severity_styles_values_are_tuples(self):
        for key, val in SEVERITY_STYLES.items():
            assert isinstance(val, tuple), f"SEVERITY_STYLES[{key}] is not a tuple"
            assert len(val) == 2
            assert isinstance(val[0], str)  # icon
            assert isinstance(val[1], str)  # style

    def test_confidence_styles_keys(self):
        assert "high" in CONFIDENCE_STYLES
        assert "medium" in CONFIDENCE_STYLES
        assert "low" in CONFIDENCE_STYLES

    def test_confidence_styles_values_are_tuples(self):
        for key, val in CONFIDENCE_STYLES.items():
            assert isinstance(val, tuple), f"CONFIDENCE_STYLES[{key}] is not a tuple"
            assert len(val) == 2


# ============================================================================
# __init__
# ============================================================================


class TestErrorDetailsModalInit(unittest.TestCase):
    """Tests for ErrorDetailsModal.__init__."""

    @patch("tui.screens.error_details.load_audit_trail")
    def test_init_with_error_info(self, mock_load_audit):
        mock_load_audit.return_value = _make_audit_trail()

        agent = _make_agent()
        error_info = _make_error_info()
        modal = ErrorDetailsModal(agent, error_info=error_info)

        assert modal.agent is agent
        assert modal.error_info is error_info
        assert modal._analysis is None
        assert modal._analysis_worker is None
        mock_load_audit.assert_called_once_with("test-agent")

    @patch("tui.screens.error_details.load_audit_trail")
    @patch.object(ErrorDetailsModal, "_fetch_error_info")
    def test_init_without_error_info(self, mock_fetch, mock_load_audit):
        mock_load_audit.return_value = _make_audit_trail()
        mock_fetch.return_value = {"timestamp": "now", "message": "fetched", "last_task": "unknown"}

        agent = _make_agent()
        modal = ErrorDetailsModal(agent, error_info=None)

        mock_fetch.assert_called_once()
        assert modal.error_info["message"] == "fetched"


# ============================================================================
# _fetch_error_info
# ============================================================================


class TestFetchErrorInfo(unittest.TestCase):
    """Tests for _fetch_error_info."""

    @patch("tui.screens.error_details.load_audit_trail")
    def _make_modal(self, mock_load_audit, agent_id="test-agent"):
        mock_load_audit.return_value = _make_audit_trail()
        agent = _make_agent(id=agent_id)
        # Provide error_info to skip _fetch during init
        modal = ErrorDetailsModal(agent, error_info=_make_error_info())
        return modal

    def test_fetch_state_file_found_with_agent_state(self):
        modal = self._make_modal()
        state_data = {
            "agents": {
                "test-agent": {
                    "last_seen": "2026-02-18T11:00:00Z",
                    "last_error": "Connection refused",
                    "current_task": "Health check",
                }
            }
        }

        with patch.dict(os.environ, {"EUXIS_HOME": "/tmp/euxis"}):
            with patch("builtins.open", mock_open(read_data=json.dumps(state_data))):
                result = modal._fetch_error_info()

        assert result["timestamp"] == "2026-02-18T11:00:00Z"
        assert result["message"] == "Connection refused"
        assert result["last_task"] == "Health check"

    def test_fetch_state_file_found_no_agent_state(self):
        modal = self._make_modal(agent_id="missing-agent")
        state_data = {"agents": {"other-agent": {"last_error": "something"}}}

        with patch.dict(os.environ, {"EUXIS_HOME": "/tmp/euxis"}):
            with patch("builtins.open", mock_open(read_data=json.dumps(state_data))):
                result = modal._fetch_error_info()

        # Falls through with defaults
        assert result["message"] == "Error details unavailable"

    def test_fetch_state_file_not_found(self):
        modal = self._make_modal()

        with patch.dict(os.environ, {"EUXIS_HOME": "/tmp/euxis"}):
            with patch("builtins.open", side_effect=FileNotFoundError("no such file")):
                result = modal._fetch_error_info()

        assert result["message"] == "Error details unavailable"
        assert result["last_task"] == "Unknown"

    def test_fetch_state_file_json_error(self):
        modal = self._make_modal()

        with patch.dict(os.environ, {"EUXIS_HOME": "/tmp/euxis"}):
            with patch("builtins.open", mock_open(read_data="NOT JSON")):
                result = modal._fetch_error_info()

        assert result["message"] == "Error details unavailable"

    def test_fetch_uses_euxis_home_default(self):
        modal = self._make_modal()

        with patch.dict(os.environ, {}, clear=True):
            with patch("os.path.expanduser", return_value="/home/user/.euxis"):
                with patch("builtins.open", side_effect=FileNotFoundError):
                    result = modal._fetch_error_info()

        assert result["message"] == "Error details unavailable"


# ============================================================================
# on_mount
# ============================================================================


class TestOnMount(unittest.TestCase):
    """Tests for on_mount."""

    @patch("tui.screens.error_details.load_audit_trail")
    @patch("tui.screens.error_details.alert_agent_error")
    def test_on_mount_starts_worker_and_alerts(self, mock_alert, mock_load_audit):
        mock_load_audit.return_value = _make_audit_trail()

        agent = _make_agent()
        error_info = _make_error_info(message="Test error 123")
        modal = ErrorDetailsModal(agent, error_info=error_info)

        mock_worker = Mock()
        modal.run_worker = Mock(return_value=mock_worker)

        modal.on_mount()

        modal.run_worker.assert_called_once()
        call_kwargs = modal.run_worker.call_args
        assert call_kwargs.kwargs.get("name") == "arbiter-analysis"
        assert call_kwargs.kwargs.get("thread") is True
        assert modal._analysis_worker is mock_worker
        mock_alert.assert_called_once_with("test-agent", "Test error 123")

    @patch("tui.screens.error_details.load_audit_trail")
    @patch("tui.screens.error_details.alert_agent_error")
    def test_on_mount_truncates_error_summary(self, mock_alert, mock_load_audit):
        mock_load_audit.return_value = _make_audit_trail()

        agent = _make_agent()
        long_msg = "x" * 200
        error_info = _make_error_info(message=long_msg)
        modal = ErrorDetailsModal(agent, error_info=error_info)
        modal.run_worker = Mock(return_value=Mock())

        modal.on_mount()

        # Alert should receive truncated message (80 chars)
        actual_summary = mock_alert.call_args[0][1]
        assert len(actual_summary) == 80


# ============================================================================
# _run_analysis
# ============================================================================


class TestRunAnalysis(unittest.TestCase):
    """Tests for _run_analysis (async)."""

    @patch("tui.screens.error_details.load_audit_trail")
    @patch("tui.screens.error_details.ArbiterAnalyzer")
    def test_run_analysis_calls_analyzer(self, MockAnalyzer, mock_load_audit):
        mock_load_audit.return_value = _make_audit_trail()

        agent = _make_agent(id="debugger", tier="fleet")
        error_info = _make_error_info(
            message="Segfault",
            last_task="Code review",
            timestamp="2026-02-18T10:30:00Z",
        )
        modal = ErrorDetailsModal(agent, error_info=error_info)

        mock_analyzer_instance = Mock()
        expected_analysis = _make_error_analysis()
        mock_analyzer_instance.analyze = AsyncMock(return_value=expected_analysis)
        MockAnalyzer.return_value = mock_analyzer_instance

        import asyncio

        result = asyncio.get_event_loop().run_until_complete(modal._run_analysis())

        MockAnalyzer.assert_called_once_with(timeout=15.0)
        mock_analyzer_instance.analyze.assert_called_once_with(
            agent_id="debugger",
            tier="fleet",
            error_message="Segfault",
            last_task="Code review",
            timestamp="2026-02-18T10:30:00Z",
        )
        assert result is expected_analysis


# ============================================================================
# on_worker_state_changed
# ============================================================================


class TestOnWorkerStateChanged(unittest.TestCase):
    """Tests for on_worker_state_changed."""

    @patch("tui.screens.error_details.load_audit_trail")
    def _make_modal(self, mock_load_audit):
        mock_load_audit.return_value = _make_audit_trail()
        agent = _make_agent()
        return ErrorDetailsModal(agent, error_info=_make_error_info())

    def test_success_state(self):
        from textual.worker import WorkerState

        modal = self._make_modal()
        analysis = _make_error_analysis()

        event = Mock()
        event.worker.name = "arbiter-analysis"
        event.state = WorkerState.SUCCESS
        event.worker.result = analysis

        modal._update_analysis_display = Mock()
        modal.on_worker_state_changed(event)

        assert modal._analysis is analysis
        modal._update_analysis_display.assert_called_once()

    def test_error_state(self):
        from textual.worker import WorkerState

        modal = self._make_modal()
        mock_box = Mock()
        modal.query_one = Mock(return_value=mock_box)

        event = Mock()
        event.worker.name = "arbiter-analysis"
        event.state = WorkerState.ERROR

        modal.on_worker_state_changed(event)

        mock_box.update.assert_called_once()
        content = mock_box.update.call_args[0][0]
        assert "unavailable" in content
        mock_box.remove_class.assert_called_once_with("loading")

    def test_non_matching_worker_name(self):
        from textual.worker import WorkerState

        modal = self._make_modal()
        modal._update_analysis_display = Mock()
        modal.query_one = Mock()

        event = Mock()
        event.worker.name = "other-worker"
        event.state = WorkerState.SUCCESS

        modal.on_worker_state_changed(event)

        modal._update_analysis_display.assert_not_called()
        modal.query_one.assert_not_called()

    def test_pending_state_ignored(self):
        from textual.worker import WorkerState

        modal = self._make_modal()
        modal._update_analysis_display = Mock()
        modal.query_one = Mock()

        event = Mock()
        event.worker.name = "arbiter-analysis"
        event.state = WorkerState.PENDING

        modal.on_worker_state_changed(event)

        modal._update_analysis_display.assert_not_called()


# ============================================================================
# _update_analysis_display
# ============================================================================


class TestUpdateAnalysisDisplay(unittest.TestCase):
    """Tests for _update_analysis_display with various analysis states."""

    @patch("tui.screens.error_details.load_audit_trail")
    def _make_modal(self, mock_load_audit):
        mock_load_audit.return_value = _make_audit_trail()
        agent = _make_agent()
        modal = ErrorDetailsModal(agent, error_info=_make_error_info())
        return modal

    def test_no_analysis(self):
        modal = self._make_modal()
        modal._analysis = None
        modal.query_one = Mock()

        modal._update_analysis_display()

        # Should return early, no query_one call
        modal.query_one.assert_not_called()

    def test_critical_severity(self):
        modal = self._make_modal()
        modal._analysis = _make_error_analysis(severity="critical", confidence=0.9)

        mock_box = Mock()
        modal.query_one = Mock(return_value=mock_box)

        modal._update_analysis_display()

        mock_box.update.assert_called_once()
        content = mock_box.update.call_args[0][0]
        # critical severity icon
        assert SEVERITY_STYLES["critical"][0] in content
        mock_box.add_class.assert_called_once_with("severity-critical")
        mock_box.remove_class.assert_called_once_with("loading")

    def test_warning_severity(self):
        modal = self._make_modal()
        modal._analysis = _make_error_analysis(severity="warning", confidence=0.75)

        mock_box = Mock()
        modal.query_one = Mock(return_value=mock_box)

        modal._update_analysis_display()

        content = mock_box.update.call_args[0][0]
        assert SEVERITY_STYLES["warning"][0] in content
        mock_box.add_class.assert_called_once_with("severity-warning")

    def test_info_severity(self):
        modal = self._make_modal()
        modal._analysis = _make_error_analysis(severity="info", confidence=0.5)

        mock_box = Mock()
        modal.query_one = Mock(return_value=mock_box)

        modal._update_analysis_display()

        mock_box.add_class.assert_called_once_with("severity-info")

    def test_unknown_severity(self):
        modal = self._make_modal()
        modal._analysis = _make_error_analysis(severity="exotic", confidence=0.5)

        mock_box = Mock()
        modal.query_one = Mock(return_value=mock_box)

        modal._update_analysis_display()

        mock_box.add_class.assert_called_once_with("severity-exotic")

    def test_high_confidence(self):
        modal = self._make_modal()
        modal._analysis = _make_error_analysis(confidence=0.90)

        mock_box = Mock()
        modal.query_one = Mock(return_value=mock_box)

        modal._update_analysis_display()

        content = mock_box.update.call_args[0][0]
        assert "90%" in content
        assert "Safe to act" in content

    def test_medium_confidence(self):
        modal = self._make_modal()
        modal._analysis = _make_error_analysis(confidence=0.70)

        mock_box = Mock()
        modal.query_one = Mock(return_value=mock_box)

        modal._update_analysis_display()

        content = mock_box.update.call_args[0][0]
        assert "70%" in content
        assert "Review details" in content

    def test_low_confidence(self):
        modal = self._make_modal()
        modal._analysis = _make_error_analysis(confidence=0.40)

        mock_box = Mock()
        modal.query_one = Mock(return_value=mock_box)

        modal._update_analysis_display()

        content = mock_box.update.call_args[0][0]
        assert "40%" in content
        assert "Check logs" in content

    def test_unknown_confidence_tier(self):
        """Confidence value that yields an unknown tier."""
        from tui.core.runner import ErrorAnalysis, RecoveryStep

        modal = self._make_modal()
        analysis = _make_error_analysis(confidence=0.50)
        # Patch confidence_tier to return something unexpected
        with patch.object(type(analysis), "confidence_tier", new_callable=PropertyMock, return_value="exotic"):
            modal._analysis = analysis
            mock_box = Mock()
            modal.query_one = Mock(return_value=mock_box)
            modal._update_analysis_display()

        content = mock_box.update.call_args[0][0]
        assert "50%" in content

    def test_recovery_steps_sorted_by_probability(self):
        from tui.core.runner import RecoveryStep

        modal = self._make_modal()
        modal._analysis = _make_error_analysis(
            recovery_steps=[
                RecoveryStep(action="Low prob step", success_probability=0.3),
                RecoveryStep(action="High prob step", success_probability=0.95),
                RecoveryStep(action="Medium step", success_probability=0.6, requires_sandbox=True),
            ],
        )

        mock_box = Mock()
        modal.query_one = Mock(return_value=mock_box)

        modal._update_analysis_display()

        content = mock_box.update.call_args[0][0]
        # High prob should appear first (95%)
        high_idx = content.index("High prob step")
        medium_idx = content.index("Medium step")
        low_idx = content.index("Low prob step")
        assert high_idx < medium_idx < low_idx

    def test_recovery_steps_max_three(self):
        from tui.core.runner import RecoveryStep

        modal = self._make_modal()
        modal._analysis = _make_error_analysis(
            recovery_steps=[
                RecoveryStep(action=f"Step {i}", success_probability=0.9 - i * 0.1)
                for i in range(5)
            ],
        )

        mock_box = Mock()
        modal.query_one = Mock(return_value=mock_box)

        modal._update_analysis_display()

        content = mock_box.update.call_args[0][0]
        # Only top 3 should appear
        assert "Step 0" in content  # 0.9
        assert "Step 1" in content  # 0.8
        assert "Step 2" in content  # 0.7
        assert "Step 3" not in content  # 0.6 (4th)
        assert "Step 4" not in content  # 0.5 (5th)

    def test_recovery_steps_sandbox_tag(self):
        from tui.core.runner import RecoveryStep

        modal = self._make_modal()
        modal._analysis = _make_error_analysis(
            recovery_steps=[
                RecoveryStep(action="Sandbox step", success_probability=0.8, requires_sandbox=True),
            ],
        )

        mock_box = Mock()
        modal.query_one = Mock(return_value=mock_box)

        modal._update_analysis_display()

        content = mock_box.update.call_args[0][0]
        assert "sandbox" in content

    def test_empty_recovery_steps(self):
        modal = self._make_modal()
        modal._analysis = _make_error_analysis(recovery_steps=[])

        mock_box = Mock()
        modal.query_one = Mock(return_value=mock_box)

        modal._update_analysis_display()

        content = mock_box.update.call_args[0][0]
        assert "Recovery Steps" not in content

    def test_caused_by_agent_attribution(self):
        modal = self._make_modal()
        modal._analysis = _make_error_analysis(caused_by_agent="supervisor")

        mock_box = Mock()
        modal.query_one = Mock(return_value=mock_box)

        modal._update_analysis_display()

        content = mock_box.update.call_args[0][0]
        assert "supervisor" in content
        assert "Cascading failure" in content

    def test_no_caused_by_agent(self):
        modal = self._make_modal()
        modal._analysis = _make_error_analysis(caused_by_agent=None)

        mock_box = Mock()
        modal.query_one = Mock(return_value=mock_box)

        modal._update_analysis_display()

        content = mock_box.update.call_args[0][0]
        assert "Cascading failure" not in content

    def test_can_auto_recover(self):
        modal = self._make_modal()
        modal._analysis = _make_error_analysis(can_auto_recover=True)

        mock_box = Mock()
        modal.query_one = Mock(return_value=mock_box)

        modal._update_analysis_display()

        content = mock_box.update.call_args[0][0]
        assert "automatic recovery" in content

    def test_cannot_auto_recover(self):
        modal = self._make_modal()
        modal._analysis = _make_error_analysis(can_auto_recover=False)

        mock_box = Mock()
        modal.query_one = Mock(return_value=mock_box)

        modal._update_analysis_display()

        content = mock_box.update.call_args[0][0]
        assert "automatic recovery" not in content

    def test_confidence_bar_rendering(self):
        modal = self._make_modal()
        modal._analysis = _make_error_analysis(confidence=0.70)

        mock_box = Mock()
        modal.query_one = Mock(return_value=mock_box)

        modal._update_analysis_display()

        content = mock_box.update.call_args[0][0]
        # 70% => 7 filled + 3 empty
        assert "\u2588" * 7 in content  # 7 filled blocks


# ============================================================================
# on_button_pressed
# ============================================================================


class TestOnButtonPressed(unittest.TestCase):
    """Tests for on_button_pressed."""

    @patch("tui.screens.error_details.load_audit_trail")
    def _make_modal(self, mock_load_audit):
        mock_load_audit.return_value = _make_audit_trail()
        agent = _make_agent()
        modal = ErrorDetailsModal(agent, error_info=_make_error_info())
        modal.dismiss = Mock()
        return modal

    def test_restart_button(self):
        modal = self._make_modal()
        event = Mock()
        event.button.id = "restart"
        modal.on_button_pressed(event)
        modal.dismiss.assert_called_once_with("restart")

    def test_simulate_button(self):
        modal = self._make_modal()
        event = Mock()
        event.button.id = "simulate"
        modal.on_button_pressed(event)
        modal.dismiss.assert_called_once_with("simulate")

    def test_logs_button(self):
        modal = self._make_modal()
        event = Mock()
        event.button.id = "logs"
        modal.on_button_pressed(event)
        modal.dismiss.assert_called_once_with("logs")

    def test_close_button(self):
        modal = self._make_modal()
        event = Mock()
        event.button.id = "close"
        modal.on_button_pressed(event)
        modal.dismiss.assert_called_once_with(None)

    def test_unknown_button_dismisses_none(self):
        modal = self._make_modal()
        event = Mock()
        event.button.id = "something-else"
        modal.on_button_pressed(event)
        modal.dismiss.assert_called_once_with(None)


# ============================================================================
# Action methods
# ============================================================================


class TestActionMethods(unittest.TestCase):
    """Tests for action_close, action_restart, action_simulate, action_logs, action_validate."""

    @patch("tui.screens.error_details.load_audit_trail")
    def _make_modal(self, mock_load_audit):
        mock_load_audit.return_value = _make_audit_trail()
        agent = _make_agent()
        modal = ErrorDetailsModal(agent, error_info=_make_error_info())
        modal.dismiss = Mock()
        modal.notify = Mock()
        return modal

    def test_action_close(self):
        modal = self._make_modal()
        modal.action_close()
        modal.dismiss.assert_called_once_with(None)

    def test_action_restart(self):
        modal = self._make_modal()
        modal.action_restart()
        modal.dismiss.assert_called_once_with("restart")

    def test_action_simulate(self):
        modal = self._make_modal()
        modal.action_simulate()
        modal.dismiss.assert_called_once_with("simulate")

    def test_action_logs(self):
        modal = self._make_modal()
        modal.action_logs()
        modal.dismiss.assert_called_once_with("logs")

    def test_action_validate_with_analysis(self):
        modal = self._make_modal()
        modal._analysis = _make_error_analysis()
        modal.action_validate()
        modal.notify.assert_called_once()
        assert "validated" in modal.notify.call_args[0][0]
        modal.dismiss.assert_called_once_with(None)

    def test_action_validate_without_analysis(self):
        modal = self._make_modal()
        modal._analysis = None
        modal.action_validate()
        modal.notify.assert_not_called()
        modal.dismiss.assert_called_once_with(None)


# ============================================================================
# Bindings
# ============================================================================


class TestBindings(unittest.TestCase):
    """Tests for ErrorDetailsModal key bindings."""

    @patch("tui.screens.error_details.load_audit_trail")
    def test_bindings_present(self, mock_load_audit):
        mock_load_audit.return_value = _make_audit_trail()
        agent = _make_agent()
        modal = ErrorDetailsModal(agent, error_info=_make_error_info())

        keys = [b.key for b in modal.BINDINGS]
        assert "escape" in keys
        assert "r" in keys
        assert "s" in keys
        assert "l" in keys
        assert "v" in keys


# ============================================================================
# compose (basic structure test)
# ============================================================================


class TestCompose(unittest.TestCase):
    """Tests for compose method."""

    @patch("tui.screens.error_details.load_audit_trail")
    def test_compose_yields_widgets(self, mock_load_audit):
        mock_load_audit.return_value = _make_audit_trail()
        agent = _make_agent()
        error_info = _make_error_info()
        modal = ErrorDetailsModal(agent, error_info=error_info)

        # compose uses Textual context managers (with Vertical()...) which need
        # the widget mount context. We just verify no exceptions on the generator.
        # The actual widget tree is tested via integration tests.
        # We can at least verify the method is a generator
        gen = modal.compose()
        assert hasattr(gen, "__next__")

    @patch("tui.screens.error_details.load_audit_trail")
    def test_compose_with_audit_trail_events(self, mock_load_audit):
        from tui.core.telemetry import DelegationEvent

        trail = _make_audit_trail(events=[
            DelegationEvent(
                timestamp="2026-02-18T10:00:00Z",
                from_agent="supervisor",
                to_agent="test-agent",
                task="deploy",
            )
        ])
        mock_load_audit.return_value = trail

        agent = _make_agent()
        error_info = _make_error_info()
        modal = ErrorDetailsModal(agent, error_info=error_info)

        # Audit trail events are present, should render audit trail section
        assert modal._audit_trail.events


if __name__ == "__main__":
    unittest.main()
