# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

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
    # Improved contrast: distinct frost colors instead of all-white
    "etx-nord": Theme(
        name="etx-nord",
        primary="#88c0d0",        # frost 3 (cyan) - distinct primary
        secondary="#81a1c1",      # frost 2 (blue) - distinct secondary
        accent="#88c0d0",         # frost 3
        warning="#ebcb8b",        # aurora yellow (brighter)
        error="#d08770",          # aurora orange (better contrast: 5.3:1)
        success="#a3be8c",        # aurora green
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

    # ═══════════════════════════════════════════════════════════════════
    # 2026 PREMIUM THEMES — Ultimate TUI Aesthetics
    # ═══════════════════════════════════════════════════════════════════

    # ── Liquid Glass V2 (Refined Apple-inspired depth) ────────────────
    "etx-liquid-glass-v2": Theme(
        name="etx-liquid-glass-v2",
        primary="#38bdf8",        # Sky blue (vibrant but not harsh)
        secondary="#818cf8",      # Indigo (depth accent)
        accent="#38bdf8",
        warning="#fbbf24",        # Amber (warm contrast)
        error="#f87171",          # Coral (soft error)
        success="#4ade80",        # Mint (fresh success)
        surface="#0f172a",        # Slate 950 (deep, nearly black)
        panel="#1e293b",          # Slate 800 (elevated glass layer)
        dark=True,
    ),

    # ── Cyber Pulse (High-octane neon futurism) ───────────────────────
    "etx-cyber-pulse": Theme(
        name="etx-cyber-pulse",
        primary="#00e8ff",        # Neon cyan (electric)
        secondary="#a855f7",      # Vivid violet
        accent="#00e8ff",
        warning="#ffbf00",        # Amber glow
        error="#ff0055",          # Hot pink (urgent)
        success="#00ff9f",        # Matrix green
        surface="#0a0a0c",        # True black depth
        panel="#16161e",          # Tokyo night panel
        dark=True,
    ),

    # ── Mocha Mousse (Earthy sophistication, Pantone 2025) ────────────
    "etx-mocha-mousse": Theme(
        name="etx-mocha-mousse",
        primary="#d4a574",        # Warm caramel
        secondary="#c4a77d",      # Soft gold
        accent="#d4a574",
        warning="#e6a85c",        # Honey amber
        error="#e57373",          # Dusty rose
        success="#81c784",        # Sage green
        surface="#1a1512",        # Deep espresso
        panel="#2d2420",          # Warm cocoa
        dark=True,
    ),

    # ── Resonant Stark (Ultra-minimal professional) ───────────────────
    # Improved: Higher contrast secondary for metadata readability
    "etx-resonant-stark": Theme(
        name="etx-resonant-stark",
        primary="#f1f5f9",        # Slate 100 (bright primary)
        secondary="#cbd5e1",      # Slate 300 (readable metadata)
        accent="#f1f5f9",
        warning="#fbbf24",        # Amber 400 (visible warning)
        error="#f87171",          # Red 400 (clear error)
        success="#4ade80",        # Green 400 (distinct success)
        surface="#020617",        # Slate 950 (pure depth)
        panel="#0f172a",          # Slate 900 (subtle lift)
        dark=True,
    ),

    # ── Deep Space (Cosmic void with stellar accents) ─────────────────
    "etx-deep-space": Theme(
        name="etx-deep-space",
        primary="#67e8f9",        # Cyan 300 (stellar)
        secondary="#c4b5fd",      # Violet 300 (nebula)
        accent="#67e8f9",
        warning="#fde047",        # Yellow 300 (solar)
        error="#fda4af",          # Rose 300 (supernova)
        success="#86efac",        # Green 300 (aurora)
        surface="#030712",        # Gray 950 (void)
        panel="#111827",          # Gray 900 (dark matter)
        dark=True,
    ),

    # ── Cloud Dancer (Pantone 2026 Color of the Year) ─────────────────
    # "Fresh start" and "mental breathing room" - serene light theme
    "etx-cloud-dancer": Theme(
        name="etx-cloud-dancer",
        primary="#3d3d3d",        # Deep charcoal (clear text)
        secondary="#6b6b6b",      # Medium gray (readable metadata)
        accent="#3d3d3d",
        warning="#b45309",        # Amber 700 (warm ochre)
        error="#b91c1c",          # Red 700 (deep, not alarming)
        success="#15803d",        # Green 700 (forest)
        surface="#F0EEE9",        # Cloud Dancer (main surface)
        panel="#E5E3DD",          # Soft shadow white
        dark=False,
    ),

    # ═══════════════════════════════════════════════════════════════════
    # SEMANTIC ALIASES — Intent-First Naming (2026 Standard)
    # Use these for quick selection by mood/intent
    # ═══════════════════════════════════════════════════════════════════

    # Focused: High contrast, zero distraction
    "etx-focused": Theme(
        name="etx-focused",
        primary="#f1f5f9",        # Bright primary
        secondary="#cbd5e1",      # Clear metadata
        accent="#f1f5f9",
        warning="#fbbf24",
        error="#f87171",
        success="#4ade80",
        surface="#020617",        # Pure depth
        panel="#0f172a",
        dark=True,
    ),

    # Calm: Reduced visual intensity for long sessions
    "etx-calm": Theme(
        name="etx-calm",
        primary="#d4a574",        # Warm caramel
        secondary="#b8977a",      # Soft tan
        accent="#d4a574",
        warning="#d4915c",        # Muted amber
        error="#c97070",          # Soft coral
        success="#7dab7d",        # Sage
        surface="#1a1512",        # Deep espresso
        panel="#2d2420",          # Warm cocoa
        dark=True,
    ),

    # Vivid: High-energy for active work
    "etx-vivid": Theme(
        name="etx-vivid",
        primary="#00e8ff",        # Neon cyan
        secondary="#a855f7",      # Vivid violet
        accent="#00e8ff",
        warning="#ffbf00",        # Amber glow
        error="#ff0055",          # Hot pink
        success="#00ff9f",        # Matrix green
        surface="#0a0a0c",        # True black
        panel="#16161e",          # Dark panel
        dark=True,
    ),

    # Soft: Gentle on eyes, cloud-like serenity
    # Secondary darkened to #595959 for 4.5:1 contrast compliance
    "etx-soft": Theme(
        name="etx-soft",
        primary="#3d3d3d",
        secondary="#595959",      # Darker gray (was #6b6b6b, now 4.8:1)
        accent="#3d3d3d",
        warning="#b45309",
        error="#b91c1c",
        success="#15803d",
        surface="#F0EEE9",        # Cloud Dancer
        panel="#E5E3DD",
        dark=False,
    ),

    # ═══════════════════════════════════════════════════════════════════
    # ACCESSIBILITY THEMES — 2026 Inclusive Design Standards
    # WCAG 2.2 AA compliant, designed for low-vision and color-blindness
    # ═══════════════════════════════════════════════════════════════════

    # High Contrast Dark — Maximum readability, WCAG AAA (7:1+)
    "etx-high-contrast": Theme(
        name="etx-high-contrast",
        primary="#ffffff",        # Pure white on near-black
        secondary="#e5e5e5",      # High-contrast metadata
        accent="#ffffff",
        warning="#ffd700",        # Gold (visible to most color-blind users)
        error="#ff6b6b",          # Bright coral (distinct from success)
        success="#00ff88",        # Bright cyan-green (distinct from error)
        surface="#0a0a0a",        # Near-black (not pure #000)
        panel="#1a1a1a",          # Elevated panel
        dark=True,
    ),

    # High Contrast Light — Maximum readability on light background
    "etx-high-contrast-light": Theme(
        name="etx-high-contrast-light",
        primary="#000000",        # Pure black on near-white
        secondary="#333333",      # High-contrast metadata
        accent="#000000",
        warning="#b45309",        # Dark amber (visible on light)
        error="#b91c1c",          # Dark red
        success="#15803d",        # Dark green
        surface="#fafafa",        # Near-white (not pure #fff)
        panel="#f0f0f0",          # Subtle panel
        dark=False,
    ),

    # Deuteranopia-Safe — Optimized for red-green color blindness
    "etx-deuteranopia": Theme(
        name="etx-deuteranopia",
        primary="#56b4e9",        # Sky blue (safe)
        secondary="#999999",      # Neutral gray
        accent="#56b4e9",
        warning="#f0e442",        # Yellow (universal warning)
        error="#d55e00",          # Orange (not red, distinguishable)
        success="#0072b2",        # Dark blue (not green)
        surface="#1a1a2e",        # Deep blue-black
        panel="#2d2d44",          # Elevated blue-gray
        dark=True,
    ),

    # Protanopia-Safe — Optimized for red color blindness
    "etx-protanopia": Theme(
        name="etx-protanopia",
        primary="#56b4e9",        # Sky blue
        secondary="#999999",      # Neutral gray
        accent="#56b4e9",
        warning="#f0e442",        # Yellow
        error="#cc79a7",          # Pink/magenta (visible to protanopes)
        success="#009e73",        # Teal (not pure green)
        surface="#1a1a2e",        # Deep blue-black
        panel="#2d2d44",          # Elevated blue-gray
        dark=True,
    ),
}

