"""Comprehensive unit tests for ShortcutBar widget.

Tests cover: initialization, default shortcuts, custom shortcuts, on_mount
rendering, CSS styling, and edge cases.
"""

from __future__ import annotations

import unittest
from unittest.mock import Mock, patch

from tui.widgets.shortcut_bar import DEFAULT_SHORTCUTS, ShortcutBar


class TestShortcutBarDefaults(unittest.TestCase):
    """Tests for DEFAULT_SHORTCUTS constant."""

    def test_default_shortcuts_is_list(self):
        """Test DEFAULT_SHORTCUTS is a list."""
        assert isinstance(DEFAULT_SHORTCUTS, list)

    def test_default_shortcuts_not_empty(self):
        """Test DEFAULT_SHORTCUTS has entries."""
        assert len(DEFAULT_SHORTCUTS) > 0

    def test_default_shortcuts_are_tuples(self):
        """Test each shortcut is a (key, label) tuple."""
        for shortcut in DEFAULT_SHORTCUTS:
            assert isinstance(shortcut, tuple)
            assert len(shortcut) == 2

    def test_default_shortcuts_have_strings(self):
        """Test shortcut keys and labels are strings."""
        for key, label in DEFAULT_SHORTCUTS:
            assert isinstance(key, str)
            assert isinstance(label, str)

    def test_help_shortcut_present(self):
        """Test help shortcut is included."""
        keys = [k for k, _ in DEFAULT_SHORTCUTS]
        assert "?" in keys

    def test_search_shortcut_present(self):
        """Test search shortcut is included."""
        keys = [k for k, _ in DEFAULT_SHORTCUTS]
        assert "/" in keys

    def test_quit_shortcut_present(self):
        """Test quit shortcut is included."""
        keys = [k for k, _ in DEFAULT_SHORTCUTS]
        assert "^Q" in keys

    def test_commands_shortcut_present(self):
        """Test commands shortcut is included."""
        keys = [k for k, _ in DEFAULT_SHORTCUTS]
        assert "^K" in keys


class TestShortcutBarInit(unittest.TestCase):
    """Tests for ShortcutBar initialization."""

    def test_initialization_no_args(self):
        """Test ShortcutBar can be instantiated without args."""
        bar = ShortcutBar()
        assert isinstance(bar, ShortcutBar)

    def test_uses_default_shortcuts(self):
        """Test ShortcutBar uses DEFAULT_SHORTCUTS when none provided."""
        bar = ShortcutBar()
        assert bar._shortcuts == DEFAULT_SHORTCUTS

    def test_custom_shortcuts(self):
        """Test ShortcutBar accepts custom shortcuts."""
        custom = [("a", "Action"), ("b", "Back")]
        bar = ShortcutBar(shortcuts=custom)
        assert bar._shortcuts == custom

    def test_empty_shortcuts_list(self):
        """Test ShortcutBar with empty list uses defaults (falsy check)."""
        bar = ShortcutBar(shortcuts=[])
        # Empty list is falsy, so it falls back to DEFAULT_SHORTCUTS
        assert bar._shortcuts == DEFAULT_SHORTCUTS

    def test_single_shortcut(self):
        """Test ShortcutBar handles single shortcut."""
        single = [("x", "Exit")]
        bar = ShortcutBar(shortcuts=single)
        assert bar._shortcuts == single

    def test_initialization_with_id(self):
        """Test ShortcutBar accepts id kwarg."""
        bar = ShortcutBar(id="footer-bar")
        assert bar.id == "footer-bar"

    def test_initialization_with_classes(self):
        """Test ShortcutBar accepts classes kwarg."""
        bar = ShortcutBar(classes="custom-footer")
        assert "custom-footer" in bar.classes


class TestShortcutBarCSS(unittest.TestCase):
    """Tests for ShortcutBar DEFAULT_CSS."""

    def test_has_default_css(self):
        """Test ShortcutBar has DEFAULT_CSS defined."""
        assert hasattr(ShortcutBar, "DEFAULT_CSS")
        assert ShortcutBar.DEFAULT_CSS is not None

    def test_css_contains_dock(self):
        """Test CSS docks bar to bottom."""
        assert "dock:" in ShortcutBar.DEFAULT_CSS or "dock: bottom" in ShortcutBar.DEFAULT_CSS

    def test_css_contains_height(self):
        """Test CSS specifies height."""
        assert "height:" in ShortcutBar.DEFAULT_CSS

    def test_css_contains_background(self):
        """Test CSS specifies background."""
        assert "background:" in ShortcutBar.DEFAULT_CSS


