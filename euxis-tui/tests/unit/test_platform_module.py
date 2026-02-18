# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Unit tests for tui/core/platform.py — cross-platform compatibility.

Tests cover: PlatformType, GraphicsProtocol, KeyMap, Platform class
(platform_type, _is_wsl, terminal, graphics_protocol, _supports_sixel,
keymap, is_macos, is_linux, is_wsl, supports_graphics, accent_color,
get_modifier_display, format_shortcut), get_platform_bindings,
is_wsl_compatibility_needed.

IMPORTANT: Since Platform uses @cached_property, each test creates a
fresh Platform() instance to avoid caching issues.
"""

from __future__ import annotations

import os
import unittest
from unittest.mock import MagicMock, Mock, mock_open, patch

from tui.core.platform import (
    LINUX_KEYMAP,
    MACOS_KEYMAP,
    WSL_KEYMAP,
    GraphicsProtocol,
    KeyMap,
    Platform,
    PlatformType,
    get_platform_bindings,
    is_wsl_compatibility_needed,
)


# =============================================================================
# PlatformType Enum
# =============================================================================


class TestPlatformType(unittest.TestCase):
    """Tests for the PlatformType enum."""

    def test_macos_exists(self):
        assert PlatformType.MACOS is not None

    def test_linux_exists(self):
        assert PlatformType.LINUX is not None

    def test_wsl_exists(self):
        assert PlatformType.WSL is not None

    def test_unknown_exists(self):
        assert PlatformType.UNKNOWN is not None

    def test_all_values_unique(self):
        values = [e.value for e in PlatformType]
        assert len(values) == len(set(values))


# =============================================================================
# GraphicsProtocol Enum
# =============================================================================


class TestGraphicsProtocol(unittest.TestCase):
    """Tests for the GraphicsProtocol enum."""

    def test_none_exists(self):
        assert GraphicsProtocol.NONE is not None

    def test_sixel_exists(self):
        assert GraphicsProtocol.SIXEL is not None

    def test_kitty_exists(self):
        assert GraphicsProtocol.KITTY is not None

    def test_iterm2_exists(self):
        assert GraphicsProtocol.ITERM2 is not None


# =============================================================================
# KeyMap
# =============================================================================


class TestKeyMap(unittest.TestCase):
    """Tests for KeyMap dataclass instances."""

    def test_macos_keymap_fields(self):
        assert MACOS_KEYMAP.copy == "cmd+c"
        assert MACOS_KEYMAP.paste == "cmd+v"
        assert MACOS_KEYMAP.clear == "cmd+k"
        assert MACOS_KEYMAP.quit == "cmd+q"
        assert MACOS_KEYMAP.command_palette == "cmd+k"
        assert MACOS_KEYMAP.meta_display == "\u2325"
        assert MACOS_KEYMAP.meta_key == "option"

    def test_linux_keymap_fields(self):
        assert LINUX_KEYMAP.copy == "ctrl+shift+c"
        assert LINUX_KEYMAP.paste == "ctrl+shift+v"
        assert LINUX_KEYMAP.clear == "ctrl+l"
        assert LINUX_KEYMAP.quit == "ctrl+q"
        assert LINUX_KEYMAP.command_palette == "ctrl+k"
        assert LINUX_KEYMAP.meta_display == "Alt"
        assert LINUX_KEYMAP.meta_key == "alt"

    def test_wsl_keymap_fields(self):
        assert WSL_KEYMAP.copy == "ctrl+shift+c"
        assert WSL_KEYMAP.paste == "ctrl+shift+v"
        assert WSL_KEYMAP.command_palette == "ctrl+p"
        assert WSL_KEYMAP.meta_display == "Alt"
        assert WSL_KEYMAP.meta_key == "alt"

    def test_keymap_frozen(self):
        """KeyMap is frozen — attributes cannot be set."""
        with self.assertRaises(AttributeError):
            MACOS_KEYMAP.copy = "ctrl+c"


# =============================================================================
# Platform.platform_type
# =============================================================================


class TestPlatformPlatformType(unittest.TestCase):
    """Tests for Platform.platform_type detection."""

    @patch("platform.system", return_value="Darwin")
    def test_macos_detected(self, mock_sys):
        p = Platform()
        assert p.platform_type == PlatformType.MACOS

    @patch("platform.system", return_value="Linux")
    def test_linux_detected(self, mock_sys):
        p = Platform()
        # Need to also mock _is_wsl to return False
        with patch.object(Platform, "_is_wsl", return_value=False):
            assert p.platform_type == PlatformType.LINUX

    @patch("platform.system", return_value="Linux")
    def test_wsl_detected(self, mock_sys):
        p = Platform()
        with patch.object(Platform, "_is_wsl", return_value=True):
            assert p.platform_type == PlatformType.WSL

    @patch("platform.system", return_value="Windows")
    def test_unknown_for_windows(self, mock_sys):
        p = Platform()
        assert p.platform_type == PlatformType.UNKNOWN

    @patch("platform.system", return_value="FreeBSD")
    def test_unknown_for_other(self, mock_sys):
        p = Platform()
        assert p.platform_type == PlatformType.UNKNOWN


# =============================================================================
# Platform._is_wsl
# =============================================================================


class TestPlatformIsWsl(unittest.TestCase):
    """Tests for Platform._is_wsl detection."""

    def test_wsl_via_proc_version_microsoft(self):
        p = Platform()
        m = mock_open(read_data="Linux version 5.15.0-microsoft-standard-WSL2")
        with patch("builtins.open", m), \
             patch.dict("os.environ", {}, clear=True), \
             patch("os.path.exists", return_value=False):
            assert p._is_wsl() is True

    def test_wsl_via_proc_version_wsl_keyword(self):
        p = Platform()
        m = mock_open(read_data="Linux version 5.15.0-wsl2")
        with patch("builtins.open", m), \
             patch.dict("os.environ", {}, clear=True), \
             patch("os.path.exists", return_value=False):
            assert p._is_wsl() is True

    def test_not_wsl_normal_linux(self):
        p = Platform()
        m = mock_open(read_data="Linux version 6.5.0-generic")
        with patch("builtins.open", m), \
             patch.dict("os.environ", {}, clear=True), \
             patch("os.path.exists", return_value=False):
            assert p._is_wsl() is False

    def test_wsl_via_env_wsl_distro_name(self):
        p = Platform()
        with patch("builtins.open", side_effect=FileNotFoundError), \
             patch.dict("os.environ", {"WSL_DISTRO_NAME": "Ubuntu"}, clear=True), \
             patch("os.path.exists", return_value=False):
            assert p._is_wsl() is True

    def test_wsl_via_env_wslenv(self):
        p = Platform()
        with patch("builtins.open", side_effect=FileNotFoundError), \
             patch.dict("os.environ", {"WSLENV": "USERPROFILE"}, clear=True), \
             patch("os.path.exists", return_value=False):
            assert p._is_wsl() is True

    def test_wsl_via_interop_file(self):
        p = Platform()
        with patch("builtins.open", side_effect=FileNotFoundError), \
             patch.dict("os.environ", {}, clear=True), \
             patch("os.path.exists", return_value=True):
            assert p._is_wsl() is True

    def test_proc_version_permission_error(self):
        """PermissionError on /proc/version should be caught."""
        p = Platform()
        with patch("builtins.open", side_effect=PermissionError), \
             patch.dict("os.environ", {}, clear=True), \
             patch("os.path.exists", return_value=False):
            assert p._is_wsl() is False


# =============================================================================
# Platform.terminal
# =============================================================================


class TestPlatformTerminal(unittest.TestCase):
    """Tests for Platform.terminal detection."""

    def test_term_program_set(self):
        p = Platform()
        with patch.dict("os.environ", {"TERM_PROGRAM": "iTerm.app"}, clear=True):
            assert p.terminal == "iterm.app"

    def test_kitty_via_window_id(self):
        p = Platform()
        with patch.dict("os.environ", {"KITTY_WINDOW_ID": "1", "TERM_PROGRAM": ""}, clear=True):
            assert p.terminal == "kitty"

    def test_iterm2_via_session_id(self):
        p = Platform()
        with patch.dict("os.environ", {"ITERM_SESSION_ID": "session123", "TERM_PROGRAM": ""}, clear=True):
            assert p.terminal == "iterm2"

    def test_windows_terminal_via_wt_session(self):
        p = Platform()
        with patch.dict("os.environ", {"WT_SESSION": "abc", "TERM_PROGRAM": ""}, clear=True):
            assert p.terminal == "windows-terminal"

    def test_alacritty_via_socket(self):
        p = Platform()
        with patch.dict("os.environ", {"ALACRITTY_SOCKET": "/tmp/alacritty.sock", "TERM_PROGRAM": ""}, clear=True):
            assert p.terminal == "alacritty"

    def test_wezterm_via_pane(self):
        p = Platform()
        with patch.dict("os.environ", {"WEZTERM_PANE": "0", "TERM_PROGRAM": ""}, clear=True):
            assert p.terminal == "wezterm"

    def test_fallback_to_term(self):
        p = Platform()
        with patch.dict("os.environ", {"TERM": "xterm-256color", "TERM_PROGRAM": ""}, clear=True):
            assert p.terminal == "xterm-256color"

    def test_fallback_to_unknown(self):
        p = Platform()
        with patch.dict("os.environ", {"TERM_PROGRAM": ""}, clear=True):
            result = p.terminal
            # Should be TERM value or "unknown"
            assert isinstance(result, str)


# =============================================================================
# Platform.graphics_protocol
# =============================================================================


class TestPlatformGraphicsProtocol(unittest.TestCase):
    """Tests for Platform.graphics_protocol detection."""

    def _make_platform_with_terminal(self, terminal_name):
        """Create a Platform instance with a fixed terminal value."""
        p = Platform()
        # We need to set cached_property directly
        p.__dict__["terminal"] = terminal_name
        return p

    def test_kitty_terminal_uses_kitty_protocol(self):
        p = self._make_platform_with_terminal("kitty")
        assert p.graphics_protocol == GraphicsProtocol.KITTY

    def test_ghostty_terminal_uses_kitty_protocol(self):
        p = self._make_platform_with_terminal("ghostty")
        assert p.graphics_protocol == GraphicsProtocol.KITTY

    def test_wezterm_terminal_uses_kitty_protocol(self):
        p = self._make_platform_with_terminal("wezterm")
        assert p.graphics_protocol == GraphicsProtocol.KITTY

    def test_iterm2_terminal_uses_iterm2_protocol(self):
        p = self._make_platform_with_terminal("iterm2")
        assert p.graphics_protocol == GraphicsProtocol.ITERM2

    def test_mlterm_uses_sixel(self):
        p = self._make_platform_with_terminal("mlterm")
        assert p.graphics_protocol == GraphicsProtocol.SIXEL

    def test_foot_uses_sixel(self):
        p = self._make_platform_with_terminal("foot")
        assert p.graphics_protocol == GraphicsProtocol.SIXEL

    def test_xterm_uses_sixel(self):
        p = self._make_platform_with_terminal("xterm")
        assert p.graphics_protocol == GraphicsProtocol.SIXEL

    def test_unknown_terminal_no_graphics(self):
        p = self._make_platform_with_terminal("unknown")
        with patch.dict("os.environ", {"TERM": "dumb"}):
            assert p.graphics_protocol == GraphicsProtocol.NONE

    def test_alacritty_no_sixel(self):
        """Alacritty is not in the sixel list and should return NONE."""
        p = self._make_platform_with_terminal("alacritty")
        with patch.dict("os.environ", {"TERM": "alacritty"}):
            assert p.graphics_protocol == GraphicsProtocol.NONE

    def test_sixel_in_term_env(self):
        """If TERM contains 'sixel', sixel should be detected."""
        p = self._make_platform_with_terminal("some-unknown-terminal")
        with patch.dict("os.environ", {"TERM": "xterm-sixel"}):
            assert p.graphics_protocol == GraphicsProtocol.SIXEL


# =============================================================================
# Platform._supports_sixel
# =============================================================================


class TestPlatformSupportsSixel(unittest.TestCase):
    """Tests for Platform._supports_sixel."""

    def test_known_sixel_terminals(self):
        known = ["mlterm", "xterm", "foot", "wezterm", "contour", "ghostty", "rio", "ctx"]
        for terminal in known:
            p = Platform()
            p.__dict__["terminal"] = terminal
            assert p._supports_sixel() is True, f"{terminal} should support sixel"

    def test_unknown_terminal_no_sixel(self):
        p = Platform()
        p.__dict__["terminal"] = "something-random"
        with patch.dict("os.environ", {"TERM": "vt100"}):
            assert p._supports_sixel() is False

    def test_sixel_in_term_variable(self):
        p = Platform()
        p.__dict__["terminal"] = "custom-term"
        with patch.dict("os.environ", {"TERM": "my-sixel-term"}):
            assert p._supports_sixel() is True


# =============================================================================
# Platform.keymap
# =============================================================================


class TestPlatformKeymap(unittest.TestCase):
    """Tests for Platform.keymap property."""

    @patch("platform.system", return_value="Darwin")
    def test_macos_uses_macos_keymap(self, mock_sys):
        p = Platform()
        assert p.keymap is MACOS_KEYMAP

    @patch("platform.system", return_value="Linux")
    def test_linux_uses_linux_keymap(self, mock_sys):
        p = Platform()
        with patch.object(Platform, "_is_wsl", return_value=False):
            assert p.keymap is LINUX_KEYMAP

    @patch("platform.system", return_value="Linux")
    def test_wsl_uses_wsl_keymap(self, mock_sys):
        p = Platform()
        with patch.object(Platform, "_is_wsl", return_value=True):
            assert p.keymap is WSL_KEYMAP

    @patch("platform.system", return_value="FreeBSD")
    def test_unknown_uses_linux_keymap(self, mock_sys):
        p = Platform()
        assert p.keymap is LINUX_KEYMAP


# =============================================================================
# Platform boolean properties: is_macos, is_linux, is_wsl
# =============================================================================


class TestPlatformBooleanProperties(unittest.TestCase):
    """Tests for is_macos, is_linux, is_wsl properties."""

    @patch("platform.system", return_value="Darwin")
    def test_is_macos_true(self, mock_sys):
        p = Platform()
        assert p.is_macos is True
        assert p.is_linux is False
        assert p.is_wsl is False

    @patch("platform.system", return_value="Linux")
    def test_is_linux_true(self, mock_sys):
        p = Platform()
        with patch.object(Platform, "_is_wsl", return_value=False):
            assert p.is_linux is True
            assert p.is_macos is False
            assert p.is_wsl is False

    @patch("platform.system", return_value="Linux")
    def test_is_wsl_true(self, mock_sys):
        p = Platform()
        with patch.object(Platform, "_is_wsl", return_value=True):
            assert p.is_wsl is True
            assert p.is_macos is False
            assert p.is_linux is False

    @patch("platform.system", return_value="Windows")
    def test_unknown_all_false(self, mock_sys):
        p = Platform()
        assert p.is_macos is False
        assert p.is_linux is False
        assert p.is_wsl is False


# =============================================================================
# Platform.supports_graphics
# =============================================================================


class TestPlatformSupportsGraphics(unittest.TestCase):
    """Tests for supports_graphics property."""

    def test_kitty_supports_graphics(self):
        p = Platform()
        p.__dict__["terminal"] = "kitty"
        assert p.supports_graphics is True

    def test_no_graphics(self):
        p = Platform()
        p.__dict__["terminal"] = "dumb"
        with patch.dict("os.environ", {"TERM": "dumb"}):
            assert p.supports_graphics is False


# =============================================================================
# Platform.accent_color
# =============================================================================


class TestPlatformAccentColor(unittest.TestCase):
    """Tests for accent_color property."""

    @patch("platform.system", return_value="Darwin")
    def test_macos_blue(self, mock_sys):
        p = Platform()
        assert p.accent_color == "#007AFF"

    @patch("platform.system", return_value="Linux")
    def test_wsl_windows_blue(self, mock_sys):
        p = Platform()
        with patch.object(Platform, "_is_wsl", return_value=True):
            assert p.accent_color == "#0078D4"

    @patch("platform.system", return_value="Linux")
    def test_linux_cyan(self, mock_sys):
        p = Platform()
        with patch.object(Platform, "_is_wsl", return_value=False):
            assert p.accent_color == "#00D9FF"

    @patch("platform.system", return_value="FreeBSD")
    def test_unknown_cyan(self, mock_sys):
        p = Platform()
        assert p.accent_color == "#00D9FF"


# =============================================================================
# Platform.get_modifier_display
# =============================================================================


class TestPlatformGetModifierDisplay(unittest.TestCase):
    """Tests for get_modifier_display on macOS vs Linux."""

    @patch("platform.system", return_value="Darwin")
    def test_macos_ctrl(self, mock_sys):
        p = Platform()
        assert p.get_modifier_display("ctrl") == "\u2303"

    @patch("platform.system", return_value="Darwin")
    def test_macos_meta(self, mock_sys):
        p = Platform()
        assert p.get_modifier_display("meta") == "\u2325"

    @patch("platform.system", return_value="Darwin")
    def test_macos_shift(self, mock_sys):
        p = Platform()
        assert p.get_modifier_display("shift") == "\u21e7"

    @patch("platform.system", return_value="Darwin")
    def test_macos_alt(self, mock_sys):
        p = Platform()
        assert p.get_modifier_display("alt") == "\u2325"

    @patch("platform.system", return_value="Darwin")
    def test_macos_cmd(self, mock_sys):
        p = Platform()
        assert p.get_modifier_display("cmd") == "\u2318"

    @patch("platform.system", return_value="Darwin")
    def test_macos_command(self, mock_sys):
        p = Platform()
        assert p.get_modifier_display("command") == "\u2318"

    @patch("platform.system", return_value="Darwin")
    def test_macos_unknown_modifier(self, mock_sys):
        p = Platform()
        assert p.get_modifier_display("hyper") == "hyper"

    @patch("platform.system", return_value="Linux")
    def test_linux_ctrl(self, mock_sys):
        p = Platform()
        with patch.object(Platform, "_is_wsl", return_value=False):
            assert p.get_modifier_display("ctrl") == "Ctrl"

    @patch("platform.system", return_value="Linux")
    def test_linux_meta(self, mock_sys):
        p = Platform()
        with patch.object(Platform, "_is_wsl", return_value=False):
            assert p.get_modifier_display("meta") == "Alt"

    @patch("platform.system", return_value="Linux")
    def test_linux_shift(self, mock_sys):
        p = Platform()
        with patch.object(Platform, "_is_wsl", return_value=False):
            assert p.get_modifier_display("shift") == "Shift"

    @patch("platform.system", return_value="Linux")
    def test_linux_cmd(self, mock_sys):
        p = Platform()
        with patch.object(Platform, "_is_wsl", return_value=False):
            assert p.get_modifier_display("cmd") == "Super"

    @patch("platform.system", return_value="Linux")
    def test_linux_unknown_modifier(self, mock_sys):
        p = Platform()
        with patch.object(Platform, "_is_wsl", return_value=False):
            assert p.get_modifier_display("hyper") == "hyper"

    @patch("platform.system", return_value="Darwin")
    def test_case_insensitive(self, mock_sys):
        p = Platform()
        assert p.get_modifier_display("CTRL") == "\u2303"
        assert p.get_modifier_display("Shift") == "\u21e7"


# =============================================================================
# Platform.format_shortcut
# =============================================================================


class TestPlatformFormatShortcut(unittest.TestCase):
    """Tests for format_shortcut on macOS vs Linux."""

    @patch("platform.system", return_value="Darwin")
    def test_macos_ctrl_k(self, mock_sys):
        p = Platform()
        result = p.format_shortcut("ctrl+k")
        # macOS: joined without separator
        assert "\u2303" in result
        assert "K" in result
        # Should be concatenated, not +
        assert "+" not in result

    @patch("platform.system", return_value="Darwin")
    def test_macos_meta_x(self, mock_sys):
        p = Platform()
        result = p.format_shortcut("meta+x")
        assert "\u2325" in result
        assert "X" in result

    @patch("platform.system", return_value="Darwin")
    def test_macos_shift_ctrl_k(self, mock_sys):
        p = Platform()
        result = p.format_shortcut("ctrl+shift+k")
        assert "\u2303" in result
        assert "\u21e7" in result
        assert "K" in result

    @patch("platform.system", return_value="Darwin")
    def test_macos_cmd_shortcut(self, mock_sys):
        p = Platform()
        result = p.format_shortcut("cmd+q")
        assert "\u2318" in result
        assert "Q" in result

    @patch("platform.system", return_value="Darwin")
    def test_macos_super_shortcut(self, mock_sys):
        p = Platform()
        result = p.format_shortcut("super+l")
        assert "\u2318" in result
        assert "L" in result

    @patch("platform.system", return_value="Darwin")
    def test_macos_option_shortcut(self, mock_sys):
        p = Platform()
        result = p.format_shortcut("option+a")
        assert "\u2325" in result
        assert "A" in result

    @patch("platform.system", return_value="Darwin")
    def test_macos_control_alias(self, mock_sys):
        p = Platform()
        result = p.format_shortcut("control+z")
        assert "\u2303" in result
        assert "Z" in result

    @patch("platform.system", return_value="Linux")
    def test_linux_ctrl_k(self, mock_sys):
        p = Platform()
        with patch.object(Platform, "_is_wsl", return_value=False):
            result = p.format_shortcut("ctrl+k")
        # Linux: joined with +
        assert "Ctrl" in result
        assert "K" in result
        assert "+" in result

    @patch("platform.system", return_value="Linux")
    def test_linux_shift_ctrl_k(self, mock_sys):
        p = Platform()
        with patch.object(Platform, "_is_wsl", return_value=False):
            result = p.format_shortcut("ctrl+shift+k")
        assert "Ctrl+Shift+K" == result

    @patch("platform.system", return_value="Linux")
    def test_linux_meta_x(self, mock_sys):
        p = Platform()
        with patch.object(Platform, "_is_wsl", return_value=False):
            result = p.format_shortcut("meta+x")
        assert "Alt+X" == result

    @patch("platform.system", return_value="Linux")
    def test_linux_plain_key(self, mock_sys):
        p = Platform()
        with patch.object(Platform, "_is_wsl", return_value=False):
            result = p.format_shortcut("f1")
        assert result == "F1"

    @patch("platform.system", return_value="Darwin")
    def test_case_insensitive_input(self, mock_sys):
        p = Platform()
        result = p.format_shortcut("CTRL+K")
        assert "\u2303" in result
        assert "K" in result


# =============================================================================
# Platform.__repr__
# =============================================================================


class TestPlatformRepr(unittest.TestCase):
    """Tests for Platform.__repr__."""

    @patch("platform.system", return_value="Darwin")
    def test_repr_format(self, mock_sys):
        p = Platform()
        with patch.dict("os.environ", {"TERM_PROGRAM": "kitty"}, clear=True):
            r = repr(p)
        assert "Platform(" in r
        assert "type=MACOS" in r
        assert "terminal=" in r
        assert "graphics=" in r


# =============================================================================
# get_platform_bindings
# =============================================================================


class TestGetPlatformBindings(unittest.TestCase):
    """Tests for the get_platform_bindings function."""

    def test_returns_list(self):
        result = get_platform_bindings()
        assert isinstance(result, list)

    def test_each_binding_is_tuple_of_three(self):
        result = get_platform_bindings()
        for binding in result:
            assert isinstance(binding, tuple)
            assert len(binding) == 3

    def test_contains_command_palette_binding(self):
        result = get_platform_bindings()
        actions = [b[1] for b in result]
        assert "command_palette" in actions

    def test_contains_quit_binding(self):
        result = get_platform_bindings()
        actions = [b[1] for b in result]
        assert "quit" in actions

    def test_descriptions_are_strings(self):
        result = get_platform_bindings()
        for _, _, desc in result:
            assert isinstance(desc, str)
            assert len(desc) > 0


# =============================================================================
# is_wsl_compatibility_needed
# =============================================================================


class TestIsWslCompatibilityNeeded(unittest.TestCase):
    """Tests for is_wsl_compatibility_needed function."""

    def test_returns_bool(self):
        result = is_wsl_compatibility_needed()
        assert isinstance(result, bool)

    @patch("platform.system", return_value="Darwin")
    def test_false_on_macos(self, mock_sys):
        # Use the module-level PLATFORM singleton — we can check via function
        # Since PLATFORM is a singleton, we patch its cached properties
        from tui.core import platform as plat_mod
        original = plat_mod.PLATFORM
        try:
            p = Platform()
            plat_mod.PLATFORM = p
            result = is_wsl_compatibility_needed()
            assert result is False
        finally:
            plat_mod.PLATFORM = original

    def test_wsl_with_windows_terminal(self):
        """WSL + windows-terminal should return True."""
        from tui.core import platform as plat_mod
        original = plat_mod.PLATFORM
        try:
            p = Platform()
            # Set cached properties directly
            p.__dict__["platform_type"] = PlatformType.WSL
            p.__dict__["terminal"] = "windows-terminal"
            plat_mod.PLATFORM = p
            result = is_wsl_compatibility_needed()
            assert result is True
        finally:
            plat_mod.PLATFORM = original

    def test_wsl_without_windows_terminal(self):
        """WSL + non-windows-terminal should return False."""
        from tui.core import platform as plat_mod
        original = plat_mod.PLATFORM
        try:
            p = Platform()
            p.__dict__["platform_type"] = PlatformType.WSL
            p.__dict__["terminal"] = "kitty"
            plat_mod.PLATFORM = p
            result = is_wsl_compatibility_needed()
            assert result is False
        finally:
            plat_mod.PLATFORM = original

    def test_linux_with_windows_terminal(self):
        """Native Linux + windows-terminal should return False (not WSL)."""
        from tui.core import platform as plat_mod
        original = plat_mod.PLATFORM
        try:
            p = Platform()
            p.__dict__["platform_type"] = PlatformType.LINUX
            p.__dict__["terminal"] = "windows-terminal"
            plat_mod.PLATFORM = p
            result = is_wsl_compatibility_needed()
            assert result is False
        finally:
            plat_mod.PLATFORM = original


# =============================================================================
# PLATFORM singleton
# =============================================================================


class TestPlatformSingleton(unittest.TestCase):
    """Tests for the module-level PLATFORM singleton."""

    def test_platform_singleton_exists(self):
        from tui.core.platform import PLATFORM
        assert isinstance(PLATFORM, Platform)

    def test_platform_singleton_has_cached_properties(self):
        from tui.core.platform import PLATFORM
        # Accessing platform_type should work without errors
        pt = PLATFORM.platform_type
        assert isinstance(pt, PlatformType)


if __name__ == "__main__":
    unittest.main()
