# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Cross-platform compatibility provider for 2026 TUI standards.

Detects the runtime environment (macOS, Linux, WSL) and provides
platform-specific keybindings, modifier keys, and terminal capabilities.

Usage:
    from tui.core.platform import Platform, PLATFORM

    if PLATFORM.is_macos:
        # Use Cmd as primary modifier
    elif PLATFORM.is_wsl:
        # Enable WSL compatibility mode
"""

from __future__ import annotations

import os
import platform
import subprocess
from dataclasses import dataclass
from enum import Enum, auto
from functools import cached_property
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    pass


class PlatformType(Enum):
    """Detected platform type."""
    MACOS = auto()
    LINUX = auto()
    WSL = auto()
    UNKNOWN = auto()


class GraphicsProtocol(Enum):
    """Supported terminal graphics protocols."""
    NONE = auto()
    SIXEL = auto()
    KITTY = auto()
    ITERM2 = auto()


@dataclass(frozen=True)
class KeyMap:
    """Platform-specific key mappings."""
    copy: str
    paste: str
    clear: str
    quit: str
    command_palette: str
    # Meta key name for display
    meta_display: str
    # Actual meta key binding prefix
    meta_key: str


# Platform-specific keymaps
MACOS_KEYMAP = KeyMap(
    copy="cmd+c",
    paste="cmd+v",
    clear="cmd+k",
    quit="cmd+q",
    command_palette="cmd+k",
    meta_display="⌥",
    meta_key="option",
)

LINUX_KEYMAP = KeyMap(
    copy="ctrl+shift+c",
    paste="ctrl+shift+v",
    clear="ctrl+l",
    quit="ctrl+q",
    command_palette="ctrl+k",
    meta_display="Alt",
    meta_key="alt",
)

WSL_KEYMAP = KeyMap(
    copy="ctrl+shift+c",
    paste="ctrl+shift+v",
    clear="ctrl+l",
    quit="ctrl+q",
    command_palette="ctrl+p",  # Avoid Ctrl+K which Windows Terminal may intercept
    meta_display="Alt",
    meta_key="alt",
)


class Platform:
    """Cross-platform environment detector and configuration provider.

    Automatically detects:
    - Operating system (macOS, Linux, WSL)
    - Terminal emulator (iTerm2, Kitty, Windows Terminal, etc.)
    - Graphics protocol support (Sixel, Kitty graphics)
    - Appropriate keybindings for the platform
    """

    def __init__(self) -> None:
        self._platform_type: PlatformType | None = None
        self._terminal: str | None = None

    @cached_property
    def platform_type(self) -> PlatformType:
        """Detect the current platform type."""
        system = platform.system().lower()

        if system == "darwin":
            return PlatformType.MACOS

        if system == "linux":
            # Check for WSL
            if self._is_wsl():
                return PlatformType.WSL
            return PlatformType.LINUX

        return PlatformType.UNKNOWN

    def _is_wsl(self) -> bool:
        """Detect if running inside Windows Subsystem for Linux."""
        # Method 1: Check /proc/version
        try:
            with open("/proc/version", "r") as f:
                version = f.read().lower()
                if "microsoft" in version or "wsl" in version:
                    return True
        except (FileNotFoundError, PermissionError):
            pass

        # Method 2: Check WSL environment variables
        if os.environ.get("WSL_DISTRO_NAME") or os.environ.get("WSLENV"):
            return True

        # Method 3: Check for WSL interop
        if os.path.exists("/proc/sys/fs/binfmt_misc/WSLInterop"):
            return True

        return False

    @cached_property
    def terminal(self) -> str:
        """Detect the terminal emulator."""
        # Check common environment variables
        term_program = os.environ.get("TERM_PROGRAM", "").lower()
        if term_program:
            return term_program

        # Check for specific terminals
        if os.environ.get("KITTY_WINDOW_ID"):
            return "kitty"
        if os.environ.get("ITERM_SESSION_ID"):
            return "iterm2"
        if os.environ.get("WT_SESSION"):
            return "windows-terminal"
        if os.environ.get("ALACRITTY_SOCKET"):
            return "alacritty"
        if os.environ.get("WEZTERM_PANE"):
            return "wezterm"

        # Fallback to TERM
        return os.environ.get("TERM", "unknown")

    @cached_property
    def graphics_protocol(self) -> GraphicsProtocol:
        """Detect supported graphics protocol.

        Priority order (2026 best practice):
        1. Kitty - Best performance, true alpha, zero flicker
        2. iTerm2 - macOS native
        3. Sixel - Broad compatibility fallback
        """
        terminal = self.terminal

        # Kitty graphics protocol - PREFERRED for modern terminals
        # Ghostty, Kitty, WezTerm all support Kitty protocol with superior performance
        if terminal in ("kitty", "ghostty", "wezterm"):
            return GraphicsProtocol.KITTY

        # iTerm2 inline images (macOS)
        if terminal == "iterm2":
            return GraphicsProtocol.ITERM2

        # Sixel fallback for broader compatibility
        if self._supports_sixel():
            return GraphicsProtocol.SIXEL

        return GraphicsProtocol.NONE

    def _supports_sixel(self) -> bool:
        """Check if terminal supports Sixel graphics."""
        # Known Sixel-capable terminals (2026 list)
        sixel_terminals = {
            "mlterm", "xterm", "foot", "wezterm", "contour",
            "ghostty", "rio", "ctx",  # Modern 2026 terminals
        }
        if self.terminal in sixel_terminals:
            return True

        # Check TERM for sixel hint
        term = os.environ.get("TERM", "")
        if "sixel" in term.lower():
            return True

        return False

    @cached_property
    def keymap(self) -> KeyMap:
        """Get platform-appropriate keybindings."""
        if self.platform_type == PlatformType.MACOS:
            return MACOS_KEYMAP
        if self.platform_type == PlatformType.WSL:
            return WSL_KEYMAP
        return LINUX_KEYMAP

    @property
    def is_macos(self) -> bool:
        """Check if running on macOS."""
        return self.platform_type == PlatformType.MACOS

    @property
    def is_linux(self) -> bool:
        """Check if running on native Linux."""
        return self.platform_type == PlatformType.LINUX

    @property
    def is_wsl(self) -> bool:
        """Check if running in WSL."""
        return self.platform_type == PlatformType.WSL

    @property
    def supports_graphics(self) -> bool:
        """Check if terminal supports any graphics protocol."""
        return self.graphics_protocol != GraphicsProtocol.NONE

    @cached_property
    def accent_color(self) -> str:
        """Get platform-appropriate accent color.

        macOS: Uses system accent color approximation
        Linux: Uses distribution-agnostic cyan
        WSL: Uses Windows accent blue
        """
        if self.platform_type == PlatformType.MACOS:
            return "#007AFF"  # macOS blue
        if self.platform_type == PlatformType.WSL:
            return "#0078D4"  # Windows blue
        return "#00D9FF"  # Cyan for Linux

    def get_modifier_display(self, modifier: str) -> str:
        """Get platform-appropriate modifier key display.

        Args:
            modifier: Generic modifier name (meta, ctrl, shift, alt)

        Returns:
            Platform-appropriate display string
        """
        if self.is_macos:
            return {
                "meta": "⌥",
                "ctrl": "⌃",
                "shift": "⇧",
                "alt": "⌥",
                "cmd": "⌘",
                "command": "⌘",
            }.get(modifier.lower(), modifier)

        return {
            "meta": "Alt",
            "ctrl": "Ctrl",
            "shift": "Shift",
            "alt": "Alt",
            "cmd": "Super",
            "command": "Super",
        }.get(modifier.lower(), modifier)

    def format_shortcut(self, shortcut: str) -> str:
        """Format a shortcut string for display on current platform.

        Args:
            shortcut: Generic shortcut like "ctrl+k" or "meta+x"

        Returns:
            Platform-formatted string like "⌃K" (macOS) or "Ctrl+K" (Linux)
        """
        parts = shortcut.lower().split("+")
        formatted = []

        for part in parts:
            if part in ("ctrl", "control"):
                formatted.append(self.get_modifier_display("ctrl"))
            elif part in ("meta", "alt", "option"):
                formatted.append(self.get_modifier_display("meta"))
            elif part in ("shift",):
                formatted.append(self.get_modifier_display("shift"))
            elif part in ("cmd", "command", "super"):
                formatted.append(self.get_modifier_display("cmd"))
            else:
                # Key name - uppercase for display
                formatted.append(part.upper())

        if self.is_macos:
            return "".join(formatted)  # macOS: ⌃⇧K
        return "+".join(formatted)  # Linux: Ctrl+Shift+K

    def __repr__(self) -> str:
        return (
            f"Platform(type={self.platform_type.name}, "
            f"terminal={self.terminal}, "
            f"graphics={self.graphics_protocol.name})"
        )


# Singleton instance for global access
PLATFORM = Platform()


def get_platform_bindings() -> list[tuple[str, str, str]]:
    """Get platform-appropriate Textual bindings.

    Returns:
        List of (key, action, description) tuples
    """
    km = PLATFORM.keymap

    return [
        (km.command_palette, "command_palette", "Commands"),
        (km.quit, "quit", "Quit"),
        # Add more platform-specific bindings as needed
    ]


def is_wsl_compatibility_needed() -> bool:
    """Check if WSL compatibility mode should be enabled.

    Returns True if running in WSL and Windows Terminal is detected,
    which may intercept certain key combinations.
    """
    return PLATFORM.is_wsl and PLATFORM.terminal == "windows-terminal"
