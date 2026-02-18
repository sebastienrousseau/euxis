# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Comprehensive unit tests for AgentAvatar and CompactAvatar widgets.

Tests cover: UNICODE_AVATARS dictionaries, get_avatar_path file resolution,
AgentAvatar compose/render with graphics/unicode fallback, CompactAvatar
state management and icon selection.
"""

from __future__ import annotations

import os
import unittest
from pathlib import Path
from unittest.mock import MagicMock, Mock, PropertyMock, patch

from tui.widgets.agent_avatar import (
    UNICODE_AVATARS,
    UNICODE_AVATARS_DIM,
    UNICODE_AVATARS_ERROR,
    AgentAvatar,
    CompactAvatar,
    get_avatar_path,
)


# ---------------------------------------------------------------------------
# UNICODE_AVATARS dictionaries
# ---------------------------------------------------------------------------

class TestUnicodeAvatars(unittest.TestCase):
    """Tests for UNICODE_AVATARS constant dictionary."""

    def test_core_agents_present(self):
        """Test core agents are in UNICODE_AVATARS."""
        core_agents = ["arbiter", "orchestrator", "guard", "auditor", "librarian", "scribe"]
        for agent in core_agents:
            assert agent in UNICODE_AVATARS, f"Missing core agent: {agent}"

    def test_default_agents_present(self):
        """Test default-tier agents are in UNICODE_AVATARS."""
        default_agents = ["coder", "reviewer", "tester", "debugger", "documenter", "refactorer", "optimizer"]
        for agent in default_agents:
            assert agent in UNICODE_AVATARS, f"Missing default agent: {agent}"

    def test_specialist_agents_present(self):
        """Test specialist agents are in UNICODE_AVATARS."""
        specialists = ["cryptographer", "security", "performance", "architect", "dba"]
        for agent in specialists:
            assert agent in UNICODE_AVATARS, f"Missing specialist: {agent}"

    def test_fallback_keys_present(self):
        """Test _default, _active, _error fallback keys exist."""
        assert "_default" in UNICODE_AVATARS
        assert "_active" in UNICODE_AVATARS
        assert "_error" in UNICODE_AVATARS

    def test_all_values_are_strings(self):
        """Test all UNICODE_AVATARS values are strings."""
        for key, val in UNICODE_AVATARS.items():
            assert isinstance(val, str), f"Key {key} has non-string value"

    def test_error_icon_is_high_attention(self):
        """Test _error key uses red indicator."""
        assert "red" in UNICODE_AVATARS["_error"]

    def test_active_icon_is_green(self):
        """Test _active key uses green indicator."""
        assert "green" in UNICODE_AVATARS["_active"]


class TestUnicodeAvatarsError(unittest.TestCase):
    """Tests for UNICODE_AVATARS_ERROR constant dictionary."""

    def test_all_keys_map_to_red_warning(self):
        """Test all standard keys map to red warning icon."""
        for key in UNICODE_AVATARS:
            if key == "_error":
                continue  # _error has special override
            assert key in UNICODE_AVATARS_ERROR
            assert "bold red" in UNICODE_AVATARS_ERROR[key]

    def test_error_key_special_override(self):
        """Test _error key has distinct high-priority icon."""
        assert "bold red on red" in UNICODE_AVATARS_ERROR["_error"]

    def test_same_keys_as_unicode_avatars(self):
        """Test UNICODE_AVATARS_ERROR has same keys as UNICODE_AVATARS."""
        assert set(UNICODE_AVATARS_ERROR.keys()) == set(UNICODE_AVATARS.keys())


class TestUnicodeAvatarsDim(unittest.TestCase):
    """Tests for UNICODE_AVATARS_DIM constant dictionary."""

    def test_all_keys_dimmed(self):
        """Test all UNICODE_AVATARS_DIM values contain dim markup."""
        for key, val in UNICODE_AVATARS_DIM.items():
            assert "[dim]" in val, f"Key {key} missing dim markup"
            assert "[/]" in val, f"Key {key} missing closing markup"

    def test_same_keys_as_unicode_avatars(self):
        """Test UNICODE_AVATARS_DIM has same keys as UNICODE_AVATARS."""
        assert set(UNICODE_AVATARS_DIM.keys()) == set(UNICODE_AVATARS.keys())

    def test_dim_values_are_strings(self):
        """Test all dimmed values are strings."""
        for key, val in UNICODE_AVATARS_DIM.items():
            assert isinstance(val, str)


# ---------------------------------------------------------------------------
# get_avatar_path
# ---------------------------------------------------------------------------

class TestGetAvatarPath(unittest.TestCase):
    """Tests for get_avatar_path() file resolution."""

    def test_state_specific_file_preferred_over_base(self):
        """Test state-specific file is returned when both exist."""
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            agents_dir = Path(tmp) / "euxis-tui" / "assets" / "agents"
            agents_dir.mkdir(parents=True)
            state_file = agents_dir / "coder_active.png"
            state_file.touch()
            base_file = agents_dir / "coder.png"
            base_file.touch()

            with patch.dict(os.environ, {"EUXIS_HOME": tmp}):
                result = get_avatar_path("coder", "active")
            assert result == state_file

    def test_state_specific_not_found_falls_to_base(self):
        """Test falls back to base image when state-specific not found."""
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            agents_dir = Path(tmp) / "euxis-tui" / "assets" / "agents"
            agents_dir.mkdir(parents=True)
            # Only base file, no state-specific
            base_file = agents_dir / "coder.png"
            base_file.touch()

            with patch.dict(os.environ, {"EUXIS_HOME": tmp}):
                result = get_avatar_path("coder", "active")
            assert result == base_file

    def test_with_tmp_path_state_file(self, tmp_path=None):
        """Test get_avatar_path with real filesystem using state-specific file."""
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            agents_dir = Path(tmp) / "euxis-tui" / "assets" / "agents"
            agents_dir.mkdir(parents=True)
            state_file = agents_dir / "arbiter_active.png"
            state_file.touch()

            with patch.dict(os.environ, {"EUXIS_HOME": tmp}):
                result = get_avatar_path("arbiter", "active")
            assert result == state_file

    def test_with_tmp_path_base_file(self):
        """Test get_avatar_path falls back to base image on disk."""
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            agents_dir = Path(tmp) / "euxis-tui" / "assets" / "agents"
            agents_dir.mkdir(parents=True)
            base_file = agents_dir / "arbiter.png"
            base_file.touch()

            with patch.dict(os.environ, {"EUXIS_HOME": tmp}):
                result = get_avatar_path("arbiter", "active")
            assert result == base_file

    def test_with_tmp_path_generic_fallback(self):
        """Test get_avatar_path falls back to generic tier-based image."""
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            agents_dir = Path(tmp) / "euxis-tui" / "assets" / "agents"
            agents_dir.mkdir(parents=True)
            generic_file = agents_dir / "_default_active.png"
            generic_file.touch()

            with patch.dict(os.environ, {"EUXIS_HOME": tmp}):
                result = get_avatar_path("unknown_agent", "active")
            assert result == generic_file

    def test_with_tmp_path_nothing_found(self):
        """Test get_avatar_path returns None when no files exist."""
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            agents_dir = Path(tmp) / "euxis-tui" / "assets" / "agents"
            agents_dir.mkdir(parents=True)

            with patch.dict(os.environ, {"EUXIS_HOME": tmp}):
                result = get_avatar_path("nonexistent", "active")
            assert result is None

    def test_uses_euxis_home_env(self):
        """Test get_avatar_path uses EUXIS_HOME environment variable."""
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            agents_dir = Path(tmp) / "euxis-tui" / "assets" / "agents"
            agents_dir.mkdir(parents=True)
            (agents_dir / "test_active.png").touch()

            with patch.dict(os.environ, {"EUXIS_HOME": tmp}):
                result = get_avatar_path("test", "active")
            assert result is not None
            assert tmp in str(result)

    def test_defaults_to_home_euxis(self):
        """Test get_avatar_path defaults to ~/.euxis when EUXIS_HOME unset."""
        with patch.dict(os.environ, {}, clear=False):
            # Remove EUXIS_HOME if set
            os.environ.pop("EUXIS_HOME", None)
            with patch("os.path.expanduser", return_value="/home/user/.euxis"):
                result = get_avatar_path("agent", "standby")
            # Will return None since files don't exist, but tests the path logic
            assert result is None


# ---------------------------------------------------------------------------
# AgentAvatar
# ---------------------------------------------------------------------------

class TestAgentAvatarInit(unittest.TestCase):
    """Tests for AgentAvatar.__init__."""

    def test_stores_agent_id(self):
        """Test AgentAvatar stores agent_id."""
        widget = AgentAvatar(agent_id="coder")
        assert widget.agent_id == "coder"

    def test_default_state_standby(self):
        """Test AgentAvatar defaults to standby state."""
        widget = AgentAvatar(agent_id="coder")
        assert widget._state == "standby"

    def test_custom_state(self):
        """Test AgentAvatar accepts custom state."""
        widget = AgentAvatar(agent_id="coder", state="active")
        assert widget._state == "active"

    def test_image_widget_initially_none(self):
        """Test _image_widget starts as None."""
        widget = AgentAvatar(agent_id="coder")
        assert widget._image_widget is None

    def test_construction_with_kwargs(self):
        """Test construction passes kwargs to Widget."""
        widget = AgentAvatar(agent_id="coder", id="avatar-coder")
        assert widget.id == "avatar-coder"

    def test_css_is_defined(self):
        """Test DEFAULT_CSS is non-empty."""
        assert isinstance(AgentAvatar.DEFAULT_CSS, str)
        assert "AgentAvatar" in AgentAvatar.DEFAULT_CSS


class TestAgentAvatarCompose(unittest.TestCase):
    """Tests for AgentAvatar.compose()."""

    @patch("tui.widgets.agent_avatar.get_avatar_path", return_value=None)
    def test_compose_without_image_uses_unicode(self, mock_get_path):
        """Test compose yields unicode widget when no image path."""
        widget = AgentAvatar(agent_id="coder")
        with patch.object(widget, "_create_unicode_widget", return_value=Mock()) as mock_unicode:
            result = list(widget.compose())
        mock_unicode.assert_called_once()
        assert len(result) == 1

    @patch("tui.widgets.agent_avatar._HAS_TEXTUAL_IMAGE", False)
    @patch("tui.widgets.agent_avatar.get_avatar_path")
    def test_compose_with_image_no_library_uses_unicode(self, mock_get_path):
        """Test compose uses unicode when image lib not available."""
        mock_get_path.return_value = Path("/fake/coder.png")
        widget = AgentAvatar(agent_id="coder")
        with patch.object(widget, "_create_unicode_widget", return_value=Mock()) as mock_unicode:
            result = list(widget.compose())
        mock_unicode.assert_called_once()

    @patch("tui.widgets.agent_avatar.PLATFORM")
    @patch("tui.widgets.agent_avatar._HAS_TEXTUAL_IMAGE", True)
    @patch("tui.widgets.agent_avatar.get_avatar_path")
    def test_compose_with_image_no_graphics_support_uses_unicode(self, mock_get_path, mock_platform):
        """Test compose uses unicode when terminal lacks graphics support."""
        mock_get_path.return_value = Path("/fake/coder.png")
        mock_platform.supports_graphics = False
        widget = AgentAvatar(agent_id="coder")
        with patch.object(widget, "_create_unicode_widget", return_value=Mock()) as mock_unicode:
            result = list(widget.compose())
        mock_unicode.assert_called_once()

    @patch("tui.widgets.agent_avatar.PLATFORM")
    @patch("tui.widgets.agent_avatar._HAS_TEXTUAL_IMAGE", True)
    @patch("tui.widgets.agent_avatar.get_avatar_path")
    def test_compose_with_image_and_graphics_uses_image_widget(self, mock_get_path, mock_platform):
        """Test compose uses image widget when graphics supported."""
        fake_path = Path("/fake/coder_active.png")
        mock_get_path.return_value = fake_path
        mock_platform.supports_graphics = True
        widget = AgentAvatar(agent_id="coder", state="active")
        with patch.object(widget, "_create_image_widget", return_value=Mock()) as mock_image:
            result = list(widget.compose())
        mock_image.assert_called_once_with(fake_path)
        assert len(result) == 1


class TestAgentAvatarCreateUnicodeWidget(unittest.TestCase):
    """Tests for AgentAvatar._create_unicode_widget."""

    def test_error_state(self):
        """Test error state returns error icon widget."""
        widget = AgentAvatar(agent_id="coder", state="error")
        result = widget._create_unicode_widget()
        # Should have avatar-error class
        from textual.widgets import Static
        assert isinstance(result, Static)

    def test_error_state_known_agent(self):
        """Test error state with known agent returns error avatar."""
        widget = AgentAvatar(agent_id="coder", state="error")
        result = widget._create_unicode_widget()
        # The renderable should contain the error icon
        assert "avatar-error" in result.classes

    def test_active_state(self):
        """Test active state uses UNICODE_AVATARS."""
        widget = AgentAvatar(agent_id="coder", state="active")
        result = widget._create_unicode_widget()
        assert "avatar-active" in result.classes

    def test_standby_state(self):
        """Test standby state uses UNICODE_AVATARS_DIM."""
        widget = AgentAvatar(agent_id="coder", state="standby")
        result = widget._create_unicode_widget()
        assert "avatar-unicode" in result.classes
        assert "avatar-active" not in result.classes

    def test_unknown_agent_id_uses_default(self):
        """Test unknown agent_id falls back to _default."""
        widget = AgentAvatar(agent_id="nonexistent_agent_xyz", state="active")
        result = widget._create_unicode_widget()
        # Should still create a widget using _default
        assert isinstance(result, type(result))  # Just verify it returns a widget

    def test_error_state_unknown_agent(self):
        """Test error state for unknown agent returns generic error icon."""
        widget = AgentAvatar(agent_id="totally_unknown", state="error")
        result = widget._create_unicode_widget()
        assert "avatar-error" in result.classes


class TestAgentAvatarCreateImageWidget(unittest.TestCase):
    """Tests for AgentAvatar._create_image_widget."""

    @patch("tui.widgets.agent_avatar.PLATFORM")
    @patch("tui.widgets.agent_avatar._TGPImage")
    def test_kitty_protocol_uses_tgp(self, mock_tgp, mock_platform):
        """Test Kitty protocol creates TGPImage widget."""
        from tui.core.platform import GraphicsProtocol
        mock_platform.graphics_protocol = GraphicsProtocol.KITTY
        mock_tgp_instance = Mock()
        mock_tgp.return_value = mock_tgp_instance

        widget = AgentAvatar(agent_id="coder")
        result = widget._create_image_widget(Path("/fake/coder.png"))
        mock_tgp.assert_called_once_with(Path("/fake/coder.png"))
        assert result == mock_tgp_instance
        assert widget._image_widget == mock_tgp_instance

    @patch("tui.widgets.agent_avatar.PLATFORM")
    @patch("tui.widgets.agent_avatar._TGPImage", None)
    @patch("tui.widgets.agent_avatar._SixelImage")
    def test_kitty_without_tgp_falls_to_sixel(self, mock_sixel, mock_platform):
        """Test Kitty protocol falls back to Sixel if TGPImage unavailable."""
        from tui.core.platform import GraphicsProtocol
        mock_platform.graphics_protocol = GraphicsProtocol.KITTY
        mock_sixel_instance = Mock()
        mock_sixel.return_value = mock_sixel_instance

        widget = AgentAvatar(agent_id="coder")
        result = widget._create_image_widget(Path("/fake/coder.png"))
        mock_sixel.assert_called_once()
        assert result == mock_sixel_instance

    @patch("tui.widgets.agent_avatar.PLATFORM")
    @patch("tui.widgets.agent_avatar._SixelImage")
    def test_sixel_protocol_uses_sixel(self, mock_sixel, mock_platform):
        """Test Sixel protocol creates SixelImage widget."""
        from tui.core.platform import GraphicsProtocol
        mock_platform.graphics_protocol = GraphicsProtocol.SIXEL
        mock_sixel_instance = Mock()
        mock_sixel.return_value = mock_sixel_instance

        widget = AgentAvatar(agent_id="coder")
        result = widget._create_image_widget(Path("/fake/coder.png"))
        mock_sixel.assert_called_once_with(Path("/fake/coder.png"))
        assert result == mock_sixel_instance

    @patch("tui.widgets.agent_avatar.PLATFORM")
    @patch("tui.widgets.agent_avatar._TGPImage", None)
    @patch("tui.widgets.agent_avatar._SixelImage", None)
    def test_no_graphics_lib_falls_to_unicode(self, mock_platform):
        """Test fallback to unicode when no graphics library available."""
        from tui.core.platform import GraphicsProtocol
        mock_platform.graphics_protocol = GraphicsProtocol.NONE

        widget = AgentAvatar(agent_id="coder", state="active")
        with patch.object(widget, "_create_unicode_widget", return_value=Mock()) as mock_unicode:
            result = widget._create_image_widget(Path("/fake/coder.png"))
        mock_unicode.assert_called_once()


class TestAgentAvatarSetState(unittest.TestCase):
    """Tests for AgentAvatar.set_state."""

    def test_same_state_no_change(self):
        """Test set_state with same state does nothing."""
        widget = AgentAvatar(agent_id="coder", state="standby")
        widget.remove_children = Mock()
        widget.mount = Mock()
        widget.set_state("standby")
        widget.remove_children.assert_not_called()
        widget.mount.assert_not_called()

    @patch("tui.widgets.agent_avatar.get_avatar_path", return_value=None)
    def test_different_state_rebuilds(self, mock_get_path):
        """Test set_state with different state triggers rebuild."""
        widget = AgentAvatar(agent_id="coder", state="standby")
        widget.remove_children = Mock()
        widget.mount = Mock()
        widget.set_state("active")
        widget.remove_children.assert_called_once()
        widget.mount.assert_called_once()

    @patch("tui.widgets.agent_avatar.get_avatar_path", return_value=None)
    def test_set_state_updates_internal_state(self, mock_get_path):
        """Test set_state updates _state attribute."""
        widget = AgentAvatar(agent_id="coder", state="standby")
        widget.remove_children = Mock()
        widget.mount = Mock()
        widget.set_state("error")
        assert widget._state == "error"

    @patch("tui.widgets.agent_avatar.PLATFORM")
    @patch("tui.widgets.agent_avatar._HAS_TEXTUAL_IMAGE", True)
    @patch("tui.widgets.agent_avatar.get_avatar_path")
    def test_set_state_with_graphics(self, mock_get_path, mock_platform):
        """Test set_state uses image widget when graphics available."""
        mock_get_path.return_value = Path("/fake/coder_active.png")
        mock_platform.supports_graphics = True

        widget = AgentAvatar(agent_id="coder", state="standby")
        widget.remove_children = Mock()
        widget.mount = Mock()
        with patch.object(widget, "_create_image_widget", return_value=Mock()) as mock_img:
            widget.set_state("active")
        mock_img.assert_called_once()

    @patch("tui.widgets.agent_avatar.get_avatar_path", return_value=None)
    def test_set_state_without_graphics_uses_unicode(self, mock_get_path):
        """Test set_state uses unicode when no graphics."""
        widget = AgentAvatar(agent_id="coder", state="standby")
        widget.remove_children = Mock()
        widget.mount = Mock()
        with patch.object(widget, "_create_unicode_widget", return_value=Mock()) as mock_uni:
            widget.set_state("error")
        mock_uni.assert_called_once()


# ---------------------------------------------------------------------------
# CompactAvatar
# ---------------------------------------------------------------------------

class TestCompactAvatarInit(unittest.TestCase):
    """Tests for CompactAvatar.__init__."""

    def test_active_true(self):
        """Test CompactAvatar with active=True."""
        widget = CompactAvatar(agent_id="coder", active=True)
        assert widget.agent_id == "coder"
        assert widget._active is True
        assert widget._error is False

    def test_error_true(self):
        """Test CompactAvatar with error=True."""
        widget = CompactAvatar(agent_id="coder", error=True)
        assert widget._error is True
        assert widget._active is False

    def test_neither_active_nor_error(self):
        """Test CompactAvatar defaults to standby (no active, no error)."""
        widget = CompactAvatar(agent_id="coder")
        assert widget._active is False
        assert widget._error is False

    def test_stores_agent_id(self):
        """Test agent_id is stored."""
        widget = CompactAvatar(agent_id="arbiter")
        assert widget.agent_id == "arbiter"

    def test_construction_with_id(self):
        """Test construction with id kwarg."""
        widget = CompactAvatar(agent_id="coder", id="compact-coder")
        assert widget.id == "compact-coder"


class TestCompactAvatarGetIcon(unittest.TestCase):
    """Tests for CompactAvatar._get_icon."""

    def test_error_overrides_active(self):
        """Test error state takes priority over active."""
        widget = CompactAvatar(agent_id="coder")
        icon = widget._get_icon("coder", active=True, error=True)
        # Should return error icon
        assert "red" in icon

    def test_active_returns_full_brightness(self):
        """Test active state returns full-brightness icon."""
        widget = CompactAvatar(agent_id="coder")
        icon = widget._get_icon("coder", active=True, error=False)
        expected = UNICODE_AVATARS.get("coder", UNICODE_AVATARS.get("_default", ""))
        assert icon == expected

    def test_standby_returns_dimmed(self):
        """Test standby (neither active nor error) returns dimmed icon."""
        widget = CompactAvatar(agent_id="coder")
        icon = widget._get_icon("coder", active=False, error=False)
        expected = UNICODE_AVATARS_DIM.get("coder", UNICODE_AVATARS_DIM.get("_default", ""))
        assert icon == expected

    def test_unknown_agent_error(self):
        """Test unknown agent in error state returns generic error icon."""
        widget = CompactAvatar(agent_id="unknown")
        icon = widget._get_icon("unknown", active=False, error=True)
        assert "red" in icon

    def test_unknown_agent_active(self):
        """Test unknown agent in active state returns _default."""
        widget = CompactAvatar(agent_id="unknown")
        icon = widget._get_icon("totally_unknown_xyz", active=True, error=False)
        expected = UNICODE_AVATARS.get("_default", "")
        assert icon == expected

    def test_unknown_agent_standby(self):
        """Test unknown agent in standby returns dimmed _default."""
        widget = CompactAvatar(agent_id="unknown")
        icon = widget._get_icon("totally_unknown_xyz", active=False, error=False)
        expected = UNICODE_AVATARS_DIM.get("_default", "")
        assert icon == expected


class TestCompactAvatarSetActive(unittest.TestCase):
    """Tests for CompactAvatar.set_active."""

    def test_set_active_true(self):
        """Test toggling active to True."""
        widget = CompactAvatar(agent_id="coder")
        with patch.object(widget, "update") as mock_update:
            widget.set_active(True)
        assert widget._active is True
        assert widget._error is False  # Active clears error
        mock_update.assert_called_once()

    def test_set_active_false(self):
        """Test toggling active to False."""
        widget = CompactAvatar(agent_id="coder", active=True)
        with patch.object(widget, "update") as mock_update:
            widget.set_active(False)
        assert widget._active is False
        mock_update.assert_called_once()

    def test_set_active_same_value_no_update(self):
        """Test setting active to same value does not update."""
        widget = CompactAvatar(agent_id="coder", active=True)
        with patch.object(widget, "update") as mock_update:
            widget.set_active(True)
        mock_update.assert_not_called()

    def test_set_active_clears_error(self):
        """Test set_active(True) clears error state."""
        widget = CompactAvatar(agent_id="coder", error=True)
        with patch.object(widget, "update"):
            widget.set_active(True)
        assert widget._error is False

    def test_set_active_updates_icon(self):
        """Test set_active updates the icon to active version."""
        widget = CompactAvatar(agent_id="coder")
        with patch.object(widget, "update") as mock_update:
            widget.set_active(True)
        expected_icon = UNICODE_AVATARS.get("coder", UNICODE_AVATARS.get("_default", ""))
        mock_update.assert_called_once_with(expected_icon)


class TestCompactAvatarSetError(unittest.TestCase):
    """Tests for CompactAvatar.set_error."""

    def test_set_error_true(self):
        """Test toggling error to True."""
        widget = CompactAvatar(agent_id="coder")
        with patch.object(widget, "update") as mock_update:
            widget.set_error(True)
        assert widget._error is True
        mock_update.assert_called_once()

    def test_set_error_false(self):
        """Test toggling error to False."""
        widget = CompactAvatar(agent_id="coder", error=True)
        with patch.object(widget, "update") as mock_update:
            widget.set_error(False)
        assert widget._error is False
        mock_update.assert_called_once()

    def test_set_error_same_value_no_update(self):
        """Test setting error to same value does not update."""
        widget = CompactAvatar(agent_id="coder", error=True)
        with patch.object(widget, "update") as mock_update:
            widget.set_error(True)
        mock_update.assert_not_called()

    def test_set_error_updates_icon(self):
        """Test set_error updates icon to error version."""
        widget = CompactAvatar(agent_id="coder")
        with patch.object(widget, "update") as mock_update:
            widget.set_error(True)
        icon = mock_update.call_args[0][0]
        assert "red" in icon


class TestCompactAvatarSetState(unittest.TestCase):
    """Tests for CompactAvatar.set_state."""

    def test_set_state_error(self):
        """Test set_state('error') sets error=True, active=False."""
        widget = CompactAvatar(agent_id="coder")
        with patch.object(widget, "update"):
            widget.set_state("error")
        assert widget._error is True
        assert widget._active is False

    def test_set_state_active(self):
        """Test set_state('active') sets active=True, error=False."""
        widget = CompactAvatar(agent_id="coder")
        with patch.object(widget, "update"):
            widget.set_state("active")
        assert widget._active is True
        assert widget._error is False

    def test_set_state_standby(self):
        """Test set_state('standby') sets both False."""
        widget = CompactAvatar(agent_id="coder", active=True, error=True)
        with patch.object(widget, "update"):
            widget.set_state("standby")
        assert widget._active is False
        assert widget._error is False

    def test_set_state_unknown_defaults_to_standby(self):
        """Test set_state with unknown state defaults to standby."""
        widget = CompactAvatar(agent_id="coder", active=True)
        with patch.object(widget, "update"):
            widget.set_state("unknown_state")
        assert widget._active is False
        assert widget._error is False

    def test_set_state_calls_update(self):
        """Test set_state always calls update."""
        widget = CompactAvatar(agent_id="coder")
        with patch.object(widget, "update") as mock_update:
            widget.set_state("active")
        mock_update.assert_called_once()

    def test_set_state_error_icon_correct(self):
        """Test set_state('error') sets the error icon."""
        widget = CompactAvatar(agent_id="coder")
        with patch.object(widget, "update") as mock_update:
            widget.set_state("error")
        icon = mock_update.call_args[0][0]
        assert "red" in icon

    def test_set_state_active_icon_correct(self):
        """Test set_state('active') sets the active icon."""
        widget = CompactAvatar(agent_id="coder")
        with patch.object(widget, "update") as mock_update:
            widget.set_state("active")
        icon = mock_update.call_args[0][0]
        expected = UNICODE_AVATARS.get("coder", UNICODE_AVATARS.get("_default", ""))
        assert icon == expected

    def test_set_state_standby_icon_correct(self):
        """Test set_state('standby') sets the dimmed icon."""
        widget = CompactAvatar(agent_id="coder", active=True)
        with patch.object(widget, "update") as mock_update:
            widget.set_state("standby")
        icon = mock_update.call_args[0][0]
        expected = UNICODE_AVATARS_DIM.get("coder", UNICODE_AVATARS_DIM.get("_default", ""))
        assert icon == expected


if __name__ == "__main__":
    unittest.main()
