# (c) 2026 Euxis Fleet. All rights reserved.
"""Terminal sparkline widget for inline performance visualization."""

from __future__ import annotations

from textual.reactive import reactive
from textual.widget import Widget
from textual.widgets import Static

SPARK_CHARS = "▁▂▃▄▅▆▇█"


def sparkline_text(values: list[float], width: int = 20) -> str:
    """Generate a sparkline string from values."""
    if not values:
        return ""

    # Normalize to fit the spark character range
    min_val = min(values)
    max_val = max(values)
    val_range = max_val - min_val if max_val != min_val else 1

    # Take the last `width` values
    recent = values[-width:]

    chars = []
    for v in recent:
        normalized = (v - min_val) / val_range
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

    def __init__(self, label: str = "", **kwargs) -> None:
        super().__init__(**kwargs)
        self.label = label
        self._values: list[float] = []

    def render(self) -> str:
        spark = sparkline_text(self._values, width=30)
        if self.label:
            return f"{self.label}: {spark}"
        return spark

    def add_value(self, value: float) -> None:
        """Add a new data point."""
        self._values.append(value)
        # Keep last 100 values
        if len(self._values) > 100:
            self._values = self._values[-100:]
        self.refresh()
