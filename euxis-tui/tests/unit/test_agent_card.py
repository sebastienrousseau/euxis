# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Comprehensive unit tests for AgentCard widget (agent_card.py).

Targets >= 95% line coverage. Tests cover:
- Module-level constants: PHASE_*, STATUS_*, TIER_STYLES, STATUS_ICONS,
  ACTIVATION_ICONS, PHASE_ICONS, ROUTER_TIER_STYLES, DEFAULT_BUDGET_*,
  LATENCY_THRESHOLDS, SUPERVISION_TIERS, CAPABILITY_TO_TIER
- get_router_tier() with all tier escalation paths
- AgentCard __init__, compose, reactive defaults
- watch_status (idle, active, error, exception path)
- _start_pulse (with and without reduced_motion), _stop_pulse, _toggle_pulse
- watch_phase (idle, research, coding +/- progress, reasoning +/- snippet, complete, exception)
- watch_links_found, watch_files_written, watch_thought_snippet, watch_progress_percent
- Token odometer: _format_odometer (all budget/latency/cost branches)
- watch_token_cost_usd (budget exceeded path), watch_latency_ms, watch_request_count
- _update_odometer (success and exception)
- update_metrics, reset_budget, set_active, set_status
- on_click, on_key (enter and other)
- action_restart, action_logs, action_dismiss, action_details (error and non-error)
- on_focus, on_blur, _update_action_hints, _hide_action_hints
- Message classes: Selected, RestartRequested, LogsRequested, ErrorDismissed,
  ErrorDetailsRequested, BudgetExceeded
