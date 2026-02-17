# (c) 2026 Euxis Fleet. All rights reserved.
"""ETX theme definitions.

10 cross-platform themes: 2 Euxis originals + 8 community favorites.
All themes use standard 24-bit hex colors that render consistently
across macOS, Windows, and Linux terminals (Terminal.app, Ghostty,
iTerm2, Windows Terminal, Alacritty, Kitty, etc.).

All color combinations meet a minimum 10:1 contrast ratio.
"""

from __future__ import annotations

from textual.theme import Theme

ETX_THEMES = {
    # ── Euxis Originals ──────────────────────────────────────────────
    "etx-liquid-glass": Theme(
        name="etx-liquid-glass",
        primary="#a1cbff",        # bright blue
        secondary="#cfbfff",      # soft violet
        accent="#a1cbff",
        warning="#fbbf24",        # amber
        error="#ffb5b5",          # rose
        success="#3ce1a4",        # emerald
        surface="#0c0e14",        # deep navy-black
        panel="#1a1d27",          # subtle elevation
        dark=True,
    ),
    "etx-liquid-light": Theme(
        name="etx-liquid-light",
        primary="#092a87",        # deep blue
        secondary="#401688",      # deep violet
        accent="#092a87",
        warning="#592404",        # dark amber
        error="#6c0909",          # deep red
        success="#0b3c1d",        # deep green
        surface="#e5e7eb",        # neutral gray
        panel="#ffffff",          # white
        dark=False,
    ),

    # ── Catppuccin ────────────────────────────────────────────────────
    "etx-catppuccin-mocha": Theme(
        name="etx-catppuccin-mocha",
        primary="#d8e7ff",        # blue
        secondary="#eee0ff",      # mauve
        accent="#d8e7ff",
        warning="#ffe0cd",        # peach
        error="#ffdde6",          # red
        success="#baf6b5",        # green
        surface="#1e1e2e",        # base
        panel="#313244",          # surface0
        dark=True,
    ),
    "etx-catppuccin-latte": Theme(
        name="etx-catppuccin-latte",
        primary="#08318d",        # blue
        secondary="#4d1891",      # mauve
        accent="#08318d",
        warning="#563100",        # dark yellow
        error="#7a001a",          # red
        success="#16430d",        # deep green
        surface="#eff1f5",        # base
        panel="#ffffff",          # white
        dark=False,
    ),

    # ── Tokyo Night ───────────────────────────────────────────────────
    "etx-tokyo-night": Theme(
        name="etx-tokyo-night",
        primary="#c4d7ff",        # blue
        secondary="#e0cfff",      # purple
        accent="#c4d7ff",
        warning="#ffce87",        # yellow
        error="#ffc8d2",          # red
        success="#b4e67f",        # green
        surface="#1a1b26",        # bg
        panel="#24283b",          # bg highlight
        dark=True,
    ),

    # ── Dracula ───────────────────────────────────────────────────────
    "etx-dracula": Theme(
        name="etx-dracula",
        primary="#f6f1ff",        # purple
        secondary="#ffc9e8",      # pink
        accent="#f6f1ff",
        warning="#ffd09d",        # orange
        error="#ffcccc",          # red
        success="#50fa7b",        # green
        surface="#282a36",        # background
        panel="#383a4f",          # current line
        dark=True,
    ),

    # ── Nord ──────────────────────────────────────────────────────────
    "etx-nord": Theme(
        name="etx-nord",
        primary="#ffffff",        # frost white
        secondary="#ffffff",      # aurora white
        accent="#ffffff",
        warning="#ffe3ab",        # aurora yellow
        error="#ffe0d0",          # aurora orange
        success="#fdfffb",        # aurora green
        surface="#2e3440",        # polar night 0
        panel="#3b4252",          # polar night 1
        dark=True,
    ),

    # ── Gruvbox Dark ──────────────────────────────────────────────────
    "etx-gruvbox": Theme(
        name="etx-gruvbox",
        primary="#cee6dc",        # aqua
        secondary="#ffd7e2",      # purple
        accent="#cee6dc",
        warning="#ffdd8e",        # yellow
        error="#ffcba0",          # orange
        success="#e6ea40",        # green
        surface="#282828",        # bg
        panel="#352f2e",          # bg1
        dark=True,
    ),

    # ── Rosé Pine ─────────────────────────────────────────────────────
    "etx-rose-pine": Theme(
        name="etx-rose-pine",
        primary="#a9dbe4",        # foam
        secondary="#e1c8ff",      # iris
        accent="#a9dbe4",
        warning="#fec97e",        # gold
        error="#ffc2d3",          # love
        success="#9adeea",        # pine
        surface="#191724",        # base
        panel="#26233a",          # overlay
        dark=True,
    ),

    # ── Ayu Mirage ────────────────────────────────────────────────────
    "etx-ayu-mirage": Theme(
        name="etx-ayu-mirage",
        primary="#9cdeff",        # blue
        secondary="#ddcdff",      # purple
        accent="#9cdeff",
        warning="#ffd580",        # yellow
        error="#ffc7ca",          # red
        success="#bae67e",        # green
        surface="#1f2430",        # bg
        panel="#232834",          # panel
        dark=True,
    ),
}

# Ordered list for theme cycling (Ctrl+T)
THEME_CYCLE = [
    "etx-liquid-glass",
    "etx-liquid-light",
    "etx-catppuccin-mocha",
    "etx-catppuccin-latte",
    "etx-tokyo-night",
    "etx-dracula",
    "etx-nord",
    "etx-gruvbox",
    "etx-rose-pine",
    "etx-ayu-mirage",
]
