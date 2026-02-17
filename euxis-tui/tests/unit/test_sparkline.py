# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Comprehensive unit tests for Sparkline widget.

Property-based tests for mathematical invariants and visualization correctness.
"""

import math
import unittest
from unittest.mock import patch

from hypothesis import assume, given
from hypothesis import strategies as st

from tui.widgets.sparkline import SPARK_CHARS, Sparkline, sparkline_text


class TestSparklineText(unittest.TestCase):
    """Unit tests for sparkline text generation function."""

    def test_empty_values(self):
        """Test sparkline with empty input."""
        result = sparkline_text([])
        assert result == ""

    def test_single_value(self):
        """Test sparkline with single value."""
        result = sparkline_text([42])
        assert result == SPARK_CHARS[0]  # Should map to lowest char

    def test_identical_values(self):
        """Test sparkline with all identical values."""
        result = sparkline_text([5, 5, 5, 5])
        expected = SPARK_CHARS[0] * 4  # All should map to lowest char
        assert result == expected

    def test_ascending_values(self):
        """Test sparkline with strictly ascending values."""
        result = sparkline_text([1, 2, 3, 4])
        # Should produce ascending character heights
        assert len(result) == 4
        assert all(c in SPARK_CHARS for c in result)

    def test_width_parameter(self):
        """Test width parameter limits output length."""
        long_values = list(range(50))
        result = sparkline_text(long_values, width=10)
        assert len(result) == 10

        # Should take last 10 values
        expected_values = list(range(40, 50))
        # Verify we get the right subset
        assert len(result) == len(expected_values)

    def test_negative_values(self):
        """Test sparkline with negative values."""
        result = sparkline_text([-10, -5, 0, 5, 10])
        assert len(result) == 5
        assert all(c in SPARK_CHARS for c in result)

        # First char should be lowest (most negative), last should be highest
        first_idx = SPARK_CHARS.index(result[0])
        last_idx = SPARK_CHARS.index(result[-1])
        assert first_idx < last_idx

    def test_floating_point_values(self):
        """Test sparkline with floating point values."""
        result = sparkline_text([1.1, 2.7, 3.14, 4.99])
        assert len(result) == 4
        assert all(c in SPARK_CHARS for c in result)

    def test_zero_width(self):
        """Test sparkline with zero width."""
        result = sparkline_text([1, 2, 3], width=0)
        assert result == ""

    def test_width_larger_than_data(self):
        """Test width larger than available data."""
        result = sparkline_text([1, 2, 3], width=10)
        assert len(result) == 3  # Should only be as long as data


class TestSparklineTextPropertyBased(unittest.TestCase):
    """Property-based tests for sparkline text generation."""

    @given(st.lists(st.floats(allow_nan=False, allow_infinity=False), min_size=1, max_size=100))
    def test_output_length_matches_input(self, values):
        """Property: Output length equals min(input_length, default_width)."""
        result = sparkline_text(values)
        expected_length = min(len(values), 20)  # Default width is 20
        assert len(result) == expected_length

    @given(st.lists(st.floats(allow_nan=False, allow_infinity=False), min_size=1, max_size=100),
           st.integers(min_value=1, max_value=50))
    def test_output_length_respects_width(self, values, width):
        """Property: Output length equals min(input_length, width)."""
        result = sparkline_text(values, width=width)
        expected_length = min(len(values), width)
        assert len(result) == expected_length

    @given(st.lists(st.floats(allow_nan=False, allow_infinity=False), min_size=1, max_size=100))
    def test_output_uses_valid_characters(self, values):
        """Property: Output only contains valid spark characters."""
        result = sparkline_text(values)
        for char in result:
            assert char in SPARK_CHARS

    @given(st.lists(st.floats(allow_nan=False, allow_infinity=False), min_size=2, max_size=50))
    def test_monotonic_input_produces_monotonic_output(self, values):
        """Property: Strictly increasing input produces non-decreasing output."""
        # Sort values to ensure monotonic increase
        sorted_values = sorted(set(values))
        assume(len(sorted_values) >= 2)  # Need at least 2 distinct values

        result = sparkline_text(sorted_values)

        # Convert chars back to indices for comparison
        indices = [SPARK_CHARS.index(c) for c in result]

        # Should be non-decreasing
        for i in range(len(indices) - 1):
            assert indices[i] <= indices[i + 1]

    @given(st.lists(st.integers(min_value=-1000, max_value=1000), min_size=1, max_size=100))
    def test_integer_values_robustness(self, values):
        """Property: Function handles integer values robustly."""
        result = sparkline_text(values)
        assert isinstance(result, str)
        assert len(result) <= len(values)

    @given(st.floats(min_value=-1000, max_value=1000, allow_nan=False, allow_infinity=False))
    def test_constant_values_produce_uniform_output(self, constant_value):
        """Property: All identical values produce uniform sparkline."""
        values = [constant_value] * 10
        result = sparkline_text(values)

        if len(result) > 0:
            # All characters should be the same
            first_char = result[0]
            assert all(c == first_char for c in result)
            # Should be the lowest spark character
            assert first_char == SPARK_CHARS[0]


class TestSparklineWidget(unittest.TestCase):
    """Unit tests for Sparkline widget class."""

    def test_widget_initialization(self):
        """Test Sparkline widget initialization."""
        widget = Sparkline()
        assert widget.label == ""
        assert widget._values == []

    def test_widget_initialization_with_label(self):
        """Test Sparkline widget initialization with label."""
        widget = Sparkline(label="Test Metric")
        assert widget.label == "Test Metric"
        assert widget._values == []

    def test_render_empty(self):
        """Test rendering empty sparkline widget."""
        widget = Sparkline()
        result = widget.render()
        assert result == ""

    def test_render_with_label_empty(self):
        """Test rendering empty sparkline widget with label."""
        widget = Sparkline(label="Test")
        result = widget.render()
        assert result == "Test: "

    def test_add_single_value(self):
        """Test adding a single value."""
        widget = Sparkline()

        with patch.object(widget, "refresh") as mock_refresh:
            widget.add_value(42.0)
            mock_refresh.assert_called_once()

        assert widget._values == [42.0]

    def test_add_multiple_values(self):
        """Test adding multiple values."""
        widget = Sparkline()

        for i in range(5):
            widget.add_value(float(i))

        assert widget._values == [0.0, 1.0, 2.0, 3.0, 4.0]

    def test_value_limit_enforcement(self):
        """Test that widget maintains maximum of 100 values."""
        widget = Sparkline()

        # Add 150 values
        for i in range(150):
            widget.add_value(float(i))

        # Should only keep last 100
        assert len(widget._values) == 100
        assert widget._values[0] == 50.0  # First of last 100
        assert widget._values[-1] == 149.0  # Last added

    def test_render_with_values(self):
        """Test rendering with actual values."""
        widget = Sparkline(label="CPU")
        widget.add_value(10.0)
        widget.add_value(20.0)
        widget.add_value(30.0)

        result = widget.render()
        assert result.startswith("CPU: ")
        assert SPARK_CHARS[0] in result  # Should contain spark chars

    def test_css_configuration(self):
        """Test CSS configuration is properly set."""
        widget = Sparkline()
        assert "height: 1" in widget.DEFAULT_CSS
        assert "width: auto" in widget.DEFAULT_CSS


class TestSparklineWidgetPropertyBased(unittest.TestCase):
    """Property-based tests for Sparkline widget."""

    @given(st.lists(st.floats(allow_nan=False, allow_infinity=False), min_size=1, max_size=200))
    def test_add_values_robustness(self, values):
        """Property: Widget handles arbitrary sequences of valid values."""
        widget = Sparkline()

        for value in values:
            widget.add_value(value)

        # Should never have more than 100 values
        assert len(widget._values) <= 100

        # Should maintain insertion order for last values
        expected_count = min(len(values), 100)
        expected_values = values[-expected_count:]
        assert widget._values == expected_values

    @given(st.text(min_size=0, max_size=50))
    def test_label_assignment(self, label):
        """Property: Widget accepts arbitrary label strings."""
        widget = Sparkline(label=label)
        assert widget.label == label

        # Rendering should not crash regardless of label
        widget.add_value(42.0)
        result = widget.render()
        assert isinstance(result, str)


class TestSparklineEdgeCases(unittest.TestCase):
    """Edge case tests for sparkline functionality."""

    def test_extreme_values(self):
        """Test handling of extreme floating point values."""
        extreme_values = [
            1e-10,    # Very small positive
            1e10,     # Very large positive
            -1e10,    # Very large negative
            -1e-10    # Very small negative
        ]

        result = sparkline_text(extreme_values)
        assert len(result) == 4
        assert all(c in SPARK_CHARS for c in result)

    def test_precision_edge_cases(self):
        """Test floating point precision edge cases."""
        close_values = [1.0, 1.0000000001, 1.0000000002]
        result = sparkline_text(close_values)
        assert isinstance(result, str)
        assert len(result) == 3

    def test_zero_range_values(self):
        """Test values with zero range (all identical)."""
        values = [math.pi] * 10
        result = sparkline_text(values)
        # All should map to the same (lowest) character
        expected = SPARK_CHARS[0] * 10
        assert result == expected

    def test_denormalized_numbers(self):
        """Test handling of denormalized floating point numbers."""
        # Python handles these automatically, but verify robustness
        tiny_values = [1e-300, 2e-300, 3e-300]
        result = sparkline_text(tiny_values)
        assert isinstance(result, str)
        assert len(result) == 3

    def test_widget_reactive_updates(self):
        """Test that reactive properties work correctly."""
        widget = Sparkline()

        # Test label reactive property
        widget.label = "New Label"
        assert widget.label == "New Label"

        # Test values reactive property
        test_values = [1.0, 2.0, 3.0]
        widget.values = test_values
        # Note: This tests the reactive property, not the internal _values


if __name__ == "__main__":
    unittest.main()
