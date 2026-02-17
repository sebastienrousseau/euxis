# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Real-time resource monitoring widget with sparkline visualization."""

from __future__ import annotations

import asyncio
from typing import TYPE_CHECKING, Any

from textual.containers import Horizontal
from textual.reactive import reactive
from textual.widget import Widget
from textual.widgets import Static

from tui.widgets.sparkline import Sparkline, sparkline_text

if TYPE_CHECKING:
    from textual.app import ComposeResult


def get_cpu_percent() -> float:
    """Get current CPU usage percentage."""
    try:
        with open("/proc/stat") as f:
            line = f.readline()
        fields = line.split()[1:8]
        idle = int(fields[3])
        total = sum(int(f) for f in fields)
        # Approximate usage (not delta-based, but fast)
        return max(0.0, min(100.0, 100.0 - (idle / total * 100)))
    except Exception:
        return 0.0


def get_memory_percent() -> float:
    """Get current memory usage percentage."""
    try:
        with open("/proc/meminfo") as f:
            lines = f.readlines()
        mem_info = {}
        for line in lines[:5]:
            parts = line.split()
            if len(parts) >= 2:
                key = parts[0].rstrip(":")
                mem_info[key] = int(parts[1])
        total = mem_info.get("MemTotal", 1)
        available = mem_info.get("MemAvailable", mem_info.get("MemFree", 0))
        used = total - available
        return max(0.0, min(100.0, (used / total) * 100))
    except Exception:
        return 0.0


def get_thermal_celsius() -> float | None:
    """Get CPU temperature in Celsius (Linux only)."""
    try:
        with open("/sys/class/thermal/thermal_zone0/temp") as f:
            return int(f.read().strip()) / 1000.0
    except Exception:
        return None


class ResourceMonitor(Widget):
    """Real-time CPU, RAM, and thermal monitoring with sparklines.

    Provides at-a-glance hardware status visualization that updates
    every second. Sparkline graphs show trends over the last 30 seconds.
    """

    DEFAULT_CSS = """
    ResourceMonitor {
        height: 2;
        width: 100%;
        layout: horizontal;
        padding: 0 1;
    }
    ResourceMonitor .resource-label {
        width: auto;
        min-width: 6;
        color: $text-muted;
    }
    ResourceMonitor .resource-spark {
        width: auto;
        min-width: 12;
    }
    ResourceMonitor .resource-value {
        width: auto;
        min-width: 6;
        color: $text;
    }
    ResourceMonitor .cpu-spark {
        color: $warning;
    }
    ResourceMonitor .mem-spark {
        color: $success;
    }
    ResourceMonitor .temp-value {
        color: $primary;
    }
    ResourceMonitor .temp-hot {
        color: $error;
        text-style: bold;
    }
    """

    cpu_values: reactive[list[float]] = reactive(list, always_update=True)
    mem_values: reactive[list[float]] = reactive(list, always_update=True)

    def __init__(self, poll_interval: float = 1.0, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self._cpu_history: list[float] = []
        self._mem_history: list[float] = []
        self._poll_interval = poll_interval
        self._polling = False

    def compose(self) -> ComposeResult:
        """Build the resource monitor layout."""
        with Horizontal():
            yield Static("CPU:", classes="resource-label")
            yield Static("", id="cpu-spark", classes="resource-spark cpu-spark")
            yield Static("", id="cpu-value", classes="resource-value")
            yield Static(" MEM:", classes="resource-label")
            yield Static("", id="mem-spark", classes="resource-spark mem-spark")
            yield Static("", id="mem-value", classes="resource-value")
            yield Static("", id="temp-value", classes="resource-value temp-value")

    def on_mount(self) -> None:
        """Start polling for resource data."""
        self._polling = True
        self.run_worker(self._poll_resources, exclusive=True)

    def on_unmount(self) -> None:
        """Stop polling."""
        self._polling = False

    async def _poll_resources(self) -> None:
        """Poll CPU, memory, and temperature at regular intervals."""
        while self._polling:
            cpu = get_cpu_percent()
            mem = get_memory_percent()
            temp = get_thermal_celsius()

            # Update history (keep last 30 samples)
            self._cpu_history.append(cpu)
            self._mem_history.append(mem)
            if len(self._cpu_history) > 30:
                self._cpu_history = self._cpu_history[-30:]
            if len(self._mem_history) > 30:
                self._mem_history = self._mem_history[-30:]

            # Update display
            self._update_display(cpu, mem, temp)

            await asyncio.sleep(self._poll_interval)

    def _update_display(self, cpu: float, mem: float, temp: float | None) -> None:
        """Update the sparkline and value displays."""
        try:
            cpu_spark = self.query_one("#cpu-spark", Static)
            cpu_spark.update(sparkline_text(self._cpu_history, width=12))

            cpu_value = self.query_one("#cpu-value", Static)
            cpu_value.update(f"{cpu:4.0f}%")

            mem_spark = self.query_one("#mem-spark", Static)
            mem_spark.update(sparkline_text(self._mem_history, width=12))

            mem_value = self.query_one("#mem-value", Static)
            mem_value.update(f"{mem:4.0f}%")

            temp_widget = self.query_one("#temp-value", Static)
            if temp is not None:
                icon = " " if temp < 70 else " " if temp < 85 else " "
                temp_widget.update(f"{icon}{temp:.0f}C")
                if temp >= 85:
                    temp_widget.add_class("temp-hot")
                else:
                    temp_widget.remove_class("temp-hot")
            else:
                temp_widget.update("")
        except Exception:
            pass  # Widget may be unmounted during update


class MiniSparkline(Static):
    """A minimal inline sparkline for embedding in other widgets."""

    def __init__(self, label: str = "", **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self._label = label
        self._values: list[float] = []

    def add_value(self, value: float) -> None:
        """Add a data point and refresh."""
        self._values.append(value)
        if len(self._values) > 20:
            self._values = self._values[-20:]
        self._update_display()

    def _update_display(self) -> None:
        """Render the sparkline."""
        spark = sparkline_text(self._values, width=10)
        if self._label:
            self.update(f"{self._label}: {spark}")
        else:
            self.update(spark)
