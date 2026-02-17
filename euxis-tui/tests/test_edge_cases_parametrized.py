# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Parametrized edge case tests for comprehensive boundary testing.

Tests empty inputs, null/None values, Unicode strings, boundary values,
and concurrent access patterns with deterministic fixtures.
"""

from __future__ import annotations

import json
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from unittest.mock import mock_open, patch

import pytest

# Ensure the tui package is importable
repo_root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(repo_root))
sys.path.insert(0, str(repo_root / "ui" / "src"))


class TestEmptyInputs:
    """Test system behavior with empty inputs."""

    @pytest.mark.parametrize("empty_input", [
        "",
        None,
        [],
        {},
        "   ",  # Whitespace only
        "\n\t\r",  # Various whitespace
    ])
    def test_config_add_recent_agent_empty_inputs(self, empty_input):
        """Test ETXConfig handles empty agent inputs gracefully."""
        from tui.core.config import ETXConfig

        config = ETXConfig()
        initial_count = len(config.recent_agents)

        config.add_recent_agent(empty_input)

        # Should not add empty/None/non-string values
        assert len(config.recent_agents) == initial_count

    @pytest.mark.parametrize("empty_data", [
        "",
        "{}",
        "[]",
        '{"agents": []}',
        '{"squads": []}',
        '{"version": ""}',
    ])
    def test_registry_empty_json_data(self, empty_data):
        """Test FleetRegistry handles empty JSON data."""
        from tui.core.registry import FleetRegistry

        with (
            patch("pathlib.Path.read_text", return_value=empty_data),
            patch("pathlib.Path.exists", return_value=True),
        ):
            try:
                registry = FleetRegistry.load()
                # Should handle gracefully
                assert registry is not None
            except (json.JSONDecodeError, KeyError, AttributeError, TypeError):
                # Some empty data might cause parsing errors, which is acceptable
                pass

    @pytest.mark.parametrize("empty_sparkline_data", [
        [],
        [None],
        [None, None, None],
        "",
    ])
    def test_sparkline_empty_data_handling(self, empty_sparkline_data):
        """Test sparkline handles empty data gracefully."""
        from tui.widgets.sparkline import sparkline_text

        result = sparkline_text(empty_sparkline_data)

        # All these cases should produce an empty string
        assert result == ""


class TestUnicodeAndSpecialCharacters:
    """Test Unicode and special character handling."""

    @pytest.mark.parametrize("unicode_input", [
        "café",  # Latin with accents
        "测试",  # Chinese characters
        "ñoño",  # Spanish characters
        "Здравствуй",  # Cyrillic
        "agent_with_\u200b_zero_width_space",  # Zero-width characters
        "line1\nline2\ttab",  # Control characters
    ])
    def test_config_unicode_agent_names(self, unicode_input):
        """Test ETXConfig handles Unicode agent names."""
        from tui.core.config import ETXConfig

        config = ETXConfig()
        config.add_recent_agent(unicode_input)

        # Should preserve Unicode correctly
        if unicode_input.strip():
            assert config.recent_agents[0] == unicode_input

    @pytest.mark.parametrize("unicode_data", [
        {"id": "tëst", "tier": "core"},
        {"id": "test-unicode", "tier": "core"},
        {"id": "Agent1", "tier": "specialist"},
    ])
    def test_agent_unicode_properties(self, unicode_data):
        """Test Agent objects handle Unicode properties."""
        from tui.core.registry import Agent

        # Add required fields
        unicode_data.update({
            "tags": (),
            "activation": "default",
            "version": "0.0.8"
        })

        agent = Agent(**unicode_data)
        assert agent.id == unicode_data["id"]

    @pytest.mark.parametrize("special_chars", [
        "agent-with-dashes",
        "agent_with_underscores",
        "agent.with.dots",
        "agent with spaces",
        "agent@with@symbols",
        "agent#with$special%chars",
        "UPPERCASE_AGENT",
        "MiXeD_CaSe_AgEnT",
    ])
    def test_special_character_handling(self, special_chars):
        """Test handling of special characters in agent names."""
        from tui.core.config import ETXConfig

        config = ETXConfig()
        config.add_recent_agent(special_chars)

        # Should handle special characters
        if special_chars.strip():
            assert config.recent_agents[0] == special_chars


class TestBoundaryValues:
    """Test boundary values and edge cases."""

    @pytest.mark.parametrize("boundary_size", [
        0,
        1,
        10,  # Default limit
        11,  # Just over limit
        100,  # Large number
        1000,  # Very large number
    ])
    def test_config_recent_agents_boundary_sizes(self, boundary_size):
        """Test recent agents list at boundary sizes."""
        from tui.core.config import ETXConfig

        config = ETXConfig()

        # Add agents up to boundary size
        for i in range(boundary_size):
            config.add_recent_agent(f"agent_{i}")

        # Should never exceed maximum limit
        assert len(config.recent_agents) <= 10

        # If we added more than the limit, check truncation
        if boundary_size > 10:
            assert len(config.recent_agents) == 10
            # Most recent should be at the front
            assert config.recent_agents[0] == f"agent_{boundary_size - 1}"

    @pytest.mark.parametrize("sparkline_size", [
        1,
        2,
        10,
        50,
        100,
        1000,
    ])
    def test_sparkline_boundary_sizes(self, sparkline_size):
        """Test sparkline with various data sizes."""
        from tui.widgets.sparkline import sparkline_text

        data = list(range(sparkline_size))
        result = sparkline_text(data)

        assert isinstance(result, str)
        if sparkline_size > 0:
            assert len(result) > 0

    @pytest.mark.parametrize("extreme_values", [
        [0, 0, 0],  # All zeros
        [1, 1, 1],  # All same
        [-1000000, 1000000],  # Extreme range
        [1e-10, 1e10],  # Very small and large
        [0.00001, 0.00002, 0.00003],  # Very small differences
    ])
    def test_sparkline_extreme_values(self, extreme_values):
        """Test sparkline with extreme values."""
        from tui.widgets.sparkline import sparkline_text

        result = sparkline_text(extreme_values)
        assert isinstance(result, str)

    @pytest.mark.parametrize("text_length", [
        1,
        255,  # Common string length limit
        256,
        1000,
        10000,
        65535,  # 16-bit limit
    ])
    def test_long_text_handling(self, text_length):
        """Test handling of very long text inputs."""
        from tui.core.config import ETXConfig

        # Create long text
        long_text = "a" * text_length

        config = ETXConfig()
        config.add_recent_agent(long_text)

        # Should handle long text (though may be truncated)
        assert len(config.recent_agents) <= 10
        if text_length > 0:
            assert len(config.recent_agents) == 1


class TestConcurrentAccess:
    """Test concurrent access patterns and race conditions."""

    def test_config_concurrent_modifications(self):
        """Test concurrent modifications to ETXConfig."""
        from tui.core.config import ETXConfig

        config = ETXConfig()
        results = []
        errors = []

        def add_agents(thread_id):
            try:
                for i in range(10):
                    agent_name = f"agent_{thread_id}_{i}"
                    config.add_recent_agent(agent_name)
                    time.sleep(0.001)  # Small delay to encourage race conditions
                results.append(thread_id)
            except Exception as exc:  # noqa: BLE001
                errors.append((thread_id, exc))

        # Run concurrent modifications
        with ThreadPoolExecutor(max_workers=5) as executor:
            futures = [executor.submit(add_agents, i) for i in range(5)]
            for future in futures:
                future.result(timeout=5)

        # Should complete without errors
        assert len(errors) == 0
        assert len(results) == 5

        # Final state should be consistent
        assert len(config.recent_agents) <= 10
        assert len(set(config.recent_agents)) == len(config.recent_agents)

    def test_registry_concurrent_access(self):
        """Test concurrent access to FleetRegistry."""
        from tui.core.registry import FleetRegistry

        registry_data = {
            "protocol_version": "0.0.8",
            "agents": (
                [
                    {"id": "core_agent_0", "tier": "core", "tags": [], "activation": "core"},
                ]
                + [
                    {"id": f"agent_{i}", "tier": "fleet", "tags": [], "activation": "default"}
                    for i in range(50)
                ]
            ),
        }

        squads_data = {
            "squads": [],
            "combos": [],
        }

        errors = []

        def access_registry(thread_id):
            try:
                # Use from_dicts to avoid file I/O mocking issues
                registry = FleetRegistry.from_dicts(
                    agents_data=registry_data,
                    squads_data=squads_data,
                )
                agents = registry.agents
                core_agents = registry.core_agents
                agent = registry.get_agent("agent_0")
                assert len(agents) == 51
                assert len(core_agents) > 0
                assert agent.id == "agent_0"
            except Exception as exc:  # noqa: BLE001
                errors.append((thread_id, exc))

        # Run concurrent access
        with ThreadPoolExecutor(max_workers=10) as executor:
            futures = [executor.submit(access_registry, i) for i in range(10)]
            for future in futures:
                future.result(timeout=5)

        # Should complete without errors
        assert len(errors) == 0


class TestNoneAndNullValues:
    """Test handling of None and null values throughout the system."""

    @pytest.mark.parametrize("none_field", [
        {"id": None, "tier": "core"},
        {"id": "test", "tier": None},
        {"id": "test", "tier": "core", "tags": None},
        {"id": "test", "tier": "core", "activation": None},
    ])
    def test_agent_none_fields(self, none_field):
        """Test Agent creation with None fields."""
        from tui.core.registry import Agent

        # Add required fields
        none_field.setdefault("tags", ())
        none_field.setdefault("activation", "default")
        none_field.setdefault("version", "0.0.8")

        try:
            agent = Agent(**none_field)
            assert agent is not None
        except (TypeError, ValueError):
            pass

    @pytest.mark.parametrize("null_json", [
        '{"id": null}',
        '{"agents": null}',
        '{"squads": null}',
    ])
    def test_registry_null_json_values(self, null_json):
        """Test registry with JSON null values."""
        from tui.core.registry import FleetRegistry

        with (
            patch("pathlib.Path.read_text", return_value=null_json),
            patch("pathlib.Path.exists", return_value=True),
        ):
            try:
                registry = FleetRegistry.load()
                assert registry is not None
            except (json.JSONDecodeError, KeyError, TypeError):
                pass


class TestErrorConditions:
    """Test various error conditions and edge cases."""

    def test_file_not_found_handling(self):
        """Test handling when registry files don't exist."""
        from tui.core.registry import FleetRegistry

        with patch("pathlib.Path.exists", return_value=False):
            try:
                registry = FleetRegistry.load()
                assert registry is not None
            except FileNotFoundError:
                pass

    def test_corrupted_json_handling(self):
        """Test handling of corrupted JSON files."""
        from tui.core.registry import FleetRegistry

        corrupted_json_data = [
            '{"invalid": json}',
            '{"unclosed": "value"',
            '{invalid_key: "value"}',
            '{"trailing": "comma",}',
            "not json at all",
        ]

        for corrupted_data in corrupted_json_data:
            with (
                patch("pathlib.Path.read_text", return_value=corrupted_data),
                patch("pathlib.Path.exists", return_value=True),
            ):
                try:
                    registry = FleetRegistry.load()
                    assert registry is not None
                except (json.JSONDecodeError, UnicodeDecodeError):
                    pass

    def test_permission_errors(self):
        """Test handling of file permission errors."""
        from tui.core.registry import FleetRegistry

        with (
            patch("pathlib.Path.read_text", side_effect=PermissionError("Access denied")),
            patch("pathlib.Path.exists", return_value=True),
        ):
            try:
                registry = FleetRegistry.load()
                assert registry is not None
            except PermissionError:
                pass

    @pytest.mark.parametrize("malformed_data", [
        {"agents": "not a list"},
        {"agents": [{"missing_id": "value"}]},
        {"agents": [{"id": "test", "missing_required": "field"}]},
        {"version": 123},
        {"invalid_root_key": "value"},
    ])
    def test_malformed_registry_data(self, malformed_data):
        """Test handling of malformed registry data."""
        from tui.core.registry import FleetRegistry

        with (
            patch("pathlib.Path.read_text", return_value=json.dumps(malformed_data)),
            patch("pathlib.Path.exists", return_value=True),
        ):
            try:
                registry = FleetRegistry.load()
                assert registry is not None
            except (KeyError, TypeError, ValueError):
                pass


class TestPerformanceBoundaries:
    """Test performance at boundary conditions."""

    def test_large_dataset_handling(self):
        """Test handling of large datasets."""
        from tui.core.registry import FleetRegistry

        # Create large dataset
        large_dataset = {
            "protocol_version": "0.0.8",
            "agents": [
                {
                    "id": f"agent_{i}",
                    "tier": "fleet",
                    "tags": [f"tag_{j}" for j in range(10)],
                    "activation": "default"
                }
                for i in range(1000)
            ],
        }

        start_time = time.time()
        registry = FleetRegistry.from_dicts(agents_data=large_dataset)
        load_time = time.time() - start_time

        # Should load within reasonable time (5 seconds)
        assert load_time < 5.0
        assert len(registry.agents) == 1000

    def test_large_sparkline_data(self):
        """Test sparkline with very large datasets."""
        from tui.widgets.sparkline import sparkline_text

        # Large dataset
        large_data = list(range(10000))

        start_time = time.time()
        result = sparkline_text(large_data, width=100)
        process_time = time.time() - start_time

        # Should process within reasonable time (1 second)
        assert process_time < 1.0
        assert isinstance(result, str)
        assert len(result) <= 100
