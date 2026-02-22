# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Agent card widget for the fleet grid with Liquid Glass styling.

Supports context-aware expansion based on agent phase:
- Research phase: Shows "Links Found" counter
- Coding phase: Shows "Files Written" progress
- Reason tier: Shows "Thought Process" snippets
- Indeterminate: Pulse animation instead of percentage
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Any

from textual.binding import Binding
from textual.containers import Vertical
from textual.events import Key
from textual.message import Message
from textual.reactive import reactive
from textual.widget import Widget
from textual.widgets import ProgressBar, Static

from tui.i18n import _
from tui.widgets.agent_avatar import CompactAvatar
from tui.core.telemetry import alert_budget_exceeded

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.core.registry import Agent

# Agent execution phases (for context-aware UI)
PHASE_IDLE = "idle"
PHASE_RESEARCH = "research"
PHASE_CODING = "coding"
PHASE_REASONING = "reasoning"
PHASE_COMPLETE = "complete"

# Agent status (for live activity indicators)
STATUS_IDLE = "idle"
STATUS_ACTIVE = "active"
STATUS_ERROR = "error"

# Tier indicators — styled by weight, colors from CSS theme
TIER_STYLES = {
    "core": ("bold", "CORE"),
    "fleet": ("", "FLEET"),
}

# Status icons with animation hint
STATUS_ICONS = {
    STATUS_IDLE: "[dim]○[/]",
    STATUS_ACTIVE: "[bold cyan]◉[/]",
    STATUS_ERROR: "[bold red]✗[/]",
}

ACTIVATION_ICONS = {
    "default": "[green]●[/]",
    "on-demand": "[yellow]◐[/]",
    "specialist": "[magenta]◆[/]",
}

# Phase icons for visual feedback
PHASE_ICONS = {
    PHASE_IDLE: "[dim]○[/]",
    PHASE_RESEARCH: "[cyan]◎[/]",
    PHASE_CODING: "[yellow]◉[/]",
    PHASE_REASONING: "[magenta]◈[/]",
    PHASE_COMPLETE: "[green]●[/]",
}

# Router cost tier indicators (from router.sh capability mapping)
# Visual distinction between cheap (Routine) and expensive (Reason) models
ROUTER_TIER_STYLES = {
    "routine": ("[green]$[/]", "Gemini Flash"),      # ~$0.10/1M - Cheap
    "data": ("[cyan]$$[/]", "DeepSeek"),             # ~$0.27/1M - Moderate
    "code": ("[yellow]$$$[/]", "Sonnet"),            # ~$3.00/1M - Standard
    "reason": ("[bold red]$$$$[/]", "Opus"),         # ~$15.00/1M - Expensive
}

# =============================================================================
# TOKEN ODOMETER & RESOURCE GOVERNANCE (2026 FinOps Pattern)
# =============================================================================

# Default budget thresholds per agent (in USD)
DEFAULT_BUDGET_SOFT = 2.00  # Yellow warning
DEFAULT_BUDGET_HARD = 5.00  # Red + soft error state

# Latency benchmarks (in seconds) - laptop-class targets
LATENCY_THRESHOLDS = {
    "excellent": 2.0,   # Green - Fast response
    "good": 5.0,        # Cyan - Acceptable
    "slow": 10.0,       # Yellow - Needs attention
    # Above slow = Red - Network/model issue
}

# Hierarchical supervision tiers (2026 Multi-Agent Pattern)
SUPERVISION_TIERS = {
    "supervisor": ["arbiter", "orchestrator"],  # High-level: goal progress view
    "coordinator": ["architect", "auditor", "strategist", "planner"],  # Domain leads
    "specialist": None,  # Everything else - worker agents
}

# Capability tag to router tier mapping (mirrors router.sh)
CAPABILITY_TO_TIER = {
    # Routine tier
    "heartbeat": "routine", "status": "routine", "health": "routine",
    "monitor": "routine", "ping": "routine",
    # Data tier
    "formatting": "data", "parsing": "data", "logging": "data",
    "metrics": "data", "telemetry": "data", "localization": "data",
    "i18n": "data", "transformation": "data",
    # Code tier
    "code-review": "code", "debugging": "code", "testing": "code",
    "refactoring": "code", "documentation": "code", "api-design": "code",
    "implementation": "code",
    # Reason tier
    "architecture": "reason", "planning": "reason", "security": "reason",
    "research": "reason", "audit": "reason", "synthesis": "reason",
    "strategy": "reason", "analysis": "reason",
}


