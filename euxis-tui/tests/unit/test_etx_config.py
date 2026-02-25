# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Comprehensive unit tests for ETXConfig.

Property-based tests for configuration management and recent item handling.
"""

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import pytest
from hypothesis import given
from hypothesis import strategies as st

from tui.core.config import ETXConfig


class TestETXConfig(unittest.TestCase):
    """Unit tests for ETXConfig configuration management."""

    def test_default_initialization(self):
        """Test ETXConfig initializes with default values."""
        config = ETXConfig()
        assert config.theme == "etx-liquid-glass"
        assert config.default_provider == "claude"
        assert config.show_agent_tags
        assert not config.reduced_motion
        assert not config.accessible_mode
        assert config.locale == "en"
        assert config.grid_columns == 4
        assert config.recent_agents == []
        assert config.recent_commands == []
        assert config.MAX_RECENT == 10

    def test_custom_initialization(self):
        """Test ETXConfig initialization with custom values."""
        config = ETXConfig(
            theme="etx-light",
            default_provider="openai",
            show_agent_tags=False,
            reduced_motion=True,
            accessible_mode=True,
            grid_columns=6,
            recent_agents=["architect", "debugger"],
            recent_commands=["help", "status"]
        )
        assert config.theme == "etx-light"
        assert config.default_provider == "openai"
        assert not config.show_agent_tags
        assert config.reduced_motion
        assert config.accessible_mode
        assert config.grid_columns == 6
        assert config.recent_agents == ["architect", "debugger"]
        assert config.recent_commands == ["help", "status"]

    def test_add_recent_agent_new(self):
        """Test adding a new agent to recent list."""
        config = ETXConfig()
        config.add_recent_agent("architect")
        assert config.recent_agents == ["architect"]

    def test_add_recent_agent_existing(self):
        """Test adding existing agent moves it to front."""
        config = ETXConfig(recent_agents=["debugger", "tester", "architect"])
        config.add_recent_agent("tester")
        assert config.recent_agents == ["tester", "debugger", "architect"]

    def test_add_recent_agent_max_limit(self):
        """Test recent agents list respects MAX_RECENT limit."""
        config = ETXConfig()
        # Add 12 agents (more than MAX_RECENT=10)
        for i in range(12):
            config.add_recent_agent(f"agent-{i}")

        assert len(config.recent_agents) == 10
        # Most recent should be first
        assert config.recent_agents[0] == "agent-11"
        # Oldest should be agent-2 (agent-0 and agent-1 got pushed out)
        assert config.recent_agents[-1] == "agent-2"

    def test_add_recent_command_new(self):
        """Test adding a new command to recent list."""
        config = ETXConfig()
        config.add_recent_command("help")
        assert config.recent_commands == ["help"]

    def test_add_recent_command_existing(self):
        """Test adding existing command moves it to front."""
        config = ETXConfig(recent_commands=["status", "version", "help"])
        config.add_recent_command("version")
        assert config.recent_commands == ["version", "status", "help"]

    def test_add_recent_command_max_limit(self):
        """Test recent commands list respects MAX_RECENT limit."""
        config = ETXConfig()
        # Add 12 commands (more than MAX_RECENT=10)
        for i in range(12):
            config.add_recent_command(f"cmd-{i}")

        assert len(config.recent_commands) == 10
        # Most recent should be first
        assert config.recent_commands[0] == "cmd-11"
        # Oldest should be cmd-2
        assert config.recent_commands[-1] == "cmd-2"

    def test_load_nonexistent_file(self):
        """Test load returns default config when file doesn't exist."""
        with patch("tui.core.config.CONFIG_PATH") as mock_path:
            mock_path.exists.return_value = False
            config = ETXConfig.load()

        assert config.theme == "etx-liquid-glass"
        assert config.default_provider == "claude"
        assert config.recent_agents == []

    def test_load_existing_file(self):
        """Test load reads configuration from existing file."""
        config_data = {
            "theme": "custom-theme",
            "default_provider": "custom-provider",
            "show_agent_tags": False,
            "recent_agents": ["test-agent"]
        }

        mock_file_content = json.dumps(config_data)
        with patch("tui.core.config.CONFIG_PATH") as mock_path:
            mock_path.exists.return_value = True
            mock_path.read_text.return_value = mock_file_content

            config = ETXConfig.load()

        assert config.theme == "custom-theme"
        assert config.default_provider == "custom-provider"
        assert not config.show_agent_tags
        assert config.recent_agents == ["test-agent"]

    def test_load_filters_unknown_fields(self):
        """Test load ignores unknown fields in config file."""
        config_data = {
            "theme": "test-theme",
            "unknown_field": "should-be-ignored",
            "another_unknown": 42
        }

        mock_file_content = json.dumps(config_data)
        with patch("tui.core.config.CONFIG_PATH") as mock_path:
            mock_path.exists.return_value = True
            mock_path.read_text.return_value = mock_file_content

            config = ETXConfig.load()

        assert config.theme == "test-theme"
        assert not hasattr(config, "unknown_field")
        assert not hasattr(config, "another_unknown")

    def test_save_creates_directory(self):
        """Test save creates parent directory if it doesn't exist."""
        config = ETXConfig(theme="test-theme")

        with patch("tui.core.config.CONFIG_PATH") as mock_path:
            mock_path.parent.mkdir = unittest.mock.Mock()
            mock_path.write_text = unittest.mock.Mock()

            config.save()

        mock_path.parent.mkdir.assert_called_once_with(parents=True, exist_ok=True)

    def test_save_writes_json(self):
        """Test save writes configuration as JSON."""
        config = ETXConfig(theme="test-theme", recent_agents=["test-agent"])

        with patch("tui.core.config.CONFIG_PATH") as mock_path:
            mock_path.parent.mkdir = unittest.mock.Mock()
            mock_path.write_text = unittest.mock.Mock()

            config.save()

        # Verify JSON was written
        written_content = mock_path.write_text.call_args[0][0]
        written_data = json.loads(written_content.rstrip("\n"))

        assert written_data["theme"] == "test-theme"
        assert written_data["recent_agents"] == ["test-agent"]

    def test_load_invalid_json(self):
        """Test load handles invalid JSON gracefully."""
        with patch("tui.core.config.CONFIG_PATH") as mock_path:
            mock_path.exists.return_value = True
            mock_path.read_text.return_value = "invalid json {"

            with pytest.raises(json.JSONDecodeError):
                ETXConfig.load()


