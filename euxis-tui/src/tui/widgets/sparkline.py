# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Terminal sparkline widget for inline performance visualization."""

from __future__ import annotations

import math
from typing import Any

from textual.reactive import reactive
from textual.widget import Widget

SPARK_CHARS = "▁▂▃▄▅▆▇█"

_MAX_SPARKLINE_VALUES = 100


def sparkline_text(values: list[float], width: int = 20) -> str:
    """Generate a sparkline string from values."""
    if not values or width <= 0:
        return ""

    # Filter out None values
    values = [v for v in values if v is not None]
    if not values:
        return ""

    # Normalize to fit the spark character range
    min_val = min(values)
    max_val = max(values)
    val_range = max_val - min_val

    # Take the last `width` values
    recent = values[-width:]

    # Guard against overflow (extreme floats) or zero range
    if not math.isfinite(val_range) or val_range == 0:
        return SPARK_CHARS[0] * len(recent)

    chars = []
    for v in recent:
        normalized = (v - min_val) / val_range
        if not math.isfinite(normalized):
            normalized = 0.0  # pragma: no cover
        normalized = max(0.0, min(1.0, normalized))
        idx = min(int(normalized * (len(SPARK_CHARS) - 1)), len(SPARK_CHARS) - 1)
        chars.append(SPARK_CHARS[idx])

    return "".join(chars)


class Sparkline(Widget):
    """Inline sparkline chart widget."""

    DEFAULT_CSS = """
    Sparkline {
        height: 1;
        width: auto;
    }
    """

    values: reactive[list[float]] = reactive(list, always_update=True)
    label: reactive[str] = reactive("")

    def __init__(self, label: str = "", **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.label = label
        self._values: list[float] = []

    def render(self) -> str:
        """Render the sparkline as a text string."""
        spark = sparkline_text(self._values, width=30)
        if self.label:
            return f"{self.label}: {spark}"
        return spark

    def add_value(self, value: float) -> None:
        """Add a new data point."""
        self._values.append(value)
        # Keep last N values
        if len(self._values) > _MAX_SPARKLINE_VALUES:
            self._values = self._values[-_MAX_SPARKLINE_VALUES:]
        self.refresh()
