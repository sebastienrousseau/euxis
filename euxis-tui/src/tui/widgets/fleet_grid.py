# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Fleet grid widget showing all agents organized by tier."""

from __future__ import annotations

from typing import TYPE_CHECKING, Any

from textual.binding import Binding
from textual.containers import Container, Horizontal, VerticalScroll
from textual.message import Message
from textual.reactive import reactive
from textual.widget import Widget
from textual.widgets import Input, Static

from tui.widgets.agent_card import (
    AgentCard,
    STATUS_ACTIVE,
    STATUS_ERROR,
    SUPERVISION_TIERS,
)

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.core.registry import Agent, Combo, FleetRegistry, Squad

SECTION_DESCRIPTIONS = {
    "core": "Always on. Strategy, review, orchestration.",
    "default": "Auto-deployed. Your everyday engineering team.",
    "ondemand": "Summoned by name. Growth, outreach, communication.",
    "specialist": "Deep domain expertise. Crypto, finance, audio, systems.",
}

# Hierarchical view descriptions (2026 Multi-Agent Orchestration)
HIERARCHY_DESCRIPTIONS = {
    "supervisor": "High-level managers. Goal progress, arbitration, orchestration.",
    "coordinator": "Domain leads. Architecture, auditing, strategy, planning.",
    "specialist": "Worker agents. Execute narrow tasks—API calls, data retrieval.",
}

# Status-based grouping for 2026 UX pattern
STATUS_ACTIVE = "active"
STATUS_IDLE = "idle"

MIN_QUOTED_LENGTH = 2


class AgentSelected(Message):
    """Posted when an agent is selected from search."""

    def __init__(self, agent_id: str, task: str = "") -> None:
        super().__init__()
        self.agent_id = agent_id
        self.task = task


class SquadSelected(Message):
    """Posted when a squad row is clicked or selected from search."""

    def __init__(self, squad_id: str, task: str = "") -> None:
        super().__init__()
        self.squad_id = squad_id
        self.task = task


class ComboSelected(Message):
    """Posted when a combo row is clicked or selected from search."""

    def __init__(self, combo_id: str, task: str = "") -> None:
        super().__init__()
        self.combo_id = combo_id
        self.task = task


class SystemCommandRequested(Message):
    """Posted when a system command is entered in search."""

    def __init__(self, command: str) -> None:
        super().__init__()
        self.command = command


class BulkRestartRequested(Message):
    """Posted when user requests bulk restart of all failed agents (Shift+R)."""

    def __init__(self, agent_ids: list[str]) -> None:
        super().__init__()
        self.agent_ids = agent_ids


SYSTEM_COMMANDS = {
    "health", "certify", "lint", "cortex", "metrics",
    "settings", "help", "about", "playbooks", "refresh",
}


class _ClickableRow(Static, can_focus=True):
    """A focusable row that posts a message on click/enter."""

    def __init__(self, label: str, message: Message, **kwargs: Any) -> None:
        super().__init__(label, **kwargs)
        self._message = message

    def on_click(self) -> None:
        self.post_message(self._message)

    def action_select(self) -> None:
        self.post_message(self._message)

    BINDINGS = [("enter", "select", "Select")]


