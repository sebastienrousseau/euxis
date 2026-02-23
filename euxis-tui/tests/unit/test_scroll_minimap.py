# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Comprehensive unit tests for ScrollMinimap widget.

Tests cover: construction with reactive defaults, render() with various
heights and viewport/active-position configurations, update_viewport()
edge cases and normal values, set_active_positions() tuple conversion.
"""

from __future__ import annotations

import unittest
from unittest.mock import MagicMock, PropertyMock, patch

from tui.widgets.scroll_minimap import ScrollMinimap


class TestScrollMinimapInit(unittest.TestCase):
    """Tests for ScrollMinimap construction and reactive defaults."""

    def test_construction(self):
        """Test ScrollMinimap can be instantiated."""
        widget = ScrollMinimap()
        assert isinstance(widget, ScrollMinimap)

    def test_default_viewport_start(self):
        """Test default viewport_start reactive value is 0.0."""
        descriptor = ScrollMinimap.__dict__["viewport_start"]
        assert descriptor._default == 0.0

    def test_default_viewport_size(self):
        """Test default viewport_size reactive value is 0.2."""
        descriptor = ScrollMinimap.__dict__["viewport_size"]
        assert descriptor._default == 0.2

    def test_default_active_positions(self):
        """Test default active_positions reactive value is empty tuple."""
        descriptor = ScrollMinimap.__dict__["active_positions"]
        assert descriptor._default == ()

    def test_css_is_defined(self):
        """Test DEFAULT_CSS is a non-empty string."""
        assert isinstance(ScrollMinimap.DEFAULT_CSS, str)
        assert len(ScrollMinimap.DEFAULT_CSS) > 0

    def test_css_contains_width(self):
        """Test CSS sets width to 2."""
        assert "width: 2" in ScrollMinimap.DEFAULT_CSS

    def test_css_docks_right(self):
        """Test CSS docks minimap to the right."""
        assert "dock: right" in ScrollMinimap.DEFAULT_CSS

    def test_construction_with_kwargs(self):
        """Test construction passes kwargs to super."""
        widget = ScrollMinimap(id="minimap")
        assert widget.id == "minimap"


class TestScrollMinimapRenderHeightZero(unittest.TestCase):
    """Tests for render() when height is 0 or less."""

    def test_render_height_zero(self):
        """Test render returns empty string when height < 1."""
        widget = ScrollMinimap()
        mock_size = MagicMock()
        mock_size.height = 0
        widget._size = mock_size
        type(widget).size = PropertyMock(return_value=mock_size)
        result = widget.render()
        assert result == ""

    def test_render_height_negative(self):
        """Test render returns empty string when height is negative."""
        widget = ScrollMinimap()
        mock_size = MagicMock()
        mock_size.height = -1
        type(widget).size = PropertyMock(return_value=mock_size)
        result = widget.render()
        assert result == ""


class TestScrollMinimapRenderHeightOne(unittest.TestCase):
    """Tests for render() when height is 1."""

    def _make_widget(self, viewport_start=0.0, viewport_size=0.2, active_positions=()):
        widget = ScrollMinimap()
        mock_size = MagicMock()
        mock_size.height = 1
        type(widget).size = PropertyMock(return_value=mock_size)
        # Set reactive values directly via internal storage
        widget._reactive_viewport_start = viewport_start
        widget._reactive_viewport_size = viewport_size
        widget._reactive_active_positions = active_positions
        return widget

    def test_render_height_one_viewport_covers_line(self):
        """Test render with height=1, viewport covering the single line."""
        widget = self._make_widget(viewport_start=0.0, viewport_size=1.0)
        result = widget.render()
        # Line 0: vp_start=0, vp_end=1, so 0 is in viewport
        assert "[on $panel]" in result

    def test_render_height_one_active_at_zero(self):
        """Test render with height=1 and active agent at position 0.0."""
        widget = self._make_widget(active_positions=(0.0,))
        result = widget.render()
        # Active line takes priority over viewport
        assert "[bold green]" in result


class TestScrollMinimapRenderHeightTen(unittest.TestCase):
    """Tests for render() with height=10 for detailed line checks."""

    def _make_widget(self, viewport_start=0.0, viewport_size=0.2, active_positions=()):
        widget = ScrollMinimap()
        mock_size = MagicMock()
        mock_size.height = 10
        type(widget).size = PropertyMock(return_value=mock_size)
        widget._reactive_viewport_start = viewport_start
        widget._reactive_viewport_size = viewport_size
        widget._reactive_active_positions = active_positions
        return widget

    def test_render_produces_ten_lines(self):
        """Test render produces exactly 10 lines for height=10."""
        widget = self._make_widget()
        result = widget.render()
        lines = result.split("\n")
        assert len(lines) == 10

    def test_viewport_indicator_positioning(self):
        """Test viewport indicator lines are in the correct range."""
        # viewport_start=0.3, viewport_size=0.2, height=10
        # vp_start_line = int(0.3*10) = 3, vp_end_line = int(0.5*10) = 5
        widget = self._make_widget(viewport_start=0.3, viewport_size=0.2)
        result = widget.render()
        lines = result.split("\n")

        # Lines 0-2 should be dim (outside viewport)
        for i in range(3):
            assert "[dim]" in lines[i], f"Line {i} should be dim"

        # Lines 3-5 should be viewport indicators
        for i in range(3, 6):
            assert "[on $panel]" in lines[i], f"Line {i} should be viewport"

        # Lines 6-9 should be dim
        for i in range(6, 10):
            assert "[dim]" in lines[i], f"Line {i} should be dim"

    def test_active_markers_at_correct_positions(self):
        """Test active agent markers appear at correct lines."""
        # Agents at positions 0.1 (line 1) and 0.7 (line 7)
        widget = self._make_widget(
            viewport_start=0.0,
            viewport_size=0.3,
            active_positions=(0.1, 0.7),
        )
        result = widget.render()
        lines = result.split("\n")

        assert "[bold green]" in lines[1], "Agent at 0.1 should be on line 1"
        assert "[bold green]" in lines[7], "Agent at 0.7 should be on line 7"

    def test_active_marker_overrides_viewport(self):
        """Test active markers take priority over viewport indicator."""
        # Viewport covers 0.0-0.5, agent at 0.2 (line 2)
        widget = self._make_widget(
            viewport_start=0.0,
            viewport_size=0.5,
            active_positions=(0.2,),
        )
        result = widget.render()
        lines = result.split("\n")

        # Line 2 should show active marker, not viewport
        assert "[bold green]" in lines[2]
        assert "[on $panel]" not in lines[2]

    def test_lines_outside_viewport_are_dim(self):
        """Test lines outside the viewport are rendered as dim."""
        widget = self._make_widget(viewport_start=0.4, viewport_size=0.1)
        result = widget.render()
        lines = result.split("\n")

        # Lines far from viewport should be dim
        assert "[dim]" in lines[0]
        assert "[dim]" in lines[9]

    def test_full_viewport_all_lines_highlighted(self):
        """Test all lines are viewport when viewport covers entire height."""
        widget = self._make_widget(viewport_start=0.0, viewport_size=1.0)
        result = widget.render()
        lines = result.split("\n")

        for i, line in enumerate(lines):
            assert "[on $panel]" in line, f"Line {i} should be viewport"

    def test_no_active_positions(self):
        """Test render with no active positions produces no green markers."""
        widget = self._make_widget(active_positions=())
        result = widget.render()
        assert "[bold green]" not in result

    def test_multiple_active_positions(self):
        """Test multiple active positions at various locations."""
        widget = self._make_widget(active_positions=(0.0, 0.3, 0.5, 0.9))
        result = widget.render()
        lines = result.split("\n")

        # 0.0 -> line 0, 0.3 -> line 3, 0.5 -> line 5, 0.9 -> line 9
        for expected_line in [0, 3, 5, 9]:
            assert "[bold green]" in lines[expected_line]


class TestScrollMinimapUpdateViewport(unittest.TestCase):
    """Tests for update_viewport() method."""

    def test_content_height_zero(self):
        """Test update_viewport with content_height=0 defaults to full viewport."""
        widget = ScrollMinimap()
        widget.update_viewport(scroll_y=0, max_scroll=0, viewport_height=100, content_height=0)
        assert widget.viewport_start == 0.0
        assert widget.viewport_size == 1.0

    def test_max_scroll_zero(self):
        """Test update_viewport with max_scroll=0 defaults to full viewport."""
        widget = ScrollMinimap()
        widget.update_viewport(scroll_y=0, max_scroll=0, viewport_height=100, content_height=100)
        assert widget.viewport_start == 0.0
        assert widget.viewport_size == 1.0

    def test_content_height_negative(self):
        """Test update_viewport with negative content_height defaults."""
        widget = ScrollMinimap()
        widget.update_viewport(scroll_y=0, max_scroll=10, viewport_height=100, content_height=-5)
        assert widget.viewport_start == 0.0
        assert widget.viewport_size == 1.0

    def test_max_scroll_negative(self):
        """Test update_viewport with negative max_scroll defaults."""
        widget = ScrollMinimap()
        widget.update_viewport(scroll_y=0, max_scroll=-1, viewport_height=100, content_height=200)
        assert widget.viewport_start == 0.0
        assert widget.viewport_size == 1.0

    def test_normal_values_start(self):
        """Test update_viewport sets viewport_start correctly."""
        widget = ScrollMinimap()
        # scroll_y=100, content_height=500 -> viewport_start = 0.2
        widget.update_viewport(scroll_y=100, max_scroll=400, viewport_height=100, content_height=500)
        assert widget.viewport_start == 0.2

    def test_normal_values_size(self):
        """Test update_viewport sets viewport_size correctly."""
        widget = ScrollMinimap()
        # viewport_height=100, content_height=500 -> viewport_size = 0.2
        widget.update_viewport(scroll_y=100, max_scroll=400, viewport_height=100, content_height=500)
        assert widget.viewport_size == 0.2

    def test_viewport_size_capped_at_one(self):
        """Test viewport_size is capped at 1.0 even if viewport >= content."""
        widget = ScrollMinimap()
        # viewport_height=600 > content_height=500 -> min(1.0, 1.2) = 1.0
        widget.update_viewport(scroll_y=0, max_scroll=1, viewport_height=600, content_height=500)
        assert widget.viewport_size == 1.0

    def test_scrolled_to_bottom(self):
        """Test update_viewport at the bottom of content."""
        widget = ScrollMinimap()
        # scroll_y=400, content_height=500 -> viewport_start = 0.8
        widget.update_viewport(scroll_y=400, max_scroll=400, viewport_height=100, content_height=500)
        assert widget.viewport_start == 0.8

    def test_half_scrolled(self):
        """Test update_viewport at the midpoint."""
        widget = ScrollMinimap()
        widget.update_viewport(scroll_y=250, max_scroll=500, viewport_height=100, content_height=600)
        expected_start = 250 / 600
        expected_size = 100 / 600
        self.assertAlmostEqual(widget.viewport_start, expected_start)
        self.assertAlmostEqual(widget.viewport_size, expected_size)


class TestScrollMinimapSetActivePositions(unittest.TestCase):
    """Tests for set_active_positions() method."""

    def test_set_active_positions_converts_to_tuple(self):
        """Test set_active_positions converts list to tuple."""
        widget = ScrollMinimap()
        widget.set_active_positions([0.1, 0.5, 0.9])
        assert widget.active_positions == (0.1, 0.5, 0.9)

    def test_set_active_positions_empty_list(self):
        """Test set_active_positions with empty list."""
        widget = ScrollMinimap()
        widget.set_active_positions([])
        assert widget.active_positions == ()

    def test_set_active_positions_single_position(self):
        """Test set_active_positions with single position."""
        widget = ScrollMinimap()
        widget.set_active_positions([0.5])
        assert widget.active_positions == (0.5,)

    def test_set_active_positions_result_is_tuple(self):
        """Test set_active_positions result type is tuple."""
        widget = ScrollMinimap()
        widget.set_active_positions([0.1, 0.2])
        assert isinstance(widget.active_positions, tuple)

    def test_set_active_positions_boundary_values(self):
        """Test set_active_positions with boundary values 0.0 and 1.0."""
        widget = ScrollMinimap()
        widget.set_active_positions([0.0, 1.0])
        assert widget.active_positions == (0.0, 1.0)

    def test_set_active_positions_overwrites_previous(self):
        """Test set_active_positions replaces previous positions."""
        widget = ScrollMinimap()
        widget.set_active_positions([0.1, 0.2, 0.3])
        widget.set_active_positions([0.7])
        assert widget.active_positions == (0.7,)


if __name__ == "__main__":
    unittest.main()
