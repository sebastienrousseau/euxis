# (c) 2026 Euxis Fleet. All rights reserved.
"""Performance metrics dashboard with sparkline visualization."""

from __future__ import annotations

import json
from typing import TYPE_CHECKING

from textual.containers import VerticalScroll
from textual.screen import Screen
from textual.widgets import Static

from tui.core import EUXIS_HOME
from tui.widgets.header import ETXHeader
from tui.widgets.shortcut_bar import ShortcutBar
from tui.widgets.sparkline import sparkline_text

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.app import EuxisApp


class MetricsScreen(Screen[None]):
    """Performance metrics dashboard with agent execution history."""

    BINDINGS = [
        ("escape", "go_back", "Back"),
        ("ctrl+k", "app.command_palette", "Commands"),
    ]

    DEFAULT_CSS = """
    #metrics-container {
        layout: vertical;
        padding: 1 2;
    }

    .metric-card {
        height: 5;
        border: round $primary-background-darken-2;
        padding: 0 1;
        margin: 0 0 1 0;
        background: $surface;
    }

    .metric-title {
        text-style: bold;
        color: $accent;
    }

    .metric-value {
        text-style: bold;
        color: $text;
    }

    .metric-spark {
        color: $success;
    }

    .metric-row {
        layout: horizontal;
        height: 3;
        padding: 0 1;
    }

    .metric-label {
        width: 20;
        content-align-vertical: middle;
    }

    .metric-data {
        width: 1fr;
        content-align-vertical: middle;
    }
    """

    @property
    def euxis_app(self) -> EuxisApp:
        """Return the typed application instance."""
        return self.app  # type: ignore[return-value]

    def compose(self) -> ComposeResult:
        """Build the metrics dashboard layout."""
        yield ETXHeader(id="header")
        with VerticalScroll(id="metrics-container"):
            yield Static(
                "[bold cyan]Performance Metrics[/]", classes="section-title"
            )
            yield Static(id="metrics-summary")
            yield Static(id="metrics-agents")
            yield Static(id="metrics-providers")

        yield ShortcutBar()

    def on_mount(self) -> None:
        """Configure header and load performance metrics."""
        header = self.query_one(ETXHeader)
        header.project = self.euxis_app.project_name
        header.branch = self.euxis_app.git_branch or ""

        self._load_metrics()

    def _load_metrics(self) -> None:
        perf_dir = EUXIS_HOME / "data" / "perf"
        summary = self.query_one("#metrics-summary", Static)
        agents_view = self.query_one("#metrics-agents", Static)
        providers_view = self.query_one("#metrics-providers", Static)

        # Collect metrics from JSONL files
        agent_times: dict[str, list[float]] = {}
        provider_counts: dict[str, int] = {}
        total_runs = 0

        if perf_dir.exists():
            for jsonl_file in sorted(perf_dir.glob("*.jsonl")):
                try:
                    for line in jsonl_file.read_text().strip().split("\n"):
                        if not line:
                            continue
                        entry = json.loads(line)
                        agent = entry.get("agent", "unknown")
                        provider = entry.get("provider", "unknown")
                        duration = entry.get("duration_ms", 0)

                        if agent not in agent_times:
                            agent_times[agent] = []
                        agent_times[agent].append(duration / 1000.0)

                        provider_counts[provider] = provider_counts.get(provider, 0) + 1
                        total_runs += 1
                except (json.JSONDecodeError, KeyError):
                    continue

        if total_runs == 0:
            summary.update(
                "[dim]No performance data available yet.\n"
                "Run agents to generate metrics.[/]"
            )
            agents_view.update("")
            providers_view.update("")
            return

        # Summary
        all_times = [t for times in agent_times.values() for t in times]
        avg_time = sum(all_times) / len(all_times) if all_times else 0
        spark = sparkline_text(all_times[-50:], width=40)

        summary.update(
            f"[bold]Total Runs:[/] {total_runs}  "
            f"[bold]Agents Used:[/] {len(agent_times)}  "
            f"[bold]Avg Time:[/] {avg_time:.1f}s\n"
            f"[dim]Response Times:[/] [green]{spark}[/]"
        )

        # Per-agent metrics
        agent_lines = ["", "[bold cyan]Agent Performance[/]"]
        for agent_id in sorted(agent_times.keys()):
            times = agent_times[agent_id]
            avg = sum(times) / len(times)
            spark = sparkline_text(times[-20:], width=20)
            agent_lines.append(
                f"  [bold]{agent_id:<16}[/] "
                f"[dim]{len(times):>3} runs[/]  "
                f"[dim]avg[/] {avg:>5.1f}s  "
                f"[green]{spark}[/]"
            )
        agents_view.update("\n".join(agent_lines))

        # Provider distribution
        prov_lines = ["", "[bold cyan]Provider Distribution[/]"]
        for provider in sorted(provider_counts.keys()):
            count = provider_counts[provider]
            pct = (count / total_runs) * 100
            bar_width = int(pct / 2)
            bar = "█" * bar_width + "░" * (50 - bar_width)
            prov_lines.append(
                f"  [bold]{provider:<12}[/] "
                f"[dim]{count:>4} runs[/]  "
                f"[yellow]{bar}[/] {pct:.0f}%"
            )
        providers_view.update("\n".join(prov_lines))

    def action_go_back(self) -> None:
        """Return to the previous screen."""
        self.app.pop_screen()
