# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Agent avatar widget with graphics protocol support.

Renders high-fidelity pixel-art agent icons using the best available
graphics protocol (Kitty > Sixel > Unicode fallback).

2026 UX Pattern:
- Active agents: Full-color vibrant avatars
- Standby agents: Dimmed/desaturated versions
- Error agents: Red-tinted warning state
"""

from __future__ import annotations

import os
from pathlib import Path
from typing import TYPE_CHECKING, Any

from textual.widget import Widget
from textual.widgets import Static

from tui.core.platform import PLATFORM, GraphicsProtocol

if TYPE_CHECKING:
    from textual.app import ComposeResult

# Try to import graphics libraries (optional dependencies)
_HAS_TEXTUAL_IMAGE = False
_SixelImage = None
_TGPImage = None

try:
    from textual_image.widget import SixelImage as _SixelImage
    from textual_image.widget import TGPImage as _TGPImage  # pragma: no cover
    _HAS_TEXTUAL_IMAGE = True  # pragma: no cover
except ImportError:
    pass


# Unicode fallback icons for when graphics aren't available
# These use Unicode block characters for a pixel-art effect
# Following 2026 "highest-attention color" pattern for status
UNICODE_AVATARS = {
    # Core agents - geometric shapes (high visual weight)
    "arbiter": "[bold cyan]◈[/]",
    "orchestrator": "[bold magenta]◉[/]",
    "guard": "[bold red]◆[/]",
    "auditor": "[bold yellow]◇[/]",
    "librarian": "[bold blue]▣[/]",
    "scribe": "[bold green]▤[/]",
    # Default agents - circles
    "coder": "[cyan]●[/]",
    "reviewer": "[yellow]●[/]",
    "tester": "[green]●[/]",
    "debugger": "[red]●[/]",
    "documenter": "[blue]●[/]",
    "refactorer": "[magenta]●[/]",
    "optimizer": "[cyan]●[/]",
    # On-demand - diamonds
    "analyst": "[magenta]◆[/]",
    "researcher": "[cyan]◆[/]",
    "writer": "[green]◆[/]",
    "marketer": "[yellow]◆[/]",
    "communicator": "[blue]◆[/]",
    # Specialist - stars (expertise indicator)
    "cryptographer": "[yellow]★[/]",
    "security": "[red]★[/]",
    "performance": "[cyan]★[/]",
    "architect": "[magenta]★[/]",
    "dba": "[blue]★[/]",
    # Fallback states
    "_default": "[dim]●[/]",
    "_active": "[bold green]◉[/]",
    "_error": "[bold red on red]⚠[/]",  # High-attention error indicator
}

# Error state icons - highest visual priority (red background for critical)
UNICODE_AVATARS_ERROR = {
    key: f"[bold red]⚠[/]" for key in UNICODE_AVATARS
}
UNICODE_AVATARS_ERROR["_error"] = "[bold red on red]✗[/]"

# Dimmed versions for standby state
UNICODE_AVATARS_DIM = {
    key: f"[dim]{val.split(']')[1].split('[')[0]}[/]"
    for key, val in UNICODE_AVATARS.items()
}


def get_avatar_path(agent_id: str, state: str = "active") -> Path | None:
    """Get the path to an agent's avatar image.

    Args:
        agent_id: The agent identifier
        state: One of "active", "standby", "error"

    Returns:
        Path to the image file, or None if not found
    """
    euxis_home = os.environ.get("EUXIS_HOME", os.path.expanduser("~/.euxis"))
    assets_dir = Path(euxis_home) / "euxis-tui" / "assets" / "agents"

    # Try state-specific image first
    state_path = assets_dir / f"{agent_id}_{state}.png"
    if state_path.exists():
        return state_path

    # Fall back to base image
    base_path = assets_dir / f"{agent_id}.png"
    if base_path.exists():
        return base_path

    # Try generic tier-based fallback
    generic_path = assets_dir / f"_default_{state}.png"
    if generic_path.exists():
        return generic_path

    return None


class AgentAvatar(Widget):
    """A widget for rendering agent avatars with graphics protocol support.

    Automatically selects the best rendering method:
    1. Kitty graphics (TGP) - Best for Ghostty, Kitty terminals
    2. Sixel graphics - Wide terminal support
    3. Unicode fallback - Always works

    The avatar reflects the agent's state through visual treatment:
    - Active: Full brightness, may include animation hint
    - Standby: Dimmed/desaturated
    - Error: Red tint or warning indicator
    """

    DEFAULT_CSS = """
    AgentAvatar {
        width: 3;
        height: 2;
        content-align: center middle;
    }
    """

    def __init__(
        self,
        agent_id: str,
        state: str = "standby",
        **kwargs: Any,
    ) -> None:
        super().__init__(**kwargs)
        self.agent_id = agent_id
        self._state = state
        self._image_widget: Widget | None = None

    def compose(self) -> ComposeResult:
        """Compose the avatar using the best available renderer."""
        # Check if we have image assets and graphics support
        image_path = get_avatar_path(self.agent_id, self._state)

        if image_path and _HAS_TEXTUAL_IMAGE and PLATFORM.supports_graphics:
            # Use graphics-based rendering
            yield self._create_image_widget(image_path)
        else:
            # Fall back to Unicode
            yield self._create_unicode_widget()

    def _create_image_widget(self, image_path: Path) -> Widget:
        """Create a graphics-based image widget."""
        protocol = PLATFORM.graphics_protocol

        if protocol == GraphicsProtocol.KITTY and _TGPImage:
            # Kitty protocol - best for Ghostty
            widget = _TGPImage(image_path)
        elif protocol in (GraphicsProtocol.SIXEL, GraphicsProtocol.KITTY) and _SixelImage:
            # Sixel fallback
            widget = _SixelImage(image_path)
        else:
            # Shouldn't reach here, but safety fallback
            return self._create_unicode_widget()

        self._image_widget = widget
        return widget

    def _create_unicode_widget(self) -> Widget:
        """Create a Unicode-based fallback widget.

        Visual hierarchy (2026 "highest-attention color" pattern):
        - Error: Red warning icon (⚠) - demands immediate attention
        - Active: Full brightness colored icon - operational feedback
        - Standby: Dimmed icon - reduces cognitive load
        """
        if self._state == "error":
            # Error state gets highest visual priority
            icon = UNICODE_AVATARS_ERROR.get(self.agent_id, "[bold red]⚠[/]")
            return Static(icon, classes="avatar-unicode avatar-error")
        elif self._state == "active":
            icons = UNICODE_AVATARS
        else:
            icons = UNICODE_AVATARS_DIM

        icon = icons.get(self.agent_id, icons.get("_default", "●"))
        css_class = "avatar-unicode" + (" avatar-active" if self._state == "active" else "")
        return Static(icon, classes=css_class)

    def set_state(self, state: str) -> None:
        """Update the avatar state (active/standby/error).

        This triggers a re-render with the appropriate visual treatment.
        """
        if state != self._state:
            self._state = state
            # Rebuild the avatar
            self.remove_children()
            image_path = get_avatar_path(self.agent_id, state)

            if image_path and _HAS_TEXTUAL_IMAGE and PLATFORM.supports_graphics:
                new_widget = self._create_image_widget(image_path)
            else:
                new_widget = self._create_unicode_widget()

            self.mount(new_widget)


class CompactAvatar(Static):
    """A minimal 1-character avatar for tight spaces.

    Uses Unicode symbols that represent agent "personality":
    - Core: Geometric (◈ ◉ ◆)
    - Default: Circles (● ○)
    - Specialist: Stars (★ ☆)

    Visual states follow 2026 "highest-attention" pattern:
    - Error (red ⚠) > Active (bright) > Standby (dim)
    """

    def __init__(
        self,
        agent_id: str,
        active: bool = False,
        error: bool = False,
        **kwargs: Any,
    ) -> None:
        icon = self._get_icon(agent_id, active, error)
        super().__init__(icon, **kwargs)
        self.agent_id = agent_id
        self._active = active
        self._error = error

    def _get_icon(self, agent_id: str, active: bool, error: bool) -> str:
        """Get the appropriate icon based on state hierarchy."""
        if error:
            return UNICODE_AVATARS_ERROR.get(agent_id, "[bold red]⚠[/]")
        elif active:
            return UNICODE_AVATARS.get(agent_id, UNICODE_AVATARS.get("_default", "●"))
        else:
            return UNICODE_AVATARS_DIM.get(agent_id, UNICODE_AVATARS_DIM.get("_default", "●"))

    def set_active(self, active: bool) -> None:
        """Toggle between active and standby visual state."""
        if active != self._active:
            self._active = active
            self._error = False  # Active clears error
            self.update(self._get_icon(self.agent_id, active, False))

    def set_error(self, error: bool = True) -> None:
        """Set error state (highest visual priority)."""
        if error != self._error:
            self._error = error
            self.update(self._get_icon(self.agent_id, self._active, error))

    def set_state(self, state: str) -> None:
        """Set state by name: 'active', 'standby', 'error'."""
        if state == "error":
            self._error = True
            self._active = False
        elif state == "active":
            self._error = False
            self._active = True
        else:  # standby
            self._error = False
            self._active = False
        self.update(self._get_icon(self.agent_id, self._active, self._error))