class FleetGrid(Widget):
    """Grid display of the full agent fleet, organized by tier.

    Performance optimizations:
    - Deferred mounting: Cards are mounted after initial screen paint
    - Batched updates: Rebuilds happen in batches to avoid blocking
    - Viewport-aware: Only full cards in viewport get expensive rendering

    Navigation:
    - Arrow keys or h/j/k/l for vim-style grid navigation
    - Enter to select focused card
    """

    DEFAULT_CSS = """
    FleetGrid {
        height: 1fr;
        layout: vertical;
    }
    """

    BINDINGS = [
        Binding("up", "nav_up", "Up", show=False),
        Binding("down", "nav_down", "Down", show=False),
        Binding("left", "nav_left", "Left", show=False),
        Binding("right", "nav_right", "Right", show=False),
        Binding("k", "nav_up", "Up", show=False),
        Binding("j", "nav_down", "Down", show=False),
        Binding("h", "nav_left", "Left", show=False),
        Binding("l", "nav_right", "Right", show=False),
        Binding("g", "goto_active", "Go to Active", show=False),
        # Bulk error recovery actions
        Binding("R", "restart_all_failed", "Restart All Failed", show=False),
        # View mode toggle (2026 Multi-Agent Orchestration)
        Binding("v", "toggle_view", "Toggle View", show=False),
    ]

    filter_text: reactive[str] = reactive("")
    # View mode: "activation" (Core/Default/OnDemand/Specialist) or "hierarchy" (Supervisor/Coordinator/Specialist)
    view_mode: reactive[str] = reactive("activation")

    # Grid layout: cards per row
    _CARDS_PER_ROW = 4

    def __init__(self, registry: FleetRegistry, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.registry = registry
        self._mounted = False
        self._rebuilding = False  # Guard against concurrent rebuilds

    def compose(self) -> ComposeResult:
        """Build the fleet grid layout with search, active indicator, and scroll area."""
        with Container(id="fleet-search"):
            yield Input(
                placeholder="/ to search, type agent/squad/combo [task]...",
                id="fleet-filter-input",
            )
        # Active agents breadcrumb - shows count and allows quick scroll
        yield Static(
            "",
            id="active-indicator",
            classes="active-indicator hidden",
        )
        yield VerticalScroll(id="fleet-scroll")

    def on_mount(self) -> None:
        """Build the initial agent grid."""
        if not self._mounted:
            self._mounted = True
            # Build grid only once on mount - watcher handles subsequent rebuilds
            self.call_later(self._initial_build)

    def _initial_build(self) -> None:
        """Deferred initial build to avoid race with reactive watchers."""
        if not self._rebuilding:
            self._rebuild_grid()
            self.focus_first_card()

    def on_input_changed(self, event: Input.Changed) -> None:
        """Update filter text when search input changes."""
        if event.input.id == "fleet-filter-input":
            self.filter_text = event.value

    # Tokens stripped from the front of search input before entity matching.
    # Supports full CLI syntax like: euxis combos run envision "Analyse this"
    _STRIP_PREFIXES = {"euxis"}
    _STRIP_CATEGORIES = {
        "combo", "combos", "squad", "squads", "agent", "agents",
        "run", "deploy", "execute", "start", "launch",
    }

    def on_input_submitted(self, event: Input.Submitted) -> None:
        """Launch the first matching entity when Enter is pressed.

        Accepts multiple input styles:
          envision Analyse this repo
          euxis combos run envision "Analyse this repo"
          run orchestrator Review the code
          health
        """
        if event.input.id != "fleet-filter-input":
            return

        query = event.value.strip()
        if not query:
            return

        entity_name, task = self._parse_search_command(query)

        handled = False

        # Try exact entity ID match
        match = self._find_exact_entity(entity_name)
        if match:
            entity_type, entity_id = match
            self._dispatch_selection(entity_type, entity_id, task)
            handled = True
        elif entity_name in SYSTEM_COMMANDS:
            # Try system commands (health, certify, lint, etc.)
            self.post_message(SystemCommandRequested(entity_name))
            handled = True
        else:
            # Fall back to first filtered result
            handled = self._dispatch_first_filtered()

        if handled:
            self._clear_search(event.input)

    def _dispatch_first_filtered(self) -> bool:
        """Dispatch the first matching entity from filtered results."""
        for agent in self.registry.agents:
            if self._matches_filter(agent):
                self.post_message(AgentSelected(agent.id))
                return True

        for squad in self.registry.squads:
            if self._matches_squad(squad):
                self.post_message(SquadSelected(squad.id))
                return True

        for combo in self.registry.combos:
            if self._matches_combo(combo):  # pragma: no cover
                self.post_message(ComboSelected(combo.id))
                return True

        return False

    def _parse_search_command(self, text: str) -> tuple[str, str]:
        """Parse search input, stripping CLI-style prefixes.

        Handles all of:
          envision Analyse this repo        → ("envision", "Analyse this repo")
          euxis combos run envision task    → ("envision", "task")
          run orchestrator Review code        → ("orchestrator", "Review code")
          health                              → ("health", "")
        """
        # Strip surrounding quotes from the whole input
        words = text.strip().split()
        # Drop known prefix tokens from the front
        while words and words[0].lower() in (self._STRIP_PREFIXES | self._STRIP_CATEGORIES):
            words.pop(0)

        if not words:
            return (text.strip().lower(), "")

        entity_name = words[0].lower()
        # Strip quotes from entity name (e.g. "envision")
        entity_name = entity_name.strip("\"'")
        task_words = words[1:]

        # Rejoin task, stripping outer quotes
        task = " ".join(task_words).strip()
        if len(task) >= MIN_QUOTED_LENGTH and task[0] == task[-1] and task[0] in "\"'":
            task = task[1:-1]

        return (entity_name, task)

    def _clear_search(self, input_widget: Input) -> None:
        """Reset the search bar after launching a command."""
        input_widget.value = ""
        self.filter_text = ""

    def _find_exact_entity(self, name: str) -> tuple[str, str] | None:
        """Find an agent, squad, or combo by exact ID match."""
        agent = self.registry.get_agent(name)
        if agent:
            return ("agent", agent.id)
        for squad in self.registry.squads:
            if squad.id == name:  # pragma: no cover
                return ("squad", squad.id)
        for combo in self.registry.combos:
            if combo.id == name:  # pragma: no cover
                return ("combo", combo.id)
        return None

    def _dispatch_selection(self, entity_type: str, entity_id: str, task: str = "") -> None:
        """Post the appropriate selection message for an entity."""
        if entity_type == "agent":
            self.post_message(AgentSelected(entity_id, task))
        elif entity_type == "squad":
            self.post_message(SquadSelected(entity_id, task))
        elif entity_type == "combo":
            self.post_message(ComboSelected(entity_id, task))

    def watch_filter_text(self) -> None:
        """Rebuild the grid when the filter text changes."""
        # Only rebuild if already mounted (avoid double-build on init)
        if self._mounted and not self._rebuilding:
            self._rebuild_grid()

    def watch_view_mode(self) -> None:
        """Rebuild the grid when view mode changes."""
        if self._mounted and not self._rebuilding:
            self._rebuild_grid()

    def action_toggle_view(self) -> None:
        """Toggle between activation and hierarchy view modes."""
        if self.view_mode == "activation":
            self.view_mode = "hierarchy"
            self.app.notify("View: Hierarchy (Supervisors → Coordinators → Specialists)", timeout=2)
        else:
            self.view_mode = "activation"
            self.app.notify("View: Activation (Core → Default → On-Demand → Specialist)", timeout=2)

    def _matches_filter(self, agent: Agent) -> bool:
        if not self.filter_text:
            return True
        query = self.filter_text.lower()
        return (
            query in agent.id.lower()
            or any(query in t.lower() for t in agent.tags)
            or query in agent.tier.lower()
            or query in agent.activation.lower()
        )

    def _rebuild_grid(self) -> None:
        """Rebuild the entire grid. Protected against concurrent calls."""
        if self._rebuilding:
            return
        self._rebuilding = True

        try:
            scroll = self.query_one("#fleet-scroll", VerticalScroll)
            scroll.remove_children()

            # Get active and error agent IDs from mesh state (shared across views)
            active_ids = self._get_active_agent_ids()
            error_ids = self._get_error_agent_ids()

            if self.view_mode == "hierarchy":
                self._rebuild_hierarchy_view(scroll, active_ids, error_ids)
            else:
                self._rebuild_activation_view(scroll, active_ids, error_ids)

            # Squads and combos — also filterable
            self._mount_squads_and_combos(scroll)

            # Update active indicator breadcrumb
            self.call_later(self.update_active_indicator)
        finally:
            self._rebuilding = False

    def _rebuild_activation_view(
        self,
        scroll: VerticalScroll,
        active_ids: set[str],
        error_ids: set[str],
    ) -> None:
        """Build grid grouped by activation tier (original view)."""
        sections = [
            ("Core Agents", self.registry.core_agents, "core"),
            ("Default Agents", self.registry.default_agents, "default"),
            ("On-Demand Agents", self.registry.ondemand_agents, "ondemand"),
            ("Specialist Agents", self.registry.specialist_agents, "specialist"),
        ]

        for title, agents, section_id in sections:
            filtered = [a for a in agents if self._matches_filter(a)]
            if not filtered:
                continue

            label = Static(
                f"[bold]{title}[/] [dim]({len(filtered)})[/]",
                classes="section-title",
            )
            scroll.mount(label)

            desc_text = SECTION_DESCRIPTIONS.get(section_id, "")
            if desc_text:  # pragma: no cover
                desc = Static(
                    f"[dim italic]{desc_text}[/]",
                    classes="section-desc",
                )
                scroll.mount(desc)

            self._mount_agent_cards(scroll, filtered, active_ids, error_ids)

    def _rebuild_hierarchy_view(
        self,
        scroll: VerticalScroll,
        active_ids: set[str],
        error_ids: set[str],
    ) -> None:
        """Build grid grouped by supervision hierarchy (2026 Multi-Agent Pattern)."""
        # Categorize all agents by supervision tier
        all_agents = list(self.registry.agents)
        supervisors = []
        coordinators = []
        specialists = []

        for agent in all_agents:
            if not self._matches_filter(agent):
                continue
            if agent.id in SUPERVISION_TIERS["supervisor"]:
                supervisors.append(agent)
            elif agent.id in SUPERVISION_TIERS["coordinator"]:
                coordinators.append(agent)
            else:
                specialists.append(agent)

        # View mode indicator
        view_indicator = Static(
            "[bold cyan]⬡[/] [bold]Hierarchy View[/] [dim](V to toggle)[/]",
            classes="hierarchy-title",
        )
        scroll.mount(view_indicator)

        # Mount supervisors (high-level managers)
        if supervisors:
            label = Static(
                f"[bold magenta]◈ Supervisors[/] [dim]({len(supervisors)})[/]",
                classes="section-title hierarchy-supervisors",
            )
            scroll.mount(label)
            desc = Static(
                f"[dim italic]{HIERARCHY_DESCRIPTIONS['supervisor']}[/]",
                classes="section-desc",
            )
            scroll.mount(desc)
            self._mount_agent_cards(scroll, supervisors, active_ids, error_ids)

        # Mount coordinators (domain leads)
        if coordinators:
            label = Static(
                f"[bold cyan]◉ Coordinators[/] [dim]({len(coordinators)})[/]",
                classes="section-title hierarchy-coordinators",
            )
            scroll.mount(label)
            desc = Static(
                f"[dim italic]{HIERARCHY_DESCRIPTIONS['coordinator']}[/]",
                classes="section-desc",
            )
            scroll.mount(desc)
            self._mount_agent_cards(scroll, coordinators, active_ids, error_ids)

        # Mount specialists (workers)
        if specialists:
            label = Static(
                f"[bold green]○ Specialists[/] [dim]({len(specialists)})[/]",
                classes="section-title",
            )
            scroll.mount(label)
            desc = Static(
                f"[dim italic]{HIERARCHY_DESCRIPTIONS['specialist']}[/]",
                classes="section-desc",
            )
            scroll.mount(desc)
            self._mount_agent_cards(scroll, specialists, active_ids, error_ids)

    def _mount_agent_cards(
        self,
        scroll: VerticalScroll,
        agents: list[Any],
        active_ids: set[str],
        error_ids: set[str],
    ) -> None:
        """Mount agent cards in rows of 4 with status indicators."""
        for i in range(0, len(agents), self._CARDS_PER_ROW):
            row_agents = agents[i : i + self._CARDS_PER_ROW]
            row = Horizontal(classes="fleet-row")
            scroll.mount(row)
            for agent in row_agents:
                card = AgentCard(agent, id=f"card-{agent.id}")
                row.mount(card)
                # Set status AFTER mounting (pulse timer needs mounted widget)
                # Error state takes priority over active (highest attention)
                if agent.id in error_ids:
                    card.status = STATUS_ERROR
                elif agent.id in active_ids:
                    card.status = STATUS_ACTIVE

    def _get_active_agent_ids(self) -> set[str]:
        """Get IDs of currently active agents from mesh state.

        Falls back to demo agents (arbiter, orchestrator) if no mesh state exists,
        so users can see the pulse animation in action.
        """
        import json
        import os

        active_ids: set[str] = set()
        euxis_home = os.environ.get("EUXIS_HOME", os.path.expanduser("~/.euxis"))
        state_file = os.path.join(euxis_home, "euxis-runtime/mesh/state.json")

        try:
            with open(state_file) as f:
                state = json.load(f)
            agents = state.get("agents", {})
            for agent_id, info in agents.items():
                if info.get("status") == "online":
                    active_ids.add(agent_id)
        except (FileNotFoundError, json.JSONDecodeError, KeyError):
            # Demo mode: show pulse on a couple of core agents
            # Remove this fallback for production
            active_ids = {"arbiter", "orchestrator"}

        return active_ids

    def _get_error_agent_ids(self) -> set[str]:
        """Get IDs of agents in error state from mesh state.

        Falls back to demo error agent (guard) if no mesh state exists,
        so users can test the contextual recovery actions.
        """
        import json
        import os

        error_ids: set[str] = set()
        euxis_home = os.environ.get("EUXIS_HOME", os.path.expanduser("~/.euxis"))
        state_file = os.path.join(euxis_home, "euxis-runtime/mesh/state.json")

        try:
            with open(state_file) as f:
                state = json.load(f)
            agents = state.get("agents", {})
            for agent_id, info in agents.items():
                if info.get("status") == "error":
                    error_ids.add(agent_id)
        except (FileNotFoundError, json.JSONDecodeError, KeyError):
            # Demo mode: show error state on guard for testing recovery actions
            # Remove this fallback for production
            error_ids = {"guard"}

        return error_ids

    def _mount_agent_rows(
        self,
        scroll: VerticalScroll,
        agents: list[Agent],
        is_active: bool = False,
    ) -> None:
        """Mount agent cards in rows of 4."""
        row_class = "fleet-row fleet-row-active" if is_active else "fleet-row fleet-row-standby"
        for i in range(0, len(agents), self._CARDS_PER_ROW):
            row_agents = agents[i : i + self._CARDS_PER_ROW]
            row = Horizontal(classes=row_class)
            scroll.mount(row)
            for agent in row_agents:
                card = AgentCard(agent, id=f"card-{agent.id}")
                if is_active:
                    card.add_class("status-active")
                else:
                    card.add_class("status-standby")
                row.mount(card)

    def _matches_squad(self, squad: Squad) -> bool:
        """Check if a squad matches the current filter."""
        if not self.filter_text:
            return True
        query = self.filter_text.lower()
        return (
            query in squad.id.lower()
            or query in squad.name.lower()
            or query in squad.purpose.lower()
            or query in squad.lead.lower()
            or any(query in m.lower() for m in squad.members)
        )

    def _matches_combo(self, combo: Combo) -> bool:
        """Check if a combo matches the current filter."""
        if not self.filter_text:
            return True
        query = self.filter_text.lower()
        return (
            query in combo.id.lower()
            or query in combo.name.lower()
            or query in combo.description.lower()
            or any(query in a.lower() for a in combo.chain)
        )

    def _mount_squads_and_combos(self, scroll: VerticalScroll) -> None:
        """Mount squad and combo sections below agent tiers."""
        squads = [s for s in self.registry.squads if self._matches_squad(s)]
        combos = [c for c in self.registry.combos if self._matches_combo(c)]
        if not squads and not combos:
            return

        # Visual divider between agents and teams
        divider = Static(
            "[dim]" + "─" * 60 + "[/]",
            classes="fleet-divider",
        )
        scroll.mount(divider)

        if squads:
            label = Static(
                f"[bold]Squads[/] [dim]({len(squads)})[/]",
                classes="section-title",
            )
            scroll.mount(label)
            desc = Static(
                "[dim italic]Coordinated teams. Deploy together, move together.[/]",
                classes="section-desc",
            )
            scroll.mount(desc)

            for squad in squads:
                text = (
                    f"[bold]▸ {squad.name}[/]  [dim]·[/]  "
                    f"{squad.purpose}  [dim]·[/]  "
                    f"{len(squad.members)} agents"
                )
                row = _ClickableRow(
                    text,
                    SquadSelected(squad.id),
                    classes="squad-row",
                )
                scroll.mount(row)

        if combos:
            label = Static(
                f"[bold]Combos[/] [dim]({len(combos)})[/]",
                classes="section-title",
            )
            scroll.mount(label)
            desc = Static(
                "[dim italic]Sequential chains. Each agent builds on the last.[/]",
                classes="section-desc",
            )
            scroll.mount(desc)

            for combo in combos:
                text = (
                    f"[bold]▸ {combo.name}[/]  [dim]·[/]  "
                    f"{combo.description}  [dim]·[/]  "
                    f"{len(combo.chain)} stages"
                )
                row = _ClickableRow(
                    text,
                    ComboSelected(combo.id),
                    classes="combo-row",
                )
                scroll.mount(row)

    def focus_first_card(self) -> None:
        """Focus the first agent card."""
        cards = self.query(AgentCard)
        if cards:
            cards.first().focus()

    def _get_card_list(self) -> list[AgentCard]:
        """Get all agent cards in display order."""
        return list(self.query(AgentCard))

    def _get_focused_index(self) -> int:
        """Get the index of the currently focused card, or -1 if none."""
        cards = self._get_card_list()
        focused = self.screen.focused
        for i, card in enumerate(cards):
            if card is focused:
                return i
        return -1

    def _focus_card_at(self, index: int) -> None:
        """Focus the card at the given index (with bounds checking)."""
        cards = self._get_card_list()
        if not cards:
            return
        # Clamp index to valid range
        index = max(0, min(index, len(cards) - 1))
        cards[index].focus()
        # Scroll card into view
        cards[index].scroll_visible()

    def action_nav_up(self) -> None:
        """Navigate to card above (previous row, same column)."""
        current = self._get_focused_index()
        if current < 0:
            self.focus_first_card()
            return
        # Move up one row
        new_index = current - self._CARDS_PER_ROW
        if new_index >= 0:
            self._focus_card_at(new_index)

    def action_nav_down(self) -> None:
        """Navigate to card below (next row, same column)."""
        current = self._get_focused_index()
        if current < 0:
            self.focus_first_card()
            return
        cards = self._get_card_list()
        # Move down one row
        new_index = current + self._CARDS_PER_ROW
        if new_index < len(cards):
            self._focus_card_at(new_index)

    def action_nav_left(self) -> None:
        """Navigate to card on the left."""
        current = self._get_focused_index()
        if current < 0:
            self.focus_first_card()
            return
        if current > 0:
            self._focus_card_at(current - 1)

    def action_nav_right(self) -> None:
        """Navigate to card on the right."""
        current = self._get_focused_index()
        if current < 0:
            self.focus_first_card()
            return
        cards = self._get_card_list()
        if current < len(cards) - 1:
            self._focus_card_at(current + 1)

    # ========================================================================
    # ACTIVE AGENTS BREADCRUMB — Quick navigation to running agents
    # ========================================================================

    def update_active_indicator(self) -> None:
        """Update the active agents breadcrumb indicator."""
        try:
            indicator = self.query_one("#active-indicator", Static)
            active_cards = [c for c in self.query(AgentCard) if c.status == STATUS_ACTIVE]

            if active_cards:
                names = ", ".join(c.agent.id for c in active_cards[:3])
                suffix = f" +{len(active_cards) - 3}" if len(active_cards) > 3 else ""
                indicator.update(
                    f"[bold green]● {len(active_cards)} Active[/]  [dim]({names}{suffix})[/]  "
                    f"[dim italic]Press [bold]g[/] to jump[/]"
                )
                indicator.remove_class("hidden")
            else:
                indicator.add_class("hidden")
        except Exception:
            pass  # Widget may not be mounted

    def action_goto_active(self) -> None:
        """Jump to the first active agent card."""
        active_cards = [c for c in self.query(AgentCard) if c.status == STATUS_ACTIVE]
        if active_cards:
            active_cards[0].focus()
            active_cards[0].scroll_visible()

    def action_restart_all_failed(self) -> None:
        """Restart all agents in error state (Shift+R bulk action)."""
        error_cards = [c for c in self.query(AgentCard) if c.status == STATUS_ERROR]
        if error_cards:
            agent_ids = [c.agent.id for c in error_cards]
            self.post_message(BulkRestartRequested(agent_ids))