def get_router_tier(capability_tags: list[str]) -> str:
    """Determine the router tier from capability tags (highest tier wins)."""
    tier_rank = {"routine": 1, "data": 2, "code": 3, "reason": 4}
    highest = "code"  # Default
    highest_rank = 3

    for tag in capability_tags:
        tier = CAPABILITY_TO_TIER.get(tag)
        if tier and tier_rank.get(tier, 0) > highest_rank:
            highest = tier
            highest_rank = tier_rank[tier]

    return highest


class AgentCard(Widget, can_focus=True):
    """A card representing a single agent in the fleet grid.

    Supports context-aware expansion where the UI adapts based on
    what the agent is currently doing (phase-based rendering).

    Active agents get a subtle "pulse" animation via timer-driven
    class toggling (since Textual doesn't support CSS @keyframes).
    """

    # CSS defined in etx.tcss for consistency
    DEFAULT_CSS = ""

    # Reactive properties for context-aware updates
    phase: reactive[str] = reactive(PHASE_IDLE)
    status: reactive[str] = reactive(STATUS_IDLE)  # idle/active/error
    _pulse_state: reactive[bool] = reactive(False)  # For breathing animation
    thought_snippet: reactive[str] = reactive("")
    links_found: reactive[int] = reactive(0)
    files_written: reactive[int] = reactive(0)
    progress_percent: reactive[int] = reactive(-1)  # -1 = indeterminate

    # Token Odometer (2026 FinOps: Resource Governance)
    token_cost_usd: reactive[float] = reactive(0.0)  # Cumulative cost in USD
    token_budget_usd: reactive[float] = reactive(DEFAULT_BUDGET_SOFT)  # Soft budget
    latency_ms: reactive[int] = reactive(0)  # Last response latency in ms
    request_count: reactive[int] = reactive(0)  # Total API requests

    class Selected(Message):
        """Posted when an agent card is selected."""

        def __init__(self, agent: Agent) -> None:
            super().__init__()
            self.agent = agent

    # ========================================================================
    # CONTEXTUAL RECOVERY ACTIONS — Error state quick actions (2026 UX)
    # ========================================================================

    class RestartRequested(Message):
        """Posted when user requests agent restart (R key on error state)."""

        def __init__(self, agent: Agent) -> None:
            super().__init__()
            self.agent = agent

    class LogsRequested(Message):
        """Posted when user requests agent logs (L key on error state)."""

        def __init__(self, agent: Agent) -> None:
            super().__init__()
            self.agent = agent

    class ErrorDismissed(Message):
        """Posted when user dismisses/acknowledges error (D key)."""

        def __init__(self, agent: Agent) -> None:
            super().__init__()
            self.agent = agent

    class ErrorDetailsRequested(Message):
        """Posted when user requests error details (I key)."""

        def __init__(self, agent: Agent) -> None:
            super().__init__()
            self.agent = agent

    class BudgetExceeded(Message):
        """Posted when agent exceeds token budget (2026 FinOps)."""

        def __init__(self, agent: Agent, cost: float, budget: float) -> None:
            super().__init__()
            self.agent = agent
            self.cost = cost
            self.budget = budget

    # Contextual bindings - only shown/active when agent is in error state
    BINDINGS = [
        Binding("r", "restart", "Restart", show=False),
        Binding("l", "logs", "Logs", show=False),
        Binding("d", "dismiss", "Dismiss", show=False),
        Binding("i", "details", "Details", show=False),
    ]

    def __init__(self, agent: Agent, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.agent = agent
        self._router_tier = ""
        self._pulse_timer: object | None = None  # Timer for breathing animation
        self._budget_warned = False  # Track if we've already warned
        if agent.tier == "core":
            self.add_class("tier-core")
        # Set supervision tier class
        self._supervision_tier = self._get_supervision_tier(agent.id)
        if self._supervision_tier:
            self.add_class(f"tier-{self._supervision_tier}")

    def _get_supervision_tier(self, agent_id: str) -> str:
        """Determine supervision tier for hierarchical display."""
        if agent_id in SUPERVISION_TIERS["supervisor"]:
            return "supervisor"
        elif agent_id in SUPERVISION_TIERS["coordinator"]:
            return "coordinator"
        return "specialist"

    def _format_odometer(self) -> str:
        """Format the token odometer display."""
        cost = self.token_cost_usd
        latency = self.latency_ms
        requests = self.request_count

        # Cost color based on budget
        budget_pct = (cost / self.token_budget_usd * 100) if self.token_budget_usd > 0 else 0
        if budget_pct >= 100:
            cost_color = "bold red"
            cost_icon = "🔴"
        elif budget_pct >= 75:
            cost_color = "yellow"
            cost_icon = "🟡"
        else:
            cost_color = "green"
            cost_icon = "🟢"

        # Latency color
        if latency == 0:
            latency_display = "[dim]--ms[/]"
        elif latency <= LATENCY_THRESHOLDS["excellent"] * 1000:
            latency_display = f"[green]{latency}ms[/]"
        elif latency <= LATENCY_THRESHOLDS["good"] * 1000:
            latency_display = f"[cyan]{latency}ms[/]"
        elif latency <= LATENCY_THRESHOLDS["slow"] * 1000:
            latency_display = f"[yellow]{latency}ms[/]"
        else:
            latency_display = f"[red]{latency}ms[/]"

        # Format cost display
        if cost < 0.01:
            cost_display = f"[{cost_color}]${cost:.3f}[/]"
        else:
            cost_display = f"[{cost_color}]${cost:.2f}[/]"

        return f"  {cost_icon} {cost_display} · {latency_display} · [dim]{requests} reqs[/]"

    def compose(self) -> ComposeResult:
        """Build the agent card display with avatar, status, tier badge, router cost indicator, and tags."""
        style, label = TIER_STYLES.get(self.agent.tier, ("dim", "FLEET"))

        # Determine router cost tier from capability tags
        capability_tags = getattr(self.agent, "capability_tags", []) or self.agent.tags
        self._router_tier = get_router_tier(capability_tags)
        cost_icon, model_hint = ROUTER_TIER_STYLES.get(self._router_tier, ("[yellow]$$$[/]", "Sonnet"))

        # Agent avatar (reflects active/standby state)
        yield CompactAvatar(
            self.agent.id,
            active=(self.status == STATUS_ACTIVE),
            id=f"avatar-{self.agent.id}",
            classes="agent-avatar",
        )

        yield Static(
            f"[bold]{self.agent.id}[/]  {cost_icon}",
            classes="agent-card-name",
            id=f"name-{self.agent.id}",
        )
        tier_text = f"[{style}]{label}[/]" if style else label
        yield Static(
            f"  {tier_text} · {_(self.agent.activation_label)}",
            classes="agent-card-tier",
        )
        # Show model hint and tags
        tags_display = " ".join(self.agent.tags[:3])
        yield Static(
            f"  [dim]{model_hint}[/] [italic dim]{tags_display}[/]",
            classes="agent-card-tags",
        )

        # Token Odometer (2026 FinOps: Resource Governance)
        yield Static(
            self._format_odometer(),
            id=f"odometer-{self.agent.id}",
            classes="agent-odometer",
        )

        # Context area (shown when active/expanded)
        with Vertical(classes="agent-context hidden", id=f"ctx-{self.agent.id}"):
            yield Static("", id=f"thought-{self.agent.id}", classes="agent-thought")
            yield Static("", id=f"counter-{self.agent.id}", classes="agent-counter")
            yield ProgressBar(
                total=100,
                show_eta=False,
                id=f"progress-{self.agent.id}",
                classes="agent-progress hidden",
            )

    def watch_status(self, new_status: str) -> None:
        """React to status changes by updating avatar, CSS class, and pulse animation.

        Visual hierarchy (2026 "highest-attention color" pattern):
        - Error: Red warning, no pulse, demands immediate attention
        - Active: Bright avatar + pulse animation, operational feedback
        - Idle: Dimmed avatar, reduced cognitive load
        """
        try:
            # Update avatar state (follows highest-attention pattern)
            avatar = self.query_one(f"#avatar-{self.agent.id}", CompactAvatar)
            if new_status == STATUS_ERROR:
                avatar.set_error(True)
            elif new_status == STATUS_ACTIVE:
                avatar.set_active(True)
            else:
                avatar.set_state("standby")

            # Update CSS class for styling
            self.remove_class("status-idle", "status-active", "status-error")
            self.add_class(f"status-{new_status}")

            # Pulse animation only for active (not error - errors need stable attention)
            if new_status == STATUS_ACTIVE:
                self._start_pulse()
            else:
                self._stop_pulse()
        except Exception:
            pass  # Widget may not be mounted

    def _start_pulse(self) -> None:
        """Start the breathing pulse animation for active agents.

        Respects reduced_motion setting for vestibular accessibility (2026 standard).
        """
        # Check if reduced motion is enabled
        try:
            from tui.core.config import ETXConfig
            config = ETXConfig.load()
            if config.reduced_motion:
                # Static indicator instead of animation
                self.add_class("pulse-on")
                return
        except Exception:
            pass

        if self._pulse_timer is None:
            self._pulse_timer = self.set_interval(0.8, self._toggle_pulse)
            self.add_class("pulse-on")

    def _stop_pulse(self) -> None:
        """Stop the breathing pulse animation."""
        if self._pulse_timer is not None:
            self._pulse_timer.stop()
            self._pulse_timer = None
        self.remove_class("pulse-on", "pulse-off")

    def _toggle_pulse(self) -> None:
        """Toggle between pulse states for breathing effect."""
        if self.has_class("pulse-on"):
            self.remove_class("pulse-on")
            self.add_class("pulse-off")
        else:
            self.remove_class("pulse-off")
            self.add_class("pulse-on")

    def watch_phase(self, new_phase: str) -> None:
        """React to phase changes by showing/hiding context widgets."""
        try:
            context = self.query_one(f"#ctx-{self.agent.id}")
            thought = self.query_one(f"#thought-{self.agent.id}", Static)
            counter = self.query_one(f"#counter-{self.agent.id}", Static)
            progress = self.query_one(f"#progress-{self.agent.id}", ProgressBar)

            if new_phase == PHASE_IDLE:
                context.add_class("hidden")
                self.remove_class("expanded")
            else:
                context.remove_class("hidden")
                self.add_class("expanded")

                if new_phase == PHASE_RESEARCH:
                    thought.update("")
                    counter.update(f"[cyan]Links found:[/] {self.links_found}")
                    progress.add_class("hidden")
                elif new_phase == PHASE_CODING:
                    thought.update("")
                    counter.update(f"[yellow]Files written:[/] {self.files_written}")
                    if self.progress_percent >= 0:
                        progress.remove_class("hidden")
                        progress.progress = self.progress_percent
                    else:
                        progress.add_class("hidden")
                        counter.update(f"[yellow]Files written:[/] {self.files_written} [dim italic](working...)[/]")
                elif new_phase == PHASE_REASONING:
                    # Show thought snippet for reason tier
                    if self.thought_snippet:
                        thought.update(f'[italic]"{self.thought_snippet[:60]}..."[/]')
                    else:
                        thought.update("[dim italic]Analyzing...[/]")
                    counter.update("")
                    progress.add_class("hidden")
                elif new_phase == PHASE_COMPLETE:
                    thought.update("[green]Complete[/]")
                    counter.update("")
                    progress.remove_class("hidden")
                    progress.progress = 100
        except Exception:
            pass  # Widget may not be mounted yet

    def watch_links_found(self, count: int) -> None:
        """Update links counter when in research phase."""
        if self.phase == PHASE_RESEARCH:
            try:
                counter = self.query_one(f"#counter-{self.agent.id}", Static)
                counter.update(f"[cyan]Links found:[/] {count}")
            except Exception:
                pass

    def watch_files_written(self, count: int) -> None:
        """Update files counter when in coding phase."""
        if self.phase == PHASE_CODING:
            try:
                counter = self.query_one(f"#counter-{self.agent.id}", Static)
                suffix = "" if self.progress_percent >= 0 else " [dim italic](working...)[/]"
                counter.update(f"[yellow]Files written:[/] {count}{suffix}")
            except Exception:
                pass

    def watch_thought_snippet(self, snippet: str) -> None:
        """Update thought display when in reasoning phase."""
        if self.phase == PHASE_REASONING and snippet:
            try:
                thought = self.query_one(f"#thought-{self.agent.id}", Static)
                display = snippet[:60] + "..." if len(snippet) > 60 else snippet
                thought.update(f'[italic]"{display}"[/]')
            except Exception:
                pass

    def watch_progress_percent(self, percent: int) -> None:
        """Update progress bar."""
        try:
            progress = self.query_one(f"#progress-{self.agent.id}", ProgressBar)
            if percent >= 0:
                progress.remove_class("hidden")
                progress.progress = percent
            else:
                progress.add_class("hidden")
        except Exception:
            pass

    # ========================================================================
    # TOKEN ODOMETER WATCHERS (2026 FinOps: Resource Governance)
    # ========================================================================

    def watch_token_cost_usd(self, cost: float) -> None:
        """Update odometer display and check budget."""
        self._update_odometer()
        # Check for budget exceeded (soft error state)
        if cost >= self.token_budget_usd and not self._budget_warned:
            self._budget_warned = True
            self.post_message(self.BudgetExceeded(self.agent, cost, self.token_budget_usd))
            # Visual indicator - add budget-exceeded class
            self.add_class("budget-exceeded")
            # Haptic feedback (2026 UX: macOS/Linux notifications)
            alert_budget_exceeded(self.agent.id, cost, self.token_budget_usd)

    def watch_latency_ms(self, _latency: int) -> None:
        """Update odometer display with new latency."""
        self._update_odometer()

    def watch_request_count(self, _count: int) -> None:
        """Update odometer display with new request count."""
        self._update_odometer()

    def _update_odometer(self) -> None:
        """Refresh the odometer display."""
        try:
            odometer = self.query_one(f"#odometer-{self.agent.id}", Static)
            odometer.update(self._format_odometer())
        except Exception:
            pass

    def update_metrics(
        self,
        cost_delta: float = 0.0,
        latency: int = 0,
        requests_delta: int = 0,
    ) -> None:
        """Update agent metrics from mesh state or telemetry."""
        if cost_delta > 0:
            self.token_cost_usd += cost_delta
        if latency > 0:
            self.latency_ms = latency
        if requests_delta > 0:
            self.request_count += requests_delta

    def reset_budget(self) -> None:
        """Reset budget warning state (after user acknowledgment)."""
        self._budget_warned = False
        self.remove_class("budget-exceeded")

    def set_active(
        self,
        phase: str = PHASE_IDLE,
        status: str = STATUS_IDLE,
        thought: str = "",
        links: int = 0,
        files: int = 0,
        progress: int = -1,
    ) -> None:
        """Convenience method to update all context at once."""
        self.thought_snippet = thought
        self.links_found = links
        self.files_written = files
        self.progress_percent = progress
        self.status = status  # Update status for live indicator
        self.phase = phase  # Set phase last to trigger watch

    def set_status(self, status: str) -> None:
        """Update just the status indicator (idle/active/error)."""
        self.status = status

    def on_click(self) -> None:
        """Post selection message on mouse click."""
        self.post_message(self.Selected(self.agent))

    def on_key(self, event: Key) -> None:
        """Post selection message on Enter key press."""
        if event.key == "enter":
            self.post_message(self.Selected(self.agent))
            event.stop()

    # ========================================================================
    # CONTEXTUAL RECOVERY ACTIONS — Only active in error state
    # ========================================================================

    def action_restart(self) -> None:
        """Request agent restart (R key) - only works in error state."""
        if self.status == STATUS_ERROR:
            self.post_message(self.RestartRequested(self.agent))

    def action_logs(self) -> None:
        """Request agent logs (L key) - only works in error state."""
        if self.status == STATUS_ERROR:
            self.post_message(self.LogsRequested(self.agent))

    def action_dismiss(self) -> None:
        """Dismiss/acknowledge error (D key) - only works in error state."""
        if self.status == STATUS_ERROR:
            self.status = STATUS_IDLE  # Clear error state
            self.post_message(self.ErrorDismissed(self.agent))

    def action_details(self) -> None:
        """Show error details (I key) - only works in error state."""
        if self.status == STATUS_ERROR:
            self.post_message(self.ErrorDetailsRequested(self.agent))

    def on_focus(self) -> None:
        """Show contextual action hints when focused in error state."""
        self._update_action_hints()

    def on_blur(self) -> None:
        """Hide contextual action hints when unfocused."""
        self._hide_action_hints()

    def _update_action_hints(self) -> None:
        """Show/hide action hints based on current state."""
        try:
            thought = self.query_one(f"#thought-{self.agent.id}", Static)
            if self.status == STATUS_ERROR:
                # Show contextual recovery actions
                thought.update(
                    "[bold red]Error[/] · "
                    "[dim]R[/] Restart  "
                    "[dim]L[/] Logs  "
                    "[dim]D[/] Dismiss  "
                    "[dim]I[/] Details"
                )
                # Ensure context area is visible
                context = self.query_one(f"#ctx-{self.agent.id}")
                context.remove_class("hidden")
                self.add_class("expanded")
        except Exception:
            pass

    def _hide_action_hints(self) -> None:
        """Hide action hints when card loses focus."""
        if self.status != STATUS_ERROR:
            return
        try:
            # Keep error indicator but hide expanded hints if not active
            thought = self.query_one(f"#thought-{self.agent.id}", Static)
            thought.update("[bold red]Error[/] [dim](focus for actions)[/]")
        except Exception:
            pass