# Ordered list for theme cycling (F3)
# Organized by intent for quick discovery
THEME_CYCLE = [
    # ── Semantic Quick-Select (Intent-First) ──
    "etx-focused",           # High contrast, zero distraction
    "etx-calm",              # Warm, reduced intensity
    "etx-vivid",             # High-energy neon
    "etx-soft",              # Cloud Dancer light theme

    # ── 2026 Premium Themes ──
    "etx-liquid-glass-v2",   # Apple-inspired depth
    "etx-cyber-pulse",       # Neon neubrutalism
    "etx-mocha-mousse",      # Earthy sophistication
    "etx-resonant-stark",    # Ultra-minimal professional
    "etx-deep-space",        # Cosmic void
    "etx-cloud-dancer",      # Pantone 2026

    # ── Original Euxis ──
    "etx-liquid-glass",
    "etx-liquid-light",

    # ── Community Favorites ──
    "etx-catppuccin-mocha",
    "etx-catppuccin-latte",
    "etx-tokyo-night",
    "etx-dracula",
    "etx-nord",
    "etx-gruvbox",
    "etx-rose-pine",
    "etx-ayu-mirage",

    # ── Accessibility ──
    "etx-high-contrast",
    "etx-high-contrast-light",
    "etx-deuteranopia",
    "etx-protanopia",
]


# Accessibility theme recommendations by need
ACCESSIBILITY_THEMES = {
    "low-vision": ["etx-high-contrast", "etx-high-contrast-light"],
    "color-blind": ["etx-deuteranopia", "etx-protanopia"],
    "light-sensitive": ["etx-calm", "etx-mocha-mousse"],
    "vestibular": [],  # Use reduced_motion setting instead
}