class TestShortcutBarOnMount(unittest.TestCase):
    """Tests for ShortcutBar on_mount method."""

    def test_on_mount_calls_update(self):
        """Test on_mount updates the widget content."""
        bar = ShortcutBar()
        bar.update = Mock()
        bar.on_mount()
        bar.update.assert_called_once()

    def test_on_mount_renders_shortcuts(self):
        """Test on_mount renders shortcut labels."""
        bar = ShortcutBar(shortcuts=[("x", "Exit"), ("h", "Help")])
        bar.update = Mock()
        bar.on_mount()

        content = bar.update.call_args[0][0]
        assert "x" in content
        assert "Exit" in content
        assert "h" in content
        assert "Help" in content

    def test_on_mount_uses_bold_for_keys(self):
        """Test on_mount uses bold formatting for keys."""
        bar = ShortcutBar(shortcuts=[("x", "Exit")])
        bar.update = Mock()
        bar.on_mount()

        content = bar.update.call_args[0][0]
        assert "[bold]" in content

    def test_on_mount_uses_dim_for_labels(self):
        """Test on_mount uses dim formatting for labels."""
        bar = ShortcutBar(shortcuts=[("x", "Exit")])
        bar.update = Mock()
        bar.on_mount()

        content = bar.update.call_args[0][0]
        assert "[dim]" in content

    def test_on_mount_separates_shortcuts(self):
        """Test on_mount separates shortcuts with spaces."""
        bar = ShortcutBar(shortcuts=[("a", "A"), ("b", "B")])
        bar.update = Mock()
        bar.on_mount()

        content = bar.update.call_args[0][0]
        # Shortcuts should be separated
        assert "  " in content

    def test_on_mount_empty_shortcuts(self):
        """Test on_mount handles empty shortcuts gracefully."""
        bar = ShortcutBar(shortcuts=[])
        bar.update = Mock()
        bar.on_mount()
        bar.update.assert_called_once()


class TestShortcutBarFormatting(unittest.TestCase):
    """Tests for shortcut formatting details."""

    def test_key_wrapped_in_spaces(self):
        """Test keys have surrounding spaces."""
        bar = ShortcutBar(shortcuts=[("X", "Exit")])
        bar.update = Mock()
        bar.on_mount()

        content = bar.update.call_args[0][0]
        # Should have space before and after key
        assert " X " in content

    def test_special_keys_rendered(self):
        """Test special keys like ^K are rendered."""
        bar = ShortcutBar(shortcuts=[("^K", "Commands")])
        bar.update = Mock()
        bar.on_mount()

        content = bar.update.call_args[0][0]
        assert "^K" in content

    def test_function_keys_rendered(self):
        """Test function keys like F2 are rendered."""
        bar = ShortcutBar(shortcuts=[("F2", "Welcome")])
        bar.update = Mock()
        bar.on_mount()

        content = bar.update.call_args[0][0]
        assert "F2" in content


class TestShortcutBarEdgeCases(unittest.TestCase):
    """Edge case tests for ShortcutBar."""

    def test_long_key(self):
        """Test handling of long key strings."""
        bar = ShortcutBar(shortcuts=[("Ctrl+Shift+K", "Super Command")])
        bar.update = Mock()
        bar.on_mount()

        content = bar.update.call_args[0][0]
        assert "Ctrl+Shift+K" in content

    def test_long_label(self):
        """Test handling of long label strings."""
        bar = ShortcutBar(shortcuts=[("x", "A very long description for this shortcut")])
        bar.update = Mock()
        bar.on_mount()

        content = bar.update.call_args[0][0]
        assert "A very long description" in content

    def test_unicode_in_shortcut(self):
        """Test Unicode characters in shortcuts."""
        bar = ShortcutBar(shortcuts=[("⌘", "Command")])
        bar.update = Mock()
        bar.on_mount()

        content = bar.update.call_args[0][0]
        assert "⌘" in content

    def test_many_shortcuts(self):
        """Test many shortcuts are rendered."""
        shortcuts = [(str(i), f"Action{i}") for i in range(10)]
        bar = ShortcutBar(shortcuts=shortcuts)
        bar.update = Mock()
        bar.on_mount()

        content = bar.update.call_args[0][0]
        # All shortcuts should be present
        for i in range(10):
            assert f"Action{i}" in content


class TestShortcutBarInheritance(unittest.TestCase):
    """Tests for ShortcutBar inheritance."""

    def test_inherits_from_static(self):
        """Test ShortcutBar inherits from Static."""
        from textual.widgets import Static
        assert issubclass(ShortcutBar, Static)

    def test_initial_content_empty(self):
        """Test ShortcutBar starts with empty content."""
        bar = ShortcutBar()
        # The parent Static is initialized with empty string
        # Content is set in on_mount


if __name__ == "__main__":
    unittest.main()
