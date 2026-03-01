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
        "version": "v0.0.3",
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