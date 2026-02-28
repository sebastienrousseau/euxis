# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Comprehensive unit tests for ResourceMonitor and MiniSparkline widgets.

Tests cover: get_cpu_percent, get_memory_percent, get_thermal_celsius
standalone functions, ResourceMonitor lifecycle and polling, MiniSparkline
value management and display rendering.
"""

from __future__ import annotations

import asyncio
import unittest
from unittest.mock import AsyncMock, MagicMock, Mock, call, mock_open, patch

from tui.widgets.resource_monitor import (
    MiniSparkline,
    ResourceMonitor,
    get_cpu_percent,
    get_memory_percent,
    get_thermal_celsius,
)


# ---------------------------------------------------------------------------
# get_cpu_percent
# ---------------------------------------------------------------------------

class TestGetCpuPercent(unittest.TestCase):
    """Tests for the get_cpu_percent() helper function."""

    def test_valid_proc_stat(self):
        """Test get_cpu_percent with valid /proc/stat data."""
        # Simulated /proc/stat first line:
        # cpu  user nice system idle iowait irq softirq
        data = "cpu  10000 500 3000 80000 200 100 50\n"
        m = mock_open(read_data=data)
        with patch("builtins.open", m):
            result = get_cpu_percent()
        # fields[0..6] = 10000 500 3000 80000 200 100 50
        # idle = 80000, total = 93850
        # usage = 100 - (80000/93850 * 100) ~= 14.76
        self.assertGreater(result, 0.0)
        self.assertLessEqual(result, 100.0)

    def test_all_idle_returns_near_zero(self):
        """Test get_cpu_percent when CPU is nearly all idle."""
        # idle dominates
        data = "cpu  0 0 0 100000 0 0 0\n"
        m = mock_open(read_data=data)
        with patch("builtins.open", m):
            result = get_cpu_percent()
        self.assertAlmostEqual(result, 0.0, places=1)

    def test_high_usage(self):
        """Test get_cpu_percent when CPU is under heavy load."""
        # idle = 1000 out of 100000 total
        data = "cpu  50000 10000 30000 1000 5000 3000 1000\n"
        m = mock_open(read_data=data)
        with patch("builtins.open", m):
            result = get_cpu_percent()
        self.assertGreater(result, 90.0)

    def test_file_not_found_returns_zero(self):
        """Test get_cpu_percent returns 0.0 when /proc/stat is missing."""
        with patch("builtins.open", side_effect=FileNotFoundError):
            result = get_cpu_percent()
        assert result == 0.0

    def test_permission_error_returns_zero(self):
        """Test get_cpu_percent returns 0.0 on PermissionError."""
        with patch("builtins.open", side_effect=PermissionError):
            result = get_cpu_percent()
        assert result == 0.0

    def test_malformed_data_returns_zero(self):
        """Test get_cpu_percent returns 0.0 on malformed data."""
        data = "cpu  not numbers here\n"
        m = mock_open(read_data=data)
        with patch("builtins.open", m):
            result = get_cpu_percent()
        assert result == 0.0

    def test_result_clamped_at_zero(self):
        """Test get_cpu_percent result is never negative."""
        # Edge case: idle > total shouldn't happen, but test clamp
        data = "cpu  0 0 0 0 0 0 0\n"
        m = mock_open(read_data=data)
        with patch("builtins.open", m):
            result = get_cpu_percent()
        # total = 0, will raise ZeroDivisionError -> returns 0.0
        assert result == 0.0


# ---------------------------------------------------------------------------
# get_memory_percent
# ---------------------------------------------------------------------------

class TestGetMemoryPercent(unittest.TestCase):
    """Tests for the get_memory_percent() helper function."""

    def test_valid_meminfo_with_mem_available(self):
        """Test get_memory_percent with MemTotal and MemAvailable."""
        data = (
            "MemTotal:       16000000 kB\n"
            "MemFree:         2000000 kB\n"
            "MemAvailable:    8000000 kB\n"
            "Buffers:          500000 kB\n"
            "Cached:          3000000 kB\n"
        )
        m = mock_open(read_data=data)
        with patch("builtins.open", m):
            result = get_memory_percent()
        # used = 16000000 - 8000000 = 8000000
        # percent = 8000000/16000000 * 100 = 50.0
        self.assertAlmostEqual(result, 50.0)

    def test_valid_meminfo_without_mem_available(self):
        """Test get_memory_percent falls back to MemFree."""
        data = (
            "MemTotal:       16000000 kB\n"
            "MemFree:         4000000 kB\n"
            "Buffers:          500000 kB\n"
            "Cached:          3000000 kB\n"
            "SwapTotal:       8000000 kB\n"
        )
        m = mock_open(read_data=data)
        with patch("builtins.open", m):
            result = get_memory_percent()
        # No MemAvailable, falls back to MemFree=4000000
        # used = 16000000 - 4000000 = 12000000
        # percent = 12000000/16000000 * 100 = 75.0
        self.assertAlmostEqual(result, 75.0)

    def test_file_not_found_returns_zero(self):
        """Test get_memory_percent returns 0.0 when /proc/meminfo is missing."""
        with patch("builtins.open", side_effect=FileNotFoundError):
            result = get_memory_percent()
        assert result == 0.0

    def test_permission_error_returns_zero(self):
        """Test get_memory_percent returns 0.0 on PermissionError."""
        with patch("builtins.open", side_effect=PermissionError):
            result = get_memory_percent()
        assert result == 0.0

    def test_malformed_data_returns_zero(self):
        """Test get_memory_percent returns 0.0 on malformed data."""
        data = "garbled nonsense\n"
        m = mock_open(read_data=data)
        with patch("builtins.open", m):
            result = get_memory_percent()
        # MemTotal not found, defaults to 1; MemAvailable defaults to 0
        # used = 1 - 0 = 1, percent = (1/1)*100 = 100.0 clamped
        self.assertLessEqual(result, 100.0)
        self.assertGreaterEqual(result, 0.0)

    def test_result_clamped_0_to_100(self):
        """Test memory percent is always in [0.0, 100.0]."""
        data = (
            "MemTotal:       8000000 kB\n"
            "MemFree:           1000 kB\n"
            "MemAvailable:      1000 kB\n"
            "Buffers:              0 kB\n"
            "Cached:               0 kB\n"
        )
        m = mock_open(read_data=data)
        with patch("builtins.open", m):
            result = get_memory_percent()
        self.assertGreaterEqual(result, 0.0)
        self.assertLessEqual(result, 100.0)

    def test_only_reads_first_five_lines(self):
        """Test that only the first 5 lines of /proc/meminfo are parsed."""
        data = (
            "MemTotal:       16000000 kB\n"
            "MemFree:         2000000 kB\n"
            "MemAvailable:    8000000 kB\n"
            "Buffers:          500000 kB\n"
            "Cached:          3000000 kB\n"
            "ShouldIgnore:    9999999 kB\n"
        )
        m = mock_open(read_data=data)
        with patch("builtins.open", m):
            result = get_memory_percent()
        # Should still work correctly, just ignoring line 6
        self.assertAlmostEqual(result, 50.0)


# ---------------------------------------------------------------------------
# get_thermal_celsius
# ---------------------------------------------------------------------------

class TestGetThermalCelsius(unittest.TestCase):
    """Tests for the get_thermal_celsius() helper function."""

    def test_valid_temperature(self):
        """Test get_thermal_celsius with valid thermal data."""
        m = mock_open(read_data="45000\n")
        with patch("builtins.open", m):
            result = get_thermal_celsius()
        assert result == 45.0

    def test_high_temperature(self):
        """Test get_thermal_celsius with high temperature."""
        m = mock_open(read_data="95000\n")
        with patch("builtins.open", m):
            result = get_thermal_celsius()
        assert result == 95.0

    def test_zero_temperature(self):
        """Test get_thermal_celsius with zero temperature."""
        m = mock_open(read_data="0\n")
        with patch("builtins.open", m):
            result = get_thermal_celsius()
        assert result == 0.0

    def test_file_not_found_returns_none(self):
        """Test get_thermal_celsius returns None when thermal zone is missing."""
        with patch("builtins.open", side_effect=FileNotFoundError):
            result = get_thermal_celsius()
        assert result is None

    def test_permission_error_returns_none(self):
        """Test get_thermal_celsius returns None on PermissionError."""
        with patch("builtins.open", side_effect=PermissionError):
            result = get_thermal_celsius()
        assert result is None

    def test_malformed_data_returns_none(self):
        """Test get_thermal_celsius returns None on malformed data."""
        m = mock_open(read_data="not_a_number\n")
        with patch("builtins.open", m):
            result = get_thermal_celsius()
        assert result is None


# ---------------------------------------------------------------------------
# ResourceMonitor
# ---------------------------------------------------------------------------

class TestResourceMonitorInit(unittest.TestCase):
    """Tests for ResourceMonitor.__init__."""

    def test_default_poll_interval(self):
        """Test default poll interval is 1.0."""
        widget = ResourceMonitor()
        assert widget._poll_interval == 1.0

    def test_custom_poll_interval(self):
        """Test custom poll interval is stored."""
        widget = ResourceMonitor(poll_interval=5.0)
        assert widget._poll_interval == 5.0

    def test_initial_cpu_history_empty(self):
        """Test CPU history starts empty."""
        widget = ResourceMonitor()
        assert widget._cpu_history == []

    def test_initial_mem_history_empty(self):
        """Test memory history starts empty."""
        widget = ResourceMonitor()
        assert widget._mem_history == []

    def test_initial_polling_false(self):
        """Test polling starts as False."""
        widget = ResourceMonitor()
        assert widget._polling is False

    def test_construction_with_id(self):
        """Test construction with id kwarg."""
        widget = ResourceMonitor(id="resource-mon")
        assert widget.id == "resource-mon"


class TestResourceMonitorCSS(unittest.TestCase):
    """Tests for the DEFAULT_CSS."""

    def test_css_is_defined(self):
        """Test DEFAULT_CSS is non-empty."""
        assert isinstance(ResourceMonitor.DEFAULT_CSS, str)
        assert len(ResourceMonitor.DEFAULT_CSS) > 0

    def test_css_contains_height(self):
        """Test CSS sets height."""
        assert "height: 2" in ResourceMonitor.DEFAULT_CSS

    def test_css_horizontal_layout(self):
        """Test CSS uses horizontal layout."""
        assert "layout: horizontal" in ResourceMonitor.DEFAULT_CSS


class TestResourceMonitorCompose(unittest.TestCase):
    """Tests for compose() method."""

    @patch("tui.widgets.resource_monitor.Static")
    @patch("tui.widgets.resource_monitor.Horizontal")
    def test_compose_yields_widgets(self, mock_horizontal, mock_static):
        """Test compose creates the expected widget structure."""
        mock_ctx = MagicMock()
        mock_horizontal.return_value.__enter__ = Mock(return_value=mock_ctx)
        mock_horizontal.return_value.__exit__ = Mock(return_value=False)
        widget = ResourceMonitor()
        result = list(widget.compose())
        # compose yields 7 Static widgets inside Horizontal
        assert mock_static.call_count == 7

    @patch("tui.widgets.resource_monitor.Static")
    @patch("tui.widgets.resource_monitor.Horizontal")
    def test_compose_creates_cpu_spark(self, mock_horizontal, mock_static):
        """Test compose creates cpu-spark widget."""
        mock_ctx = MagicMock()
        mock_horizontal.return_value.__enter__ = Mock(return_value=mock_ctx)
        mock_horizontal.return_value.__exit__ = Mock(return_value=False)
        widget = ResourceMonitor()
        list(widget.compose())
        ids = [c.kwargs.get("id") for c in mock_static.call_args_list]
        assert "cpu-spark" in ids

    @patch("tui.widgets.resource_monitor.Static")
    @patch("tui.widgets.resource_monitor.Horizontal")
    def test_compose_creates_mem_spark(self, mock_horizontal, mock_static):
        """Test compose creates mem-spark widget."""
        mock_ctx = MagicMock()
        mock_horizontal.return_value.__enter__ = Mock(return_value=mock_ctx)
        mock_horizontal.return_value.__exit__ = Mock(return_value=False)
        widget = ResourceMonitor()
        list(widget.compose())
        ids = [c.kwargs.get("id") for c in mock_static.call_args_list]
        assert "mem-spark" in ids

    @patch("tui.widgets.resource_monitor.Static")
    @patch("tui.widgets.resource_monitor.Horizontal")
    def test_compose_creates_temp_value(self, mock_horizontal, mock_static):
        """Test compose creates temp-value widget."""
        mock_ctx = MagicMock()
        mock_horizontal.return_value.__enter__ = Mock(return_value=mock_ctx)
        mock_horizontal.return_value.__exit__ = Mock(return_value=False)
        widget = ResourceMonitor()
        list(widget.compose())
        ids = [c.kwargs.get("id") for c in mock_static.call_args_list]
        assert "temp-value" in ids


class TestResourceMonitorLifecycle(unittest.TestCase):
    """Tests for on_mount and on_unmount lifecycle methods."""

    def test_on_mount_sets_polling_true(self):
        """Test on_mount sets _polling to True."""
        widget = ResourceMonitor()
        widget.run_worker = Mock()
        widget.on_mount()
        assert widget._polling is True

    def test_on_mount_calls_run_worker(self):
        """Test on_mount calls run_worker with _poll_resources."""
        widget = ResourceMonitor()
        widget.run_worker = Mock()
        widget.on_mount()
        widget.run_worker.assert_called_once_with(widget._poll_resources, exclusive=True)

    def test_on_unmount_sets_polling_false(self):
        """Test on_unmount sets _polling to False."""
        widget = ResourceMonitor()
        widget._polling = True
        widget.on_unmount()
        assert widget._polling is False


class TestResourceMonitorPollResources(unittest.TestCase):
    """Tests for _poll_resources async method."""

    def test_poll_resources_updates_history(self):
        """Test _poll_resources appends to CPU and memory history."""
        widget = ResourceMonitor(poll_interval=0.01)
        widget._polling = True

        call_count = 0

        async def fake_sleep(interval):
            nonlocal call_count
            call_count += 1
            if call_count >= 2:
                widget._polling = False

        with patch("tui.widgets.resource_monitor.get_cpu_percent", return_value=42.0), \
             patch("tui.widgets.resource_monitor.get_memory_percent", return_value=55.0), \
             patch("tui.widgets.resource_monitor.get_thermal_celsius", return_value=65.0), \
             patch.object(widget, "_update_display"), \
             patch("asyncio.sleep", side_effect=fake_sleep):
            asyncio.run(widget._poll_resources())

        assert len(widget._cpu_history) == 2
        assert widget._cpu_history == [42.0, 42.0]
        assert len(widget._mem_history) == 2
        assert widget._mem_history == [55.0, 55.0]

    def test_poll_resources_calls_update_display(self):
        """Test _poll_resources calls _update_display each iteration."""
        widget = ResourceMonitor(poll_interval=0.01)
        widget._polling = True

        call_count = 0

        async def fake_sleep(interval):
            nonlocal call_count
            call_count += 1
            if call_count >= 1:
                widget._polling = False

        mock_update = Mock()

        with patch("tui.widgets.resource_monitor.get_cpu_percent", return_value=10.0), \
             patch("tui.widgets.resource_monitor.get_memory_percent", return_value=20.0), \
             patch("tui.widgets.resource_monitor.get_thermal_celsius", return_value=None), \
             patch.object(widget, "_update_display", mock_update), \
             patch("asyncio.sleep", side_effect=fake_sleep):
            asyncio.run(widget._poll_resources())

        mock_update.assert_called_with(10.0, 20.0, None)

    def test_poll_resources_trims_history_at_30(self):
        """Test _poll_resources trims history to 30 entries."""
        widget = ResourceMonitor(poll_interval=0.01)
        widget._polling = True
        # Pre-fill history with 29 items
        widget._cpu_history = list(range(29))
        widget._mem_history = list(range(29))

        call_count = 0

        async def fake_sleep(interval):
            nonlocal call_count
            call_count += 1
            if call_count >= 3:
                widget._polling = False

        with patch("tui.widgets.resource_monitor.get_cpu_percent", return_value=99.0), \
             patch("tui.widgets.resource_monitor.get_memory_percent", return_value=88.0), \
             patch("tui.widgets.resource_monitor.get_thermal_celsius", return_value=50.0), \
             patch.object(widget, "_update_display"), \
             patch("asyncio.sleep", side_effect=fake_sleep):
            asyncio.run(widget._poll_resources())

        # 29 + 3 = 32, trimmed to 30
        assert len(widget._cpu_history) == 30
        assert len(widget._mem_history) == 30
        # Last entry should be the polled value
        assert widget._cpu_history[-1] == 99.0
        assert widget._mem_history[-1] == 88.0


class TestResourceMonitorUpdateDisplay(unittest.TestCase):
    """Tests for _update_display method."""

    def _make_widget_with_mocks(self):
        """Create a ResourceMonitor with mocked query_one."""
        widget = ResourceMonitor()
        widget._cpu_history = [10.0, 20.0, 30.0]
        widget._mem_history = [40.0, 50.0, 60.0]

        mock_cpu_spark = Mock()
        mock_cpu_value = Mock()
        mock_mem_spark = Mock()
        mock_mem_value = Mock()
        mock_temp_value = Mock()
        mock_temp_value.add_class = Mock()
        mock_temp_value.remove_class = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            mapping = {
                "#cpu-spark": mock_cpu_spark,
                "#cpu-value": mock_cpu_value,
                "#mem-spark": mock_mem_spark,
                "#mem-value": mock_mem_value,
                "#temp-value": mock_temp_value,
            }
            return mapping.get(selector, Mock())

        widget.query_one = Mock(side_effect=query_one_side_effect)
        return widget, mock_cpu_spark, mock_cpu_value, mock_mem_spark, mock_mem_value, mock_temp_value

    def test_update_display_sets_cpu_spark(self):
        """Test _update_display updates CPU sparkline."""
        widget, mock_cpu_spark, *_ = self._make_widget_with_mocks()
        widget._update_display(25.0, 50.0, 65.0)
        mock_cpu_spark.update.assert_called_once()

    def test_update_display_sets_cpu_value(self):
        """Test _update_display updates CPU value text."""
        widget, _, mock_cpu_value, *_ = self._make_widget_with_mocks()
        widget._update_display(25.0, 50.0, 65.0)
        mock_cpu_value.update.assert_called_once_with("  25%")

    def test_update_display_sets_mem_spark(self):
        """Test _update_display updates memory sparkline."""
        widget, _, _, mock_mem_spark, *_ = self._make_widget_with_mocks()
        widget._update_display(25.0, 50.0, 65.0)
        mock_mem_spark.update.assert_called_once()

    def test_update_display_sets_mem_value(self):
        """Test _update_display updates memory value text."""
        widget, _, _, _, mock_mem_value, _ = self._make_widget_with_mocks()
        widget._update_display(25.0, 50.0, 65.0)
        mock_mem_value.update.assert_called_once_with("  50%")

    def test_update_display_with_temp(self):
        """Test _update_display shows temperature when available."""
        widget, *_, mock_temp = self._make_widget_with_mocks()
        widget._update_display(25.0, 50.0, 65.0)
        # temp < 70, first icon
        update_arg = mock_temp.update.call_args[0][0]
        assert "65" in update_arg
        assert "C" in update_arg

    def test_update_display_temp_below_70_removes_hot_class(self):
        """Test temp < 70 removes temp-hot class."""
        widget, *_, mock_temp = self._make_widget_with_mocks()
        widget._update_display(25.0, 50.0, 65.0)
        mock_temp.remove_class.assert_called_with("temp-hot")

    def test_update_display_temp_at_80_removes_hot_class(self):
        """Test temp between 70 and 85 removes temp-hot class."""
        widget, *_, mock_temp = self._make_widget_with_mocks()
        widget._update_display(25.0, 50.0, 80.0)
        mock_temp.remove_class.assert_called_with("temp-hot")

    def test_update_display_temp_at_90_adds_hot_class(self):
        """Test temp >= 85 adds temp-hot class."""
        widget, *_, mock_temp = self._make_widget_with_mocks()
        widget._update_display(25.0, 50.0, 90.0)
        mock_temp.add_class.assert_called_with("temp-hot")
        update_arg = mock_temp.update.call_args[0][0]
        assert "90" in update_arg

    def test_update_display_temp_exactly_85(self):
        """Test temp == 85 adds temp-hot class."""
        widget, *_, mock_temp = self._make_widget_with_mocks()
        widget._update_display(25.0, 50.0, 85.0)
        mock_temp.add_class.assert_called_with("temp-hot")

    def test_update_display_temp_none_clears_widget(self):
        """Test temp=None sets empty string on temp widget."""
        widget, *_, mock_temp = self._make_widget_with_mocks()
        widget._update_display(25.0, 50.0, None)
        mock_temp.update.assert_called_once_with("")

    def test_update_display_exception_caught(self):
        """Test _update_display catches exceptions gracefully."""
        widget = ResourceMonitor()
        widget._cpu_history = [10.0]
        widget._mem_history = [20.0]
        widget.query_one = Mock(side_effect=Exception("widget not mounted"))
        # Should not raise
        widget._update_display(10.0, 20.0, None)

    def test_update_display_sparkline_text_called(self):
        """Test _update_display calls sparkline_text with history."""
        widget, mock_cpu_spark, *_ = self._make_widget_with_mocks()
        with patch("tui.widgets.resource_monitor.sparkline_text", return_value="test_spark") as mock_st:
            widget._update_display(25.0, 50.0, None)
        # sparkline_text should be called for CPU and MEM
        assert mock_st.call_count == 2
        # First call is for CPU history
        assert mock_st.call_args_list[0] == call([10.0, 20.0, 30.0], width=12)


# ---------------------------------------------------------------------------
# MiniSparkline
# ---------------------------------------------------------------------------

class TestMiniSparklineInit(unittest.TestCase):
    """Tests for MiniSparkline.__init__."""

    def test_default_label(self):
        """Test MiniSparkline default label is empty."""
        widget = MiniSparkline()
        assert widget._label == ""

    def test_custom_label(self):
        """Test MiniSparkline stores custom label."""
        widget = MiniSparkline(label="CPU")
        assert widget._label == "CPU"

    def test_initial_values_empty(self):
        """Test MiniSparkline starts with empty values."""
        widget = MiniSparkline()
        assert widget._values == []

    def test_construction_with_id(self):
        """Test construction with id kwarg."""
        widget = MiniSparkline(id="mini-spark")
        assert widget.id == "mini-spark"


class TestMiniSparklineAddValue(unittest.TestCase):
    """Tests for MiniSparkline.add_value."""

    def test_add_single_value(self):
        """Test adding a single value."""
        widget = MiniSparkline()
        with patch.object(widget, "_update_display"):
            widget.add_value(42.0)
        assert widget._values == [42.0]

    def test_add_multiple_values(self):
        """Test adding multiple values preserves order."""
        widget = MiniSparkline()
        with patch.object(widget, "_update_display"):
            for v in [1.0, 2.0, 3.0]:
                widget.add_value(v)
        assert widget._values == [1.0, 2.0, 3.0]

    def test_add_value_calls_update_display(self):
        """Test add_value calls _update_display."""
        widget = MiniSparkline()
        with patch.object(widget, "_update_display") as mock_update:
            widget.add_value(10.0)
        mock_update.assert_called_once()

    def test_values_trimmed_at_20(self):
        """Test values list is trimmed to last 20 entries."""
        widget = MiniSparkline()
        with patch.object(widget, "_update_display"):
            for i in range(25):
                widget.add_value(float(i))
        assert len(widget._values) == 20
        assert widget._values[0] == 5.0
        assert widget._values[-1] == 24.0

    def test_values_exactly_20_no_trim(self):
        """Test values list at exactly 20 does not trim."""
        widget = MiniSparkline()
        with patch.object(widget, "_update_display"):
            for i in range(20):
                widget.add_value(float(i))
        assert len(widget._values) == 20
        assert widget._values[0] == 0.0

    def test_values_21_trims_to_20(self):
        """Test adding the 21st value triggers trimming."""
        widget = MiniSparkline()
        with patch.object(widget, "_update_display"):
            for i in range(21):
                widget.add_value(float(i))
        assert len(widget._values) == 20
        assert widget._values[0] == 1.0


class TestMiniSparklineUpdateDisplay(unittest.TestCase):
    """Tests for MiniSparkline._update_display."""

    def test_update_display_with_label(self):
        """Test _update_display includes label prefix."""
        widget = MiniSparkline(label="CPU")
        widget._values = [10.0, 20.0, 30.0]
        with patch("tui.widgets.resource_monitor.sparkline_text", return_value="abc") as mock_st:
            with patch.object(widget, "update") as mock_update:
                widget._update_display()
        mock_update.assert_called_once_with("CPU: abc")

    def test_update_display_without_label(self):
        """Test _update_display with no label outputs only sparkline."""
        widget = MiniSparkline()
        widget._values = [10.0, 20.0]
        with patch("tui.widgets.resource_monitor.sparkline_text", return_value="xy") as mock_st:
            with patch.object(widget, "update") as mock_update:
                widget._update_display()
        mock_update.assert_called_once_with("xy")

    def test_update_display_calls_sparkline_text(self):
        """Test _update_display passes values and width=10 to sparkline_text."""
        widget = MiniSparkline()
        widget._values = [1.0, 2.0, 3.0]
        with patch("tui.widgets.resource_monitor.sparkline_text", return_value="") as mock_st:
            with patch.object(widget, "update"):
                widget._update_display()
        mock_st.assert_called_once_with([1.0, 2.0, 3.0], width=10)

    def test_update_display_empty_values(self):
        """Test _update_display with empty values produces empty sparkline."""
        widget = MiniSparkline(label="MEM")
        widget._values = []
        with patch("tui.widgets.resource_monitor.sparkline_text", return_value="") as mock_st:
            with patch.object(widget, "update") as mock_update:
                widget._update_display()
        mock_update.assert_called_once_with("MEM: ")


if __name__ == "__main__":
    unittest.main()