class TestETXConfigPropertyBased(unittest.TestCase):
    """Property-based tests for ETXConfig robustness."""

    @given(st.text(min_size=1, max_size=50))
    def test_theme_assignment(self, theme_value):
        """Property: Config accepts arbitrary theme strings."""
        config = ETXConfig(theme=theme_value)
        assert config.theme == theme_value

    @given(st.text(min_size=1, max_size=50))
    def test_provider_assignment(self, provider_value):
        """Property: Config accepts arbitrary provider strings."""
        config = ETXConfig(default_provider=provider_value)
        assert config.default_provider == provider_value

    @given(st.integers(min_value=1, max_value=20))
    def test_grid_columns_assignment(self, columns):
        """Property: Config accepts arbitrary positive integer columns."""
        config = ETXConfig(grid_columns=columns)
        assert config.grid_columns == columns

    @given(st.lists(
        st.text(
            min_size=1, max_size=20,
            alphabet=st.characters(whitelist_categories=["Lu", "Ll", "Nd"]),
        ),
        min_size=0, max_size=50,
    ))
    def test_recent_agents_robustness(self, agent_list):
        """Property: Recent agents handling is robust for arbitrary agent lists."""
        config = ETXConfig()

        for agent in agent_list:
            config.add_recent_agent(agent)

        # Should never exceed MAX_RECENT
        assert len(config.recent_agents) <= config.MAX_RECENT

        # If any agents were added, most recent should be the last one added
        if agent_list:
            assert config.recent_agents[0] == agent_list[-1]

        # No duplicates should exist
        assert len(config.recent_agents) == len(set(config.recent_agents))

    @given(st.lists(
        st.text(
            min_size=1, max_size=50,
            alphabet=st.characters(whitelist_categories=["Lu", "Ll", "Nd", "Pc"]),
        ),
        min_size=0, max_size=50,
    ))
    def test_recent_commands_robustness(self, command_list):
        """Property: Recent commands handling is robust for arbitrary command lists."""
        config = ETXConfig()

        for command in command_list:
            config.add_recent_command(command)

        # Should never exceed MAX_RECENT
        assert len(config.recent_commands) <= config.MAX_RECENT

        # If any commands were added, most recent should be the last one added
        if command_list:
            assert config.recent_commands[0] == command_list[-1]

        # No duplicates should exist
        assert len(config.recent_commands) == len(set(config.recent_commands))

    @given(st.dictionaries(
        st.text(min_size=1, max_size=20),
        st.one_of(st.text(), st.integers(), st.booleans(), st.lists(st.text())),
        min_size=1, max_size=10
    ))
    def test_config_serialization_roundtrip(self, config_dict):
        """Property: Configuration serializes and deserializes consistently."""
        # Only use valid field names to avoid filter logic
        valid_fields = {"theme", "default_provider", "show_agent_tags", "reduced_motion",
                       "accessible_mode", "grid_columns", "recent_agents", "recent_commands"}

        filtered_dict = {k: v for k, v in config_dict.items() if k in valid_fields}

        if filtered_dict:  # Only test if we have valid fields
            try:
                # Create config with valid fields
                config = ETXConfig(**filtered_dict)

                # Serialize to JSON and back
                json_str = json.dumps(config.__dict__)
                parsed_dict = json.loads(json_str)

                # Should be able to reconstruct
                reconstructed = ETXConfig(
                    **{k: v for k, v in parsed_dict.items() if k in valid_fields}
                )

                # Valid fields should match
                for field in valid_fields:
                    if hasattr(config, field):
                        assert getattr(config, field) == getattr(reconstructed, field)

            except (TypeError, ValueError):
                # Some generated values may not be valid for specific fields
                pass