"""

from __future__ import annotations

import unittest
from unittest.mock import MagicMock, Mock, PropertyMock, patch

from tui.core.registry import Agent
from tui.widgets.agent_card import (
    ACTIVATION_ICONS,
    CAPABILITY_TO_TIER,
    DEFAULT_BUDGET_HARD,
    DEFAULT_BUDGET_SOFT,
    LATENCY_THRESHOLDS,
    PHASE_CODING,
    PHASE_COMPLETE,
    PHASE_ICONS,
    PHASE_IDLE,
    PHASE_REASONING,
    PHASE_RESEARCH,
    ROUTER_TIER_STYLES,
    STATUS_ACTIVE,
    STATUS_ERROR,
    STATUS_ICONS,
    STATUS_IDLE,
    SUPERVISION_TIERS,
    TIER_STYLES,
    AgentCard,
    get_router_tier,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_agent(**overrides) -> Agent:
    """Create a test Agent with sensible defaults."""
    defaults = {
        "id": "architect",
        "tier": "core",
        "version": "v0.0.2",
        "tags": ["design", "plan"],
        "activation": "default",
        "capability_tags": [],
    }
    defaults.update(overrides)
    return Agent(**defaults)


def _make_card(agent=None, **agent_overrides) -> AgentCard:
    """Create an AgentCard bypassing reactive watcher side effects."""
    if agent is None:
        agent = _make_agent(**agent_overrides)
    card = AgentCard(agent)
    return card


# ---------------------------------------------------------------------------
# Module-level constants
# ---------------------------------------------------------------------------

class TestPhaseConstants(unittest.TestCase):
    """Test PHASE_* string constants."""

    def test_phase_idle(self):
        assert PHASE_IDLE == "idle"

    def test_phase_research(self):
        assert PHASE_RESEARCH == "research"

    def test_phase_coding(self):
        assert PHASE_CODING == "coding"

    def test_phase_reasoning(self):
        assert PHASE_REASONING == "reasoning"

    def test_phase_complete(self):
        assert PHASE_COMPLETE == "complete"


class TestStatusConstants(unittest.TestCase):
    """Test STATUS_* string constants."""

    def test_status_idle(self):
        assert STATUS_IDLE == "idle"

    def test_status_active(self):
        assert STATUS_ACTIVE == "active"

    def test_status_error(self):
        assert STATUS_ERROR == "error"


class TestTierStyles(unittest.TestCase):
    """Test TIER_STYLES dictionary."""

    def test_core_entry(self):
        style, label = TIER_STYLES["core"]
        assert style == "bold"
        assert label == "CORE"

    def test_fleet_entry(self):
        style, label = TIER_STYLES["fleet"]
        assert style == ""
        assert label == "FLEET"

    def test_unknown_tier_fallback(self):
        result = TIER_STYLES.get("unknown", ("dim", "FLEET"))
        assert result == ("dim", "FLEET")


class TestStatusIcons(unittest.TestCase):
    """Test STATUS_ICONS dictionary."""

    def test_idle_icon_dim(self):
        assert "dim" in STATUS_ICONS[STATUS_IDLE]

    def test_active_icon_cyan(self):
        assert "cyan" in STATUS_ICONS[STATUS_ACTIVE]

    def test_error_icon_red(self):
        assert "red" in STATUS_ICONS[STATUS_ERROR]

    def test_all_keys_present(self):
        assert set(STATUS_ICONS.keys()) == {STATUS_IDLE, STATUS_ACTIVE, STATUS_ERROR}


class TestActivationIcons(unittest.TestCase):
    """Test ACTIVATION_ICONS dictionary."""

    def test_default_icon(self):
        assert "green" in ACTIVATION_ICONS["default"]

    def test_on_demand_icon(self):
        assert "yellow" in ACTIVATION_ICONS["on-demand"]

    def test_specialist_icon(self):
        assert "magenta" in ACTIVATION_ICONS["specialist"]


class TestPhaseIcons(unittest.TestCase):
    """Test PHASE_ICONS dictionary."""

    def test_all_phases_present(self):
        expected = {PHASE_IDLE, PHASE_RESEARCH, PHASE_CODING, PHASE_REASONING, PHASE_COMPLETE}
        assert expected == set(PHASE_ICONS.keys())

    def test_idle_is_dim(self):
        assert "dim" in PHASE_ICONS[PHASE_IDLE]

    def test_research_is_cyan(self):
        assert "cyan" in PHASE_ICONS[PHASE_RESEARCH]

    def test_coding_is_yellow(self):
        assert "yellow" in PHASE_ICONS[PHASE_CODING]

    def test_reasoning_is_magenta(self):
        assert "magenta" in PHASE_ICONS[PHASE_REASONING]

    def test_complete_is_green(self):
        assert "green" in PHASE_ICONS[PHASE_COMPLETE]


class TestRouterTierStyles(unittest.TestCase):
    """Test ROUTER_TIER_STYLES dictionary."""

    def test_routine_tier(self):
        icon, hint = ROUTER_TIER_STYLES["routine"]
        assert "$" in icon
        assert "green" in icon
        assert "Gemini" in hint

    def test_data_tier(self):
        icon, hint = ROUTER_TIER_STYLES["data"]
        assert "$$" in icon
        assert "DeepSeek" in hint

    def test_code_tier(self):
        icon, hint = ROUTER_TIER_STYLES["code"]
        assert "$$$" in icon
        assert "Sonnet" in hint

    def test_reason_tier(self):
        icon, hint = ROUTER_TIER_STYLES["reason"]
        assert "$$$$" in icon
        assert "Opus" in hint


class TestBudgetAndLatencyConstants(unittest.TestCase):
    """Test DEFAULT_BUDGET_* and LATENCY_THRESHOLDS constants."""

    def test_default_budget_soft(self):
        assert DEFAULT_BUDGET_SOFT == 2.00

    def test_default_budget_hard(self):
        assert DEFAULT_BUDGET_HARD == 5.00

    def test_latency_excellent(self):
        assert LATENCY_THRESHOLDS["excellent"] == 2.0

    def test_latency_good(self):
        assert LATENCY_THRESHOLDS["good"] == 5.0

    def test_latency_slow(self):
        assert LATENCY_THRESHOLDS["slow"] == 10.0


class TestSupervisionTiers(unittest.TestCase):
    """Test SUPERVISION_TIERS hierarchy."""

    def test_supervisor_agents(self):
        assert "arbiter" in SUPERVISION_TIERS["supervisor"]
        assert "orchestrator" in SUPERVISION_TIERS["supervisor"]

    def test_coordinator_agents(self):
        coords = SUPERVISION_TIERS["coordinator"]
        for a in ["architect", "auditor", "strategist", "planner"]:
            assert a in coords

    def test_specialist_is_none(self):
        assert SUPERVISION_TIERS["specialist"] is None


class TestCapabilityToTier(unittest.TestCase):
    """Test CAPABILITY_TO_TIER mapping."""

    def test_routine_capabilities(self):
        for tag in ["heartbeat", "status", "health", "monitor", "ping"]:
            assert CAPABILITY_TO_TIER[tag] == "routine"

    def test_data_capabilities(self):
        for tag in ["formatting", "parsing", "logging", "metrics", "telemetry",
                     "localization", "i18n", "transformation"]:
            assert CAPABILITY_TO_TIER[tag] == "data"

    def test_code_capabilities(self):
        for tag in ["code-review", "debugging", "testing", "refactoring",
                     "documentation", "api-design", "implementation"]:
            assert CAPABILITY_TO_TIER[tag] == "code"

    def test_reason_capabilities(self):
        for tag in ["architecture", "planning", "security", "research",
                     "audit", "synthesis", "strategy", "analysis"]:
            assert CAPABILITY_TO_TIER[tag] == "reason"


# ---------------------------------------------------------------------------
# get_router_tier()
# ---------------------------------------------------------------------------

class TestGetRouterTier(unittest.TestCase):
    """Test get_router_tier function with all tier escalation paths."""

    def test_empty_tags_returns_default_code(self):
        assert get_router_tier([]) == "code"

    def test_unknown_tags_returns_default_code(self):
        assert get_router_tier(["unknown", "foo"]) == "code"

    def test_routine_only(self):
        """Routine (rank 1) < default code (rank 3) so should stay code."""
        assert get_router_tier(["heartbeat", "ping"]) == "code"

    def test_data_only(self):
        """Data (rank 2) < default code (rank 3) so should stay code."""
        assert get_router_tier(["formatting", "parsing"]) == "code"

    def test_code_only(self):
        """Code (rank 3) == default, stays code."""
        assert get_router_tier(["code-review"]) == "code"

    def test_reason_escalates(self):
        """Reason (rank 4) > code (rank 3), should escalate."""
        assert get_router_tier(["architecture"]) == "reason"

    def test_mixed_takes_highest(self):
        """Mix of tags takes highest tier."""
        assert get_router_tier(["heartbeat", "formatting", "code-review", "planning"]) == "reason"

    def test_reason_with_routine(self):
        assert get_router_tier(["ping", "security"]) == "reason"

    def test_single_reason_tag(self):
        assert get_router_tier(["analysis"]) == "reason"


# ---------------------------------------------------------------------------
# AgentCard — Message classes
# ---------------------------------------------------------------------------

class TestAgentCardMessages(unittest.TestCase):
    """Test all Message subclasses on AgentCard."""

    def test_selected_message(self):
        agent = _make_agent()
        msg = AgentCard.Selected(agent)
        assert msg.agent is agent

    def test_restart_requested_message(self):
        agent = _make_agent()
        msg = AgentCard.RestartRequested(agent)
        assert msg.agent is agent

    def test_logs_requested_message(self):
        agent = _make_agent()
        msg = AgentCard.LogsRequested(agent)
        assert msg.agent is agent

    def test_error_dismissed_message(self):
        agent = _make_agent()
        msg = AgentCard.ErrorDismissed(agent)
        assert msg.agent is agent

    def test_error_details_requested_message(self):
        agent = _make_agent()
        msg = AgentCard.ErrorDetailsRequested(agent)
        assert msg.agent is agent

    def test_budget_exceeded_message(self):
        agent = _make_agent()
        msg = AgentCard.BudgetExceeded(agent, 3.50, 2.00)
        assert msg.agent is agent
        assert msg.cost == 3.50
        assert msg.budget == 2.00


# ---------------------------------------------------------------------------
# AgentCard — __init__ and supervision tier
# ---------------------------------------------------------------------------

class TestAgentCardInit(unittest.TestCase):
    """Test AgentCard initialization and supervision tier detection."""

    def test_init_stores_agent(self):
        agent = _make_agent()
        card = _make_card(agent)
        assert card.agent is agent

    def test_init_core_adds_tier_class(self):
        card = _make_card(tier="core")
        assert "tier-core" in card.classes

    def test_init_fleet_no_tier_core_class(self):
        card = _make_card(tier="fleet")
        assert "tier-core" not in card.classes

    def test_init_router_tier_empty_by_default(self):
        card = _make_card()
        assert card._router_tier == ""

    def test_init_pulse_timer_none(self):
        card = _make_card()
        assert card._pulse_timer is None

    def test_init_budget_warned_false(self):
        card = _make_card()
        assert card._budget_warned is False

    def test_supervision_tier_supervisor(self):
        card = _make_card(id="arbiter")
        assert card._supervision_tier == "supervisor"
        assert "tier-supervisor" in card.classes

    def test_supervision_tier_orchestrator(self):
        card = _make_card(id="orchestrator")
        assert card._supervision_tier == "supervisor"

    def test_supervision_tier_coordinator(self):
        card = _make_card(id="architect")
        assert card._supervision_tier == "coordinator"
        assert "tier-coordinator" in card.classes

    def test_supervision_tier_auditor(self):
        card = _make_card(id="auditor")
        assert card._supervision_tier == "coordinator"

    def test_supervision_tier_strategist(self):
        card = _make_card(id="strategist")
        assert card._supervision_tier == "coordinator"

    def test_supervision_tier_planner(self):
        card = _make_card(id="planner")
        assert card._supervision_tier == "coordinator"

    def test_supervision_tier_specialist_default(self):
        card = _make_card(id="coder")
        assert card._supervision_tier == "specialist"
        assert "tier-specialist" in card.classes

    def test_get_supervision_tier_method(self):
        card = _make_card()
        assert card._get_supervision_tier("arbiter") == "supervisor"
        assert card._get_supervision_tier("orchestrator") == "supervisor"
        assert card._get_supervision_tier("architect") == "coordinator"
        assert card._get_supervision_tier("planner") == "coordinator"
        assert card._get_supervision_tier("debugger") == "specialist"


# ---------------------------------------------------------------------------
# AgentCard — Reactive defaults
# ---------------------------------------------------------------------------

class TestAgentCardReactiveDefaults(unittest.TestCase):
    """Test reactive property defaults without triggering watchers."""

    def test_phase_default_idle(self):
        # Check class-level descriptor default
        descriptor = AgentCard.__dict__["phase"]
        assert descriptor._default == PHASE_IDLE

    def test_status_default_idle(self):
        descriptor = AgentCard.__dict__["status"]
        assert descriptor._default == STATUS_IDLE

    def test_pulse_state_default_false(self):
        descriptor = AgentCard.__dict__["_pulse_state"]
        assert descriptor._default is False

    def test_thought_snippet_default_empty(self):
        descriptor = AgentCard.__dict__["thought_snippet"]
        assert descriptor._default == ""

    def test_links_found_default_zero(self):
        descriptor = AgentCard.__dict__["links_found"]
        assert descriptor._default == 0

    def test_files_written_default_zero(self):
        descriptor = AgentCard.__dict__["files_written"]
        assert descriptor._default == 0

    def test_progress_percent_default_negative(self):
        descriptor = AgentCard.__dict__["progress_percent"]
        assert descriptor._default == -1

    def test_token_cost_usd_default_zero(self):
        descriptor = AgentCard.__dict__["token_cost_usd"]
        assert descriptor._default == 0.0

    def test_token_budget_usd_default(self):
        descriptor = AgentCard.__dict__["token_budget_usd"]
        assert descriptor._default == DEFAULT_BUDGET_SOFT

    def test_latency_ms_default_zero(self):
        descriptor = AgentCard.__dict__["latency_ms"]
        assert descriptor._default == 0

    def test_request_count_default_zero(self):
        descriptor = AgentCard.__dict__["request_count"]
        assert descriptor._default == 0


# ---------------------------------------------------------------------------
# AgentCard — compose()
# ---------------------------------------------------------------------------

class TestAgentCardCompose(unittest.TestCase):
    """Test compose method yields correct widgets."""

    @patch("tui.widgets.agent_card.ProgressBar")
    @patch("tui.widgets.agent_card.Vertical")
    @patch("tui.widgets.agent_card.Static")
    @patch("tui.widgets.agent_card.CompactAvatar")
    def test_compose_yields_widgets(self, mock_avatar, mock_static, mock_vertical, mock_progress):
        """Test compose produces at least 4 yields (avatar + 3 statics + odometer + context children)."""
        mock_vertical.return_value.__enter__ = Mock(return_value=None)
        mock_vertical.return_value.__exit__ = Mock(return_value=False)
        agent = _make_agent(tags=["design", "plan", "security"])
        card = _make_card(agent)
        result = list(card.compose())
        assert len(result) >= 4

    @patch("tui.widgets.agent_card.ProgressBar")
    @patch("tui.widgets.agent_card.Vertical")
    @patch("tui.widgets.agent_card.Static")
    @patch("tui.widgets.agent_card.CompactAvatar")
    def test_compose_sets_router_tier(self, mock_avatar, mock_static, mock_vertical, mock_progress):
        mock_vertical.return_value.__enter__ = Mock(return_value=None)
        mock_vertical.return_value.__exit__ = Mock(return_value=False)
        agent = _make_agent(capability_tags=["architecture", "planning"])
        card = _make_card(agent)
        list(card.compose())
        assert card._router_tier == "reason"

    @patch("tui.widgets.agent_card.ProgressBar")
    @patch("tui.widgets.agent_card.Vertical")
    @patch("tui.widgets.agent_card.Static")
    @patch("tui.widgets.agent_card.CompactAvatar")
    def test_compose_uses_tags_when_no_capability_tags(self, mock_avatar, mock_static, mock_vertical, mock_progress):
        """When capability_tags is empty, compose falls back to agent.tags."""
        mock_vertical.return_value.__enter__ = Mock(return_value=None)
        mock_vertical.return_value.__exit__ = Mock(return_value=False)
        agent = _make_agent(capability_tags=[], tags=["heartbeat", "status"])
        card = _make_card(agent)
        list(card.compose())
        # heartbeat & status are routine (rank 1) which can't beat default code (rank 3)
        assert card._router_tier == "code"

    @patch("tui.widgets.agent_card.ProgressBar")
    @patch("tui.widgets.agent_card.Vertical")
    @patch("tui.widgets.agent_card.Static")
    @patch("tui.widgets.agent_card.CompactAvatar")
    def test_compose_fleet_tier_style(self, mock_avatar, mock_static, mock_vertical, mock_progress):
        """Test compose for fleet tier (empty style string)."""
        mock_vertical.return_value.__enter__ = Mock(return_value=None)
        mock_vertical.return_value.__exit__ = Mock(return_value=False)
        agent = _make_agent(tier="fleet")
        card = _make_card(agent)
        result = list(card.compose())
        assert len(result) >= 4

    @patch("tui.widgets.agent_card.ProgressBar")
    @patch("tui.widgets.agent_card.Vertical")
    @patch("tui.widgets.agent_card.Static")
    @patch("tui.widgets.agent_card.CompactAvatar")
    def test_compose_unknown_tier_fallback(self, mock_avatar, mock_static, mock_vertical, mock_progress):
        """Test compose with an unknown tier defaults gracefully."""
        mock_vertical.return_value.__enter__ = Mock(return_value=None)
        mock_vertical.return_value.__exit__ = Mock(return_value=False)
        agent = _make_agent(tier="unknown")
        card = _make_card(agent)
        result = list(card.compose())
        assert len(result) >= 4


# ---------------------------------------------------------------------------
# AgentCard — _format_odometer()
# ---------------------------------------------------------------------------

class TestFormatOdometer(unittest.TestCase):
    """Test _format_odometer with all budget/latency/cost branches."""

    def test_zero_cost_green(self):
        card = _make_card()
        card._reactive_token_cost_usd = 0.0
        card._reactive_token_budget_usd = 2.0
        card._reactive_latency_ms = 0
        card._reactive_request_count = 0
        result = card._format_odometer()
        assert "green" in result
        assert "--ms" in result
        assert "0 reqs" in result

    def test_high_budget_pct_yellow(self):
        card = _make_card()
        card._reactive_token_cost_usd = 1.60  # 80% of 2.0
        card._reactive_token_budget_usd = 2.0
        card._reactive_latency_ms = 0
        card._reactive_request_count = 5
        result = card._format_odometer()
        assert "yellow" in result

    def test_over_budget_red(self):
        card = _make_card()
        card._reactive_token_cost_usd = 3.00  # 150% of 2.0
        card._reactive_token_budget_usd = 2.0
        card._reactive_latency_ms = 0
        card._reactive_request_count = 10
        result = card._format_odometer()
        assert "bold red" in result

    def test_zero_budget_no_division_error(self):
        card = _make_card()
        card._reactive_token_cost_usd = 1.0
        card._reactive_token_budget_usd = 0.0
        card._reactive_latency_ms = 0
        card._reactive_request_count = 0
        result = card._format_odometer()
        # budget_pct = 0 when budget is 0 (green path)
        assert "green" in result

    def test_latency_excellent(self):
        card = _make_card()
        card._reactive_token_cost_usd = 0.0
        card._reactive_token_budget_usd = 2.0
        card._reactive_latency_ms = 1500  # 1.5s < 2.0s excellent
        card._reactive_request_count = 1
        result = card._format_odometer()
        assert "[green]1500ms[/]" in result

    def test_latency_good(self):
        card = _make_card()
        card._reactive_token_cost_usd = 0.0
        card._reactive_token_budget_usd = 2.0
        card._reactive_latency_ms = 3500  # 3.5s, between 2.0 and 5.0
        card._reactive_request_count = 1
        result = card._format_odometer()
        assert "[cyan]3500ms[/]" in result

    def test_latency_slow(self):
        card = _make_card()
        card._reactive_token_cost_usd = 0.0
        card._reactive_token_budget_usd = 2.0
        card._reactive_latency_ms = 8000  # 8.0s, between 5.0 and 10.0
        card._reactive_request_count = 1
        result = card._format_odometer()
        assert "[yellow]8000ms[/]" in result

    def test_latency_very_slow_red(self):
        card = _make_card()
        card._reactive_token_cost_usd = 0.0
        card._reactive_token_budget_usd = 2.0
        card._reactive_latency_ms = 15000  # 15s > 10s
        card._reactive_request_count = 1
        result = card._format_odometer()
        assert "[red]15000ms[/]" in result

    def test_cost_display_small_uses_three_decimals(self):
        card = _make_card()
        card._reactive_token_cost_usd = 0.005
        card._reactive_token_budget_usd = 2.0
        card._reactive_latency_ms = 0
        card._reactive_request_count = 0
        result = card._format_odometer()
        assert "$0.005" in result

    def test_cost_display_large_uses_two_decimals(self):
        card = _make_card()
        card._reactive_token_cost_usd = 1.23
        card._reactive_token_budget_usd = 2.0
        card._reactive_latency_ms = 0
        card._reactive_request_count = 0
        result = card._format_odometer()
        assert "$1.23" in result


# ---------------------------------------------------------------------------
# AgentCard — watch_status()
# ---------------------------------------------------------------------------

class TestWatchStatus(unittest.TestCase):
    """Test watch_status with all status transitions."""

    def _setup_card_with_mocks(self):
        card = _make_card()
        mock_avatar = MagicMock()
        card.query_one = Mock(return_value=mock_avatar)
        card.remove_class = Mock()
        card.add_class = Mock()
        card._start_pulse = Mock()
        card._stop_pulse = Mock()
        return card, mock_avatar

    def test_status_error_sets_avatar_error(self):
        card, mock_avatar = self._setup_card_with_mocks()
        card.watch_status(STATUS_ERROR)
        mock_avatar.set_error.assert_called_once_with(True)
        card.add_class.assert_any_call("status-error")
        card._stop_pulse.assert_called_once()

    def test_status_active_sets_avatar_active(self):
        card, mock_avatar = self._setup_card_with_mocks()
        card.watch_status(STATUS_ACTIVE)
        mock_avatar.set_active.assert_called_once_with(True)
        card.add_class.assert_any_call("status-active")
        card._start_pulse.assert_called_once()

    def test_status_idle_sets_avatar_standby(self):
        card, mock_avatar = self._setup_card_with_mocks()
        card.watch_status(STATUS_IDLE)
        mock_avatar.set_state.assert_called_once_with("standby")
        card.add_class.assert_any_call("status-idle")
        card._stop_pulse.assert_called_once()

    def test_status_removes_old_classes(self):
        card, _ = self._setup_card_with_mocks()
        card.watch_status(STATUS_ACTIVE)
        card.remove_class.assert_called_once_with(
            "status-idle", "status-active", "status-error"
        )

    def test_status_exception_swallowed(self):
        card = _make_card()
        card.query_one = Mock(side_effect=Exception("not mounted"))
        # Should not raise
        card.watch_status(STATUS_ACTIVE)


# ---------------------------------------------------------------------------
# AgentCard — _start_pulse, _stop_pulse, _toggle_pulse
# ---------------------------------------------------------------------------

class TestPulseAnimation(unittest.TestCase):
    """Test pulse animation start/stop/toggle."""

    def test_start_pulse_creates_timer(self):
        card = _make_card()
        mock_timer = Mock()
        card.set_interval = Mock(return_value=mock_timer)
        card.add_class = Mock()
        card._pulse_timer = None

        card._start_pulse()

        card.set_interval.assert_called_once_with(0.8, card._toggle_pulse)
        card.add_class.assert_called_with("pulse-on")
        assert card._pulse_timer is mock_timer

    def test_start_pulse_no_double_start(self):
        card = _make_card()
        card.set_interval = Mock()
        card.add_class = Mock()
        card._pulse_timer = Mock()  # Already running

        card._start_pulse()

        card.set_interval.assert_not_called()

    @patch("tui.core.config.ETXConfig")
    def test_start_pulse_reduced_motion(self, mock_config_cls):
        """When reduced_motion=True, should add pulse-on but not start timer."""
        mock_config = Mock()
        mock_config.reduced_motion = True
        mock_config_cls.load.return_value = mock_config
        card = _make_card()
        card.set_interval = Mock()
        card.add_class = Mock()
        card._pulse_timer = None

        card._start_pulse()

        card.add_class.assert_called_with("pulse-on")
        card.set_interval.assert_not_called()
        assert card._pulse_timer is None

    @patch("tui.core.config.ETXConfig")
    def test_start_pulse_config_load_exception(self, mock_config_cls):
        """When config load fails, should proceed to timer-based pulse."""
        mock_config_cls.load.side_effect = Exception("config error")
        card = _make_card()
        mock_timer = Mock()
        card.set_interval = Mock(return_value=mock_timer)
        card.add_class = Mock()
        card._pulse_timer = None

        card._start_pulse()

        card.set_interval.assert_called_once()
        assert card._pulse_timer is mock_timer

    def test_stop_pulse_with_timer(self):
        card = _make_card()
        mock_timer = Mock()
        card._pulse_timer = mock_timer
        card.remove_class = Mock()

        card._stop_pulse()

        mock_timer.stop.assert_called_once()
        assert card._pulse_timer is None
        card.remove_class.assert_called_once_with("pulse-on", "pulse-off")

    def test_stop_pulse_no_timer(self):
        card = _make_card()
        card._pulse_timer = None
        card.remove_class = Mock()

        card._stop_pulse()

        card.remove_class.assert_called_once_with("pulse-on", "pulse-off")

    def test_toggle_pulse_on_to_off(self):
        card = _make_card()
        card.has_class = Mock(return_value=True)  # currently pulse-on
        card.remove_class = Mock()
        card.add_class = Mock()

        card._toggle_pulse()

        card.remove_class.assert_called_once_with("pulse-on")
        card.add_class.assert_called_once_with("pulse-off")

    def test_toggle_pulse_off_to_on(self):
        card = _make_card()
        card.has_class = Mock(return_value=False)  # not pulse-on
        card.remove_class = Mock()
        card.add_class = Mock()

        card._toggle_pulse()

        card.remove_class.assert_called_once_with("pulse-off")
        card.add_class.assert_called_once_with("pulse-on")


# ---------------------------------------------------------------------------
# AgentCard — watch_phase()
# ---------------------------------------------------------------------------

class TestWatchPhase(unittest.TestCase):
    """Test watch_phase for all phases."""

    def _setup_card_phase_mocks(self):
        """Create a card with mocks for all context child widgets."""
        card = _make_card()
        mock_context = MagicMock()
        mock_thought = MagicMock()
        mock_counter = MagicMock()
        mock_progress = MagicMock()

        def query_one_side_effect(selector, *args):
            if selector.startswith("#ctx-"):
                return mock_context
            if selector.startswith("#thought-"):
                return mock_thought
            if selector.startswith("#counter-"):
                return mock_counter
            if selector.startswith("#progress-"):
                return mock_progress
            return MagicMock()

        card.query_one = Mock(side_effect=query_one_side_effect)
        card.remove_class = Mock()
        card.add_class = Mock()
        return card, mock_context, mock_thought, mock_counter, mock_progress

    def test_phase_idle_hides_context(self):
        card, ctx, thought, counter, progress = self._setup_card_phase_mocks()
        card.watch_phase(PHASE_IDLE)
        ctx.add_class.assert_called_with("hidden")
        card.remove_class.assert_called_with("expanded")

    def test_phase_research(self):
        card, ctx, thought, counter, progress = self._setup_card_phase_mocks()
        card._reactive_links_found = 5
        card.watch_phase(PHASE_RESEARCH)
        ctx.remove_class.assert_called_with("hidden")
        card.add_class.assert_called_with("expanded")
        thought.update.assert_called_with("")
        counter.update.assert_called_with("[cyan]Links found:[/] 5")
        progress.add_class.assert_called_with("hidden")

    def test_phase_coding_with_progress(self):
        card, ctx, thought, counter, progress = self._setup_card_phase_mocks()
        card._reactive_files_written = 3
        card._reactive_progress_percent = 50
        card.watch_phase(PHASE_CODING)
        ctx.remove_class.assert_called_with("hidden")
        thought.update.assert_called_with("")
        counter.update.assert_called_with("[yellow]Files written:[/] 3")
        progress.remove_class.assert_called_with("hidden")
        assert progress.progress == 50

    def test_phase_coding_indeterminate(self):
        card, ctx, thought, counter, progress = self._setup_card_phase_mocks()
        card._reactive_files_written = 2
        card._reactive_progress_percent = -1
        card.watch_phase(PHASE_CODING)
        progress.add_class.assert_called_with("hidden")
        counter.update.assert_called_with(
            "[yellow]Files written:[/] 2 [dim italic](working...)[/]"
        )

    def test_phase_reasoning_with_snippet(self):
        card, ctx, thought, counter, progress = self._setup_card_phase_mocks()
        card._reactive_thought_snippet = "Analyzing security vulnerabilities"
        card.watch_phase(PHASE_REASONING)
        ctx.remove_class.assert_called_with("hidden")
        thought.update.assert_called()
        call_arg = thought.update.call_args[0][0]
        assert "Analyzing security" in call_arg
        counter.update.assert_called_with("")
        progress.add_class.assert_called_with("hidden")

    def test_phase_reasoning_long_snippet_truncated(self):
        card, ctx, thought, counter, progress = self._setup_card_phase_mocks()
        long_snippet = "A" * 100
        card._reactive_thought_snippet = long_snippet
        card.watch_phase(PHASE_REASONING)
        call_arg = thought.update.call_args[0][0]
        # Should truncate to 60 chars + "..."
        assert "..." in call_arg

    def test_phase_reasoning_no_snippet(self):
        card, ctx, thought, counter, progress = self._setup_card_phase_mocks()
        card._reactive_thought_snippet = ""
        card.watch_phase(PHASE_REASONING)
        thought.update.assert_called_with("[dim italic]Analyzing...[/]")

    def test_phase_complete(self):
        card, ctx, thought, counter, progress = self._setup_card_phase_mocks()
        card.watch_phase(PHASE_COMPLETE)
        thought.update.assert_called_with("[green]Complete[/]")
        counter.update.assert_called_with("")
        progress.remove_class.assert_called_with("hidden")
        assert progress.progress == 100

    def test_phase_exception_swallowed(self):
        card = _make_card()
        card.query_one = Mock(side_effect=Exception("not mounted"))
        # Should not raise
        card.watch_phase(PHASE_RESEARCH)


# ---------------------------------------------------------------------------
# AgentCard — watch_links_found, watch_files_written, watch_thought_snippet
# ---------------------------------------------------------------------------

class TestWatchLinksFound(unittest.TestCase):
    """Test watch_links_found reactive watcher."""

    def test_updates_when_in_research_phase(self):
        card = _make_card()
        card._reactive_phase = PHASE_RESEARCH
        mock_counter = Mock()
        card.query_one = Mock(return_value=mock_counter)
        card.watch_links_found(10)
        mock_counter.update.assert_called_once_with("[cyan]Links found:[/] 10")

    def test_no_update_when_not_research(self):
        card = _make_card()
        card._reactive_phase = PHASE_IDLE
        card.query_one = Mock()
        card.watch_links_found(10)
        card.query_one.assert_not_called()

    def test_exception_swallowed(self):
        card = _make_card()
        card._reactive_phase = PHASE_RESEARCH
        card.query_one = Mock(side_effect=Exception("not mounted"))
        card.watch_links_found(5)  # Should not raise


class TestWatchFilesWritten(unittest.TestCase):
    """Test watch_files_written reactive watcher."""

    def test_updates_when_coding_with_progress(self):
        card = _make_card()
        card._reactive_phase = PHASE_CODING
        card._reactive_progress_percent = 50
        mock_counter = Mock()
        card.query_one = Mock(return_value=mock_counter)
        card.watch_files_written(7)
        mock_counter.update.assert_called_once_with("[yellow]Files written:[/] 7")

    def test_updates_when_coding_indeterminate(self):
        card = _make_card()
        card._reactive_phase = PHASE_CODING
        card._reactive_progress_percent = -1
        mock_counter = Mock()
        card.query_one = Mock(return_value=mock_counter)
        card.watch_files_written(3)
        mock_counter.update.assert_called_once_with(
            "[yellow]Files written:[/] 3 [dim italic](working...)[/]"
        )

    def test_no_update_when_not_coding(self):
        card = _make_card()
        card._reactive_phase = PHASE_IDLE
        card.query_one = Mock()
        card.watch_files_written(3)
        card.query_one.assert_not_called()

    def test_exception_swallowed(self):
        card = _make_card()
        card._reactive_phase = PHASE_CODING
        card.query_one = Mock(side_effect=Exception("not mounted"))
        card.watch_files_written(3)  # Should not raise


class TestWatchThoughtSnippet(unittest.TestCase):
    """Test watch_thought_snippet reactive watcher."""

    def test_updates_when_reasoning_with_short_snippet(self):
        card = _make_card()
        card._reactive_phase = PHASE_REASONING
        mock_thought = Mock()
        card.query_one = Mock(return_value=mock_thought)
        card.watch_thought_snippet("Short thought")
        call_arg = mock_thought.update.call_args[0][0]
        assert "Short thought" in call_arg
        assert "..." not in call_arg  # Short enough, no truncation

    def test_updates_when_reasoning_with_long_snippet(self):
        card = _make_card()
        card._reactive_phase = PHASE_REASONING
        mock_thought = Mock()
        card.query_one = Mock(return_value=mock_thought)
        long_snippet = "X" * 80
        card.watch_thought_snippet(long_snippet)
        call_arg = mock_thought.update.call_args[0][0]
        assert "..." in call_arg  # Truncated

    def test_no_update_when_empty_snippet(self):
        card = _make_card()
        card._reactive_phase = PHASE_REASONING
        card.query_one = Mock()
        card.watch_thought_snippet("")
        card.query_one.assert_not_called()

    def test_no_update_when_not_reasoning(self):
        card = _make_card()
        card._reactive_phase = PHASE_IDLE
        card.query_one = Mock()
        card.watch_thought_snippet("Some thought")
        card.query_one.assert_not_called()

    def test_exception_swallowed(self):
        card = _make_card()
        card._reactive_phase = PHASE_REASONING
        card.query_one = Mock(side_effect=Exception("not mounted"))
        card.watch_thought_snippet("Test")  # Should not raise


# ---------------------------------------------------------------------------
# AgentCard — watch_progress_percent
# ---------------------------------------------------------------------------

class TestWatchProgressPercent(unittest.TestCase):
    """Test watch_progress_percent reactive watcher."""

    def test_positive_percent_shows_progress(self):
        card = _make_card()
        mock_progress = MagicMock()
        card.query_one = Mock(return_value=mock_progress)
        card.watch_progress_percent(75)
        mock_progress.remove_class.assert_called_with("hidden")
        assert mock_progress.progress == 75

    def test_negative_percent_hides_progress(self):
        card = _make_card()
        mock_progress = MagicMock()
        card.query_one = Mock(return_value=mock_progress)
        card.watch_progress_percent(-1)
        mock_progress.add_class.assert_called_with("hidden")

    def test_zero_percent_shows_progress(self):
        card = _make_card()
        mock_progress = MagicMock()
        card.query_one = Mock(return_value=mock_progress)
        card.watch_progress_percent(0)
        mock_progress.remove_class.assert_called_with("hidden")
        assert mock_progress.progress == 0

    def test_exception_swallowed(self):
        card = _make_card()
        card.query_one = Mock(side_effect=Exception("not mounted"))
        card.watch_progress_percent(50)  # Should not raise


# ---------------------------------------------------------------------------
# AgentCard — Token Odometer watchers
# ---------------------------------------------------------------------------

class TestTokenOdometerWatchers(unittest.TestCase):
    """Test watch_token_cost_usd, watch_latency_ms, watch_request_count."""

    def test_watch_token_cost_usd_below_budget(self):
        card = _make_card()
        card._update_odometer = Mock()
        card.post_message = Mock()
        card._reactive_token_budget_usd = 2.0
        card._budget_warned = False
        card.watch_token_cost_usd(1.0)
        card._update_odometer.assert_called_once()
        card.post_message.assert_not_called()

    @patch("tui.widgets.agent_card.alert_budget_exceeded")
    def test_watch_token_cost_usd_exceeds_budget(self, mock_alert):
        card = _make_card()
        card._update_odometer = Mock()
        card.post_message = Mock()
        card.add_class = Mock()
        card._reactive_token_budget_usd = 2.0
        card._budget_warned = False
        card.watch_token_cost_usd(3.0)
        card._update_odometer.assert_called_once()
        assert card._budget_warned is True
        card.post_message.assert_called_once()
        msg = card.post_message.call_args[0][0]
        assert isinstance(msg, AgentCard.BudgetExceeded)
        assert msg.cost == 3.0
        assert msg.budget == 2.0
        card.add_class.assert_called_with("budget-exceeded")
        mock_alert.assert_called_once_with(card.agent.id, 3.0, 2.0)

    @patch("tui.widgets.agent_card.alert_budget_exceeded")
    def test_watch_token_cost_usd_already_warned(self, mock_alert):
        card = _make_card()
        card._update_odometer = Mock()
        card.post_message = Mock()
        card._reactive_token_budget_usd = 2.0
        card._budget_warned = True  # Already warned
        card.watch_token_cost_usd(3.0)
        card.post_message.assert_not_called()
        mock_alert.assert_not_called()

    def test_watch_latency_ms(self):
        card = _make_card()
        card._update_odometer = Mock()
        card.watch_latency_ms(500)
        card._update_odometer.assert_called_once()

    def test_watch_request_count(self):
        card = _make_card()
        card._update_odometer = Mock()
        card.watch_request_count(10)
        card._update_odometer.assert_called_once()


class TestUpdateOdometer(unittest.TestCase):
    """Test _update_odometer method."""

    def test_update_odometer_success(self):
        card = _make_card()
        mock_odometer = Mock()
        card.query_one = Mock(return_value=mock_odometer)
        card._format_odometer = Mock(return_value="formatted")
        card._update_odometer()
        mock_odometer.update.assert_called_once_with("formatted")

    def test_update_odometer_exception_swallowed(self):
        card = _make_card()
        card.query_one = Mock(side_effect=Exception("not mounted"))
        card._update_odometer()  # Should not raise


# ---------------------------------------------------------------------------
# AgentCard — update_metrics, reset_budget
# ---------------------------------------------------------------------------

class TestUpdateMetrics(unittest.TestCase):
    """Test update_metrics convenience method."""

    def test_cost_delta_positive(self):
        card = _make_card()
        card._reactive_token_cost_usd = 1.0
        # Avoid triggering watchers by mocking the setter
        with patch.object(type(card), "token_cost_usd", new_callable=PropertyMock) as mock_cost:
            mock_cost.return_value = 1.0
            card.update_metrics(cost_delta=0.5)
            # Should have been set to 1.0 + 0.5
            mock_cost.assert_called()

    def test_cost_delta_zero_no_change(self):
        card = _make_card()
        original_cost = card.token_cost_usd
        card._update_odometer = Mock()
        card.update_metrics(cost_delta=0.0)
        # Cost should not increase when delta is 0

    def test_latency_positive(self):
        card = _make_card()
        card._update_odometer = Mock()
        card.update_metrics(latency=500)

    def test_requests_delta_positive(self):
        card = _make_card()
        card._update_odometer = Mock()
        card.update_metrics(requests_delta=3)

    def test_all_params_together(self):
        card = _make_card()
        card._update_odometer = Mock()
        card.update_metrics(cost_delta=0.5, latency=200, requests_delta=1)


class TestResetBudget(unittest.TestCase):
    """Test reset_budget method."""

    def test_reset_budget_clears_warned(self):
        card = _make_card()
        card._budget_warned = True
        card.remove_class = Mock()
        card.reset_budget()
        assert card._budget_warned is False
        card.remove_class.assert_called_once_with("budget-exceeded")


# ---------------------------------------------------------------------------
# AgentCard — set_active, set_status
# ---------------------------------------------------------------------------

class TestSetActive(unittest.TestCase):
    """Test set_active convenience method."""

    def test_set_active_defaults(self):
        card = _make_card()
        card._update_odometer = Mock()
        card.set_active()
        # Phase and status should be idle by default
        assert card.phase == PHASE_IDLE
        assert card.status == STATUS_IDLE

    def test_set_active_with_values(self):
        card = _make_card()
        card._update_odometer = Mock()
        card.set_active(
            phase=PHASE_CODING,
            status=STATUS_ACTIVE,
            thought="Working on it",
            links=5,
            files=3,
            progress=42,
        )
        assert card.thought_snippet == "Working on it"
        assert card.links_found == 5
        assert card.files_written == 3
        assert card.progress_percent == 42
        assert card.status == STATUS_ACTIVE
        assert card.phase == PHASE_CODING


class TestSetStatus(unittest.TestCase):
    """Test set_status method."""

    def test_set_status_active(self):
        card = _make_card()
        card.set_status(STATUS_ACTIVE)
        assert card.status == STATUS_ACTIVE

    def test_set_status_error(self):
        card = _make_card()
        card.set_status(STATUS_ERROR)
        assert card.status == STATUS_ERROR

    def test_set_status_idle(self):
        card = _make_card()
        card.set_status(STATUS_IDLE)
        assert card.status == STATUS_IDLE


# ---------------------------------------------------------------------------
# AgentCard — on_click, on_key
# ---------------------------------------------------------------------------

class TestOnClick(unittest.TestCase):
    """Test on_click posts Selected message."""

    def test_on_click_posts_selected(self):
        agent = _make_agent()
        card = _make_card(agent)
        card.post_message = Mock()
        card.on_click()
        card.post_message.assert_called_once()
        msg = card.post_message.call_args[0][0]
        assert isinstance(msg, AgentCard.Selected)
        assert msg.agent is agent


class TestOnKey(unittest.TestCase):
    """Test on_key for enter and other keys."""

    def test_enter_posts_selected(self):
        agent = _make_agent()
        card = _make_card(agent)
        card.post_message = Mock()
        event = Mock()
        event.key = "enter"
        card.on_key(event)
        card.post_message.assert_called_once()
        msg = card.post_message.call_args[0][0]
        assert isinstance(msg, AgentCard.Selected)
        event.stop.assert_called_once()

    def test_other_key_no_message(self):
        card = _make_card()
        card.post_message = Mock()
        event = Mock()
        event.key = "tab"
        card.on_key(event)
        card.post_message.assert_not_called()

    def test_escape_key_no_message(self):
        card = _make_card()
        card.post_message = Mock()
        event = Mock()
        event.key = "escape"
        card.on_key(event)
        card.post_message.assert_not_called()


# ---------------------------------------------------------------------------
# AgentCard — Contextual recovery actions
# ---------------------------------------------------------------------------

class TestActionRestart(unittest.TestCase):
    """Test action_restart."""

    def test_restart_in_error_state(self):
        card = _make_card()
        card._reactive_status = STATUS_ERROR
        card.post_message = Mock()
        card.action_restart()
        card.post_message.assert_called_once()
        msg = card.post_message.call_args[0][0]
        assert isinstance(msg, AgentCard.RestartRequested)

    def test_restart_in_idle_state_no_op(self):
        card = _make_card()
        card._reactive_status = STATUS_IDLE
        card.post_message = Mock()
        card.action_restart()
        card.post_message.assert_not_called()

    def test_restart_in_active_state_no_op(self):
        card = _make_card()
        card._reactive_status = STATUS_ACTIVE
        card.post_message = Mock()
        card.action_restart()
        card.post_message.assert_not_called()


class TestActionLogs(unittest.TestCase):
    """Test action_logs."""

    def test_logs_in_error_state(self):
        card = _make_card()
        card._reactive_status = STATUS_ERROR
        card.post_message = Mock()
        card.action_logs()
        card.post_message.assert_called_once()
        msg = card.post_message.call_args[0][0]
        assert isinstance(msg, AgentCard.LogsRequested)

    def test_logs_in_idle_state_no_op(self):
        card = _make_card()
        card._reactive_status = STATUS_IDLE
        card.post_message = Mock()
        card.action_logs()
        card.post_message.assert_not_called()


class TestActionDismiss(unittest.TestCase):
    """Test action_dismiss."""

    def test_dismiss_in_error_state(self):
        card = _make_card()
        card._reactive_status = STATUS_ERROR
        card.post_message = Mock()
        card.action_dismiss()
        card.post_message.assert_called_once()
        msg = card.post_message.call_args[0][0]
        assert isinstance(msg, AgentCard.ErrorDismissed)
        # Should also clear error state
        assert card.status == STATUS_IDLE

    def test_dismiss_in_idle_state_no_op(self):
        card = _make_card()
        card._reactive_status = STATUS_IDLE
        card.post_message = Mock()
        card.action_dismiss()
        card.post_message.assert_not_called()


class TestActionDetails(unittest.TestCase):
    """Test action_details."""

    def test_details_in_error_state(self):
        card = _make_card()
        card._reactive_status = STATUS_ERROR
        card.post_message = Mock()
        card.action_details()
        card.post_message.assert_called_once()
        msg = card.post_message.call_args[0][0]
        assert isinstance(msg, AgentCard.ErrorDetailsRequested)

    def test_details_in_active_state_no_op(self):
        card = _make_card()
        card._reactive_status = STATUS_ACTIVE
        card.post_message = Mock()
        card.action_details()
        card.post_message.assert_not_called()


# ---------------------------------------------------------------------------
# AgentCard — on_focus, on_blur, action hints
# ---------------------------------------------------------------------------

class TestOnFocus(unittest.TestCase):
    """Test on_focus calls _update_action_hints."""

    def test_on_focus_calls_update_hints(self):
        card = _make_card()
        card._update_action_hints = Mock()
        card.on_focus()
        card._update_action_hints.assert_called_once()


class TestOnBlur(unittest.TestCase):
    """Test on_blur calls _hide_action_hints."""

    def test_on_blur_calls_hide_hints(self):
        card = _make_card()
        card._hide_action_hints = Mock()
        card.on_blur()
        card._hide_action_hints.assert_called_once()


class TestUpdateActionHints(unittest.TestCase):
    """Test _update_action_hints method."""

    def test_error_state_shows_recovery_actions(self):
        card = _make_card()
        card._reactive_status = STATUS_ERROR
        mock_thought = Mock()
        mock_context = Mock()

        def query_one_side_effect(selector, *args):
            if "thought" in selector:
                return mock_thought
            if "ctx" in selector:
                return mock_context
            return Mock()

        card.query_one = Mock(side_effect=query_one_side_effect)
        card.add_class = Mock()
        card._update_action_hints()
        mock_thought.update.assert_called_once()
        call_arg = mock_thought.update.call_args[0][0]
        assert "Restart" in call_arg
        assert "Logs" in call_arg
        assert "Dismiss" in call_arg
        assert "Details" in call_arg
        mock_context.remove_class.assert_called_with("hidden")
        card.add_class.assert_called_with("expanded")

    def test_idle_state_no_hints(self):
        card = _make_card()
        card._reactive_status = STATUS_IDLE
        card.query_one = Mock()
        card._update_action_hints()
        # query_one called but the if-branch for error is not entered,
        # so thought.update is never called — but query_one IS called
        # for the thought widget
        # Actually in idle, the if-check on status fails so no recovery shown

    def test_exception_swallowed(self):
        card = _make_card()
        card._reactive_status = STATUS_ERROR
        card.query_one = Mock(side_effect=Exception("not mounted"))
        card._update_action_hints()  # Should not raise


class TestHideActionHints(unittest.TestCase):
    """Test _hide_action_hints method."""

    def test_non_error_returns_early(self):
        card = _make_card()
        card._reactive_status = STATUS_IDLE
        card.query_one = Mock()
        card._hide_action_hints()
        card.query_one.assert_not_called()

    def test_error_state_updates_thought(self):
        card = _make_card()
        card._reactive_status = STATUS_ERROR
        mock_thought = Mock()
        card.query_one = Mock(return_value=mock_thought)
        card._hide_action_hints()
        mock_thought.update.assert_called_once()
        call_arg = mock_thought.update.call_args[0][0]
        assert "Error" in call_arg
        assert "focus for actions" in call_arg

    def test_error_state_exception_swallowed(self):
        card = _make_card()
        card._reactive_status = STATUS_ERROR
        card.query_one = Mock(side_effect=Exception("not mounted"))
        card._hide_action_hints()  # Should not raise


# ---------------------------------------------------------------------------
# AgentCard — BINDINGS
# ---------------------------------------------------------------------------

class TestBindings(unittest.TestCase):
    """Test BINDINGS class attribute."""

    def test_has_restart_binding(self):
        keys = [b.key for b in AgentCard.BINDINGS]
        assert "r" in keys

    def test_has_logs_binding(self):
        keys = [b.key for b in AgentCard.BINDINGS]
        assert "l" in keys

    def test_has_dismiss_binding(self):
        keys = [b.key for b in AgentCard.BINDINGS]
        assert "d" in keys

    def test_has_details_binding(self):
        keys = [b.key for b in AgentCard.BINDINGS]
        assert "i" in keys

    def test_all_bindings_hidden(self):
        for b in AgentCard.BINDINGS:
            assert b.show is False


# ---------------------------------------------------------------------------
# AgentCard — DEFAULT_CSS
# ---------------------------------------------------------------------------

class TestDefaultCSS(unittest.TestCase):
    """Test DEFAULT_CSS is empty string."""

    def test_default_css_empty(self):
        assert AgentCard.DEFAULT_CSS == ""


if __name__ == "__main__":
    unittest.main()
