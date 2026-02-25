# SPDX-License-Identifier: AGPL-3.0-or-later
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

class TestStatusConstants(unittest.TestCase):
    """Test STATUS_* string constants."""

    def test_status_idle(self):
        assert STATUS_IDLE == "idle"

    def test_status_active(self):
        assert STATUS_ACTIVE == "active"

    def test_status_error(self):
        assert STATUS_ERROR == "error"

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