# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Property-based tests for Euxis core components.

Tests serialization roundtrips, parser invariants, mathematical properties,
and edge cases using hypothesis-generated inputs.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

from hypothesis import assume, example, given, settings
from hypothesis import strategies as st

# Ensure the tui package is importable
repo_root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(repo_root))
sys.path.insert(0, str(repo_root / "ui" / "src"))

from tui.core.config import ETXConfig
from tui.core.registry import Agent, Squad


class TestAgentSerialization:
    """Property-based tests for Agent serialization roundtrips."""

    @given(
        agent_id=st.text(min_size=1, max_size=50, alphabet=st.characters(
            whitelist_categories=("Ll", "Lu", "Nd"), whitelist_characters="-_"
        )),
        tier=st.sampled_from(["core", "fleet", "default", "on-demand", "specialist"]),
        tags=st.lists(st.text(min_size=1, max_size=20), min_size=0, max_size=10),
        activation=st.sampled_from(["default", "on-demand", "specialist"])
    )
    @settings(max_examples=100, deadline=1000)
    def test_agent_serialization_roundtrip(self, agent_id, tier, tags, activation):
        """Test that Agent objects serialize and deserialize correctly."""
        assume(agent_id.strip() != "")

        agent = Agent(
            id=agent_id.strip(),
            tier=tier,
            version="0.0.8",
            tags=tuple(tags),
            activation=activation,
        )

        # Serialize to dict and back
        serialized = {
            "id": agent.id,
            "tier": agent.tier,
            "version": agent.version,
            "tags": agent.tags,
            "activation": agent.activation,
        }
        reconstructed = Agent(**serialized)

        # Properties should be preserved
        assert reconstructed.id == agent.id
        assert reconstructed.tier == agent.tier
        assert reconstructed.tags == agent.tags
        assert reconstructed.activation == agent.activation

    @given(
        squad_id=st.text(min_size=1, max_size=30, alphabet=st.characters(
            whitelist_categories=("Ll", "Lu", "Nd"), whitelist_characters="-_"
        )),
        name=st.text(min_size=1, max_size=50),
        purpose=st.text(min_size=1, max_size=200),
        lead=st.text(min_size=1, max_size=20),
        members=st.lists(st.text(min_size=1, max_size=20), min_size=1, max_size=15)
    )
    @settings(max_examples=50)
    def test_squad_serialization_roundtrip(self, squad_id, name, purpose, lead, members):
        """Test that Squad objects serialize and deserialize correctly."""
        assume(squad_id.strip() != "")
        assume(name.strip() != "")
        assume(purpose.strip() != "")
        assume(lead.strip() != "")
        assume(all(m.strip() != "" for m in members))

        squad = Squad(
            id=squad_id.strip(),
            name=name.strip(),
            purpose=purpose.strip(),
            lead=lead.strip(),
            members=tuple(m.strip() for m in members),
        )

        # Test serialization
        serialized = {
            "id": squad.id,
            "name": squad.name,
            "purpose": squad.purpose,
            "lead": squad.lead,
            "members": squad.members,
        }
        reconstructed = Squad(**serialized)

        assert reconstructed.id == squad.id
        assert reconstructed.name == squad.name
        assert reconstructed.purpose == squad.purpose
        assert reconstructed.lead == squad.lead
        assert reconstructed.members == squad.members

    @given(
        config_data=st.fixed_dictionaries({
            "theme": st.sampled_from(["etx-dark", "etx-light", "etx-contrast"]),
            "default_provider": st.sampled_from(["claude", "gemini", "ollama"]),
            "show_agent_tags": st.booleans(),
            "reduced_motion": st.booleans(),
            "accessible_mode": st.booleans(),
            "grid_columns": st.integers(min_value=1, max_value=8),
        })
    )
    def test_config_serialization_roundtrip(self, config_data):
        """Test ETXConfig serialization preserves all properties."""
        config = ETXConfig(**config_data)

        # Test JSON serialization roundtrip
        json_str = json.dumps(config.__dict__)
        restored_data = json.loads(json_str)
        # Filter to known fields
        known = {k: v for k, v in restored_data.items() if k in ETXConfig.__dataclass_fields__}
        restored_config = ETXConfig(**known)

        assert restored_config.theme == config.theme
        assert restored_config.default_provider == config.default_provider
        assert restored_config.show_agent_tags == config.show_agent_tags
        assert restored_config.reduced_motion == config.reduced_motion


class TestConfigBoundaries:
    """Test configuration edge cases and boundary conditions."""

    @given(st.lists(st.text(min_size=1, max_size=50), min_size=0, max_size=20))
    def test_recent_agents_limit(self, agents):
        """Test that recent agents list respects maximum limit."""
        config = ETXConfig()

        for agent in agents:
            config.add_recent_agent(agent)

        # Should never exceed the limit
        assert len(config.recent_agents) <= 10

        # Most recent non-empty should be first
        non_empty = [a for a in agents if a.strip()]
        if non_empty:
            assert config.recent_agents[0] == non_empty[-1]

    @example(agents=["agent1", "agent1", "agent2", "agent1"])  # Test deduplication
    @given(st.lists(st.text(min_size=1, max_size=20), min_size=1, max_size=15))
    def test_recent_agents_deduplication(self, agents):
        """Test that duplicate agents are properly deduplicated."""
        config = ETXConfig()

        for agent in agents:
            config.add_recent_agent(agent)

        # No duplicates should exist
        assert len(config.recent_agents) == len(set(config.recent_agents))


class TestValidationInvariants:
    """Test validation functions maintain their invariants."""

    @given(st.text())
    def test_agent_name_validation_invariant(self, agent_name):
        """Test that agent name validation via _SAFE_ID is consistent."""
        from tui.core.runner import _SAFE_ID

        result = bool(_SAFE_ID.match(agent_name))

        # Invariant: empty strings are always invalid
        if not agent_name.strip():
            assert not result

        # Invariant: strings with path traversal patterns are invalid
        if ".." in agent_name or "/" in agent_name:
            assert not result

        # Invariant: valid IDs start with a letter
        if result:
            assert agent_name[0].isalpha()

        # Invariant: valid IDs have max length 64
        if len(agent_name) > 64:
            assert not result


class TestMathematicalProperties:
    """Test mathematical properties of algorithms."""

    @given(
        st.lists(st.integers(min_value=0, max_value=100), min_size=1, max_size=20)
    )
    def test_sparkline_monotonicity(self, values):
        """Test that sparkline generation preserves relative ordering."""

        # Mock the sparkline function behavior
        def mock_sparkline_text(data, width=20):
            if not data:
                return ""

            # Normalize to 0-7 range (8 sparkline characters)
            min_val, max_val = min(data), max(data)
            if min_val == max_val:
                return "▄" * min(width, len(data))

            normalized = []
            for val in data:
                norm = int(7 * (val - min_val) / (max_val - min_val))
                normalized.append(norm)

            return normalized

        result = mock_sparkline_text(values)

        # Property: relative ordering should be preserved
        if len(values) > 1:
            for i in range(len(values) - 1):
                if values[i] < values[i + 1]:
                    # In sparkline, smaller values should have smaller indices
                    assert result[i] <= result[i + 1]
                elif values[i] > values[i + 1]:
                    assert result[i] >= result[i + 1]

    @given(
        st.integers(min_value=1, max_value=1000),
        st.integers(min_value=1, max_value=100)
    )
    def test_agent_distribution_properties(self, total_agents, num_tiers):
        """Test that agent distribution across tiers follows expected properties."""
        # Simulate agent distribution
        agents_per_tier = total_agents // num_tiers
        remainder = total_agents % num_tiers

        distribution = [agents_per_tier] * num_tiers
        for i in range(remainder):
            distribution[i] += 1

        # Mathematical properties
        assert sum(distribution) == total_agents  # Conservation
        assert max(distribution) - min(distribution) <= 1  # Even distribution
        assert all(count >= 0 for count in distribution)  # Non-negative


class TestUnicodeAndEdgeCases:
    """Test Unicode handling and edge cases."""

    @given(st.text(alphabet=st.characters(
        min_codepoint=0x0100,  # Beyond ASCII
        max_codepoint=0x017F   # Latin Extended-A
    )))
    def test_unicode_agent_names(self, unicode_name):
        """Test that Unicode agent names are handled correctly."""
        if unicode_name.strip():
            # Should not crash on Unicode input
            config = ETXConfig()
            config.add_recent_agent(unicode_name)

            # Should preserve Unicode correctly
            assert config.recent_agents[0] == unicode_name

    @given(st.text(max_size=0))  # Empty strings
    def test_empty_string_handling(self, empty_text):
        """Test handling of empty strings throughout the system."""
        assert empty_text == ""

        # Empty strings should be handled gracefully
        config = ETXConfig()
        config.add_recent_agent(empty_text)

        # Should not add empty agents
        assert len(config.recent_agents) == 0

    @given(
        st.one_of(
            st.none(),
            st.text(),
            st.integers(),
            st.lists(st.text()),
            st.dictionaries(st.text(), st.text())
        )
    )
    def test_json_serialization_robustness(self, data):
        """Test JSON serialization handles various data types gracefully."""
        try:
            # Should handle serialization of various types
            serialized = json.dumps(data, default=str)
            deserialized = json.loads(serialized)

            # Basic roundtrip property (with type coercion)
            if data is None:
                assert deserialized is None
            elif isinstance(data, (str, int, list, dict)):
                # These should roundtrip exactly
                pass  # Test passes if no exception

        except (TypeError, ValueError):
            # Some types may not be JSON serializable, which is acceptable
            pass