class TestETXConfigEdgeCases(unittest.TestCase):
    """Edge case tests for ETXConfig."""

    def test_unicode_recent_agents(self):
        """Test Unicode characters in recent agents."""
        config = ETXConfig()
        unicode_agents = ["建筑师", "デバッガー", "тестер", "🤖_agent"]

        for agent in unicode_agents:
            config.add_recent_agent(agent)

        assert len(config.recent_agents) == 4
        assert config.recent_agents[0] == "🤖_agent"

    def test_unicode_recent_commands(self):
        """Test Unicode characters in recent commands."""
        config = ETXConfig()
        unicode_commands = ["помощь", "ステータス", "帮助"]

        for cmd in unicode_commands:
            config.add_recent_command(cmd)

        assert len(config.recent_commands) == 3
        assert config.recent_commands[0] == "帮助"

    def test_empty_string_recent_items(self):
        """Test handling of empty strings in recent items."""
        config = ETXConfig()

        # Empty strings should be rejected
        config.add_recent_agent("")
        config.add_recent_command("")

        assert config.recent_agents == []
        assert config.recent_commands == []

    def test_whitespace_only_recent_items(self):
        """Test handling of whitespace-only recent items."""
        config = ETXConfig()

        # Whitespace-only strings should be rejected
        config.add_recent_agent("   ")
        config.add_recent_command("\t\n")

        assert config.recent_agents == []
        assert config.recent_commands == []

    def test_max_recent_boundary(self):
        """Test behavior exactly at MAX_RECENT boundary."""
        config = ETXConfig()

        # Add exactly MAX_RECENT items
        for i in range(config.MAX_RECENT):
            config.add_recent_agent(f"agent-{i}")

        assert len(config.recent_agents) == config.MAX_RECENT

        # Add one more
        config.add_recent_agent("overflow-agent")

        assert len(config.recent_agents) == config.MAX_RECENT
        assert config.recent_agents[0] == "overflow-agent"
        assert config.recent_agents[-1] == "agent-1"  # agent-0 should be gone

    def test_repeated_same_item(self):
        """Test adding the same item multiple times."""
        config = ETXConfig()

        # Add same agent multiple times
        for _ in range(5):
            config.add_recent_agent("same-agent")

        assert config.recent_agents == ["same-agent"]

    def test_zero_grid_columns(self):
        """Test edge case of zero grid columns."""
        config = ETXConfig(grid_columns=0)
        assert config.grid_columns == 0

    def test_negative_grid_columns(self):
        """Test edge case of negative grid columns."""
        config = ETXConfig(grid_columns=-1)
        assert config.grid_columns == -1

    def test_very_large_grid_columns(self):
        """Test very large grid column values."""
        large_value = 2**31 - 1
        config = ETXConfig(grid_columns=large_value)
        assert config.grid_columns == large_value


class TestETXConfigFileOperations(unittest.TestCase):
    """Tests for ETXConfig file I/O operations."""

    def setUp(self):
        """Set up temporary directory for testing."""
        self.temp_dir = tempfile.mkdtemp()
        self.temp_config_path = Path(self.temp_dir) / "test-config.json"

    def tearDown(self):
        """Clean up temporary directory."""
        import shutil
        shutil.rmtree(self.temp_dir)

    def test_real_file_save_and_load(self):
        """Test actual file save and load operations."""
        config = ETXConfig(
            theme="test-theme",
            recent_agents=["test-agent-1", "test-agent-2"]
        )

        # Mock CONFIG_PATH to use temp file
        with patch("tui.core.config.CONFIG_PATH", self.temp_config_path):
            config.save()
            loaded_config = ETXConfig.load()

        assert loaded_config.theme == "test-theme"
        assert loaded_config.recent_agents == ["test-agent-1", "test-agent-2"]

    def test_load_malformed_json(self):
        """Test loading malformed JSON file."""
        self.temp_config_path.write_text('{"theme": "test", "missing_quote: true}')

        with patch("tui.core.config.CONFIG_PATH", self.temp_config_path), \
                pytest.raises(json.JSONDecodeError):
            ETXConfig.load()

    def test_load_non_dict_json(self):
        """Test loading JSON that's not a dictionary."""
        self.temp_config_path.write_text('["not", "a", "dict"]')

        with patch("tui.core.config.CONFIG_PATH", self.temp_config_path), \
                pytest.raises(TypeError):
            ETXConfig.load()


if __name__ == "__main__":
    unittest.main()
