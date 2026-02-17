# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Fleet grid widget showing all agents organized by tier."""

from __future__ import annotations

from typing import TYPE_CHECKING, Any

from textual.containers import Container, Horizontal, VerticalScroll
from textual.message import Message
from textual.reactive import reactive
from textual.widget import Widget
from textual.widgets import Input, Static

from tui.widgets.agent_card import AgentCard

if TYPE_CHECKING:
    from textual.app import ComposeResult

    from tui.core.registry import Agent, Combo, FleetRegistry, Squad

SECTION_DESCRIPTIONS = {
    "core": "Always on. Strategy, review, orchestration.",
    "default": "Auto-deployed. Your everyday engineering team.",
    "ondemand": "Summoned by name. Growth, outreach, communication.",
    "specialist": "Deep domain expertise. Crypto, finance, audio, systems.",
}

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
    """Grid display of the full agent fleet, organized by tier."""

    DEFAULT_CSS = """
    FleetGrid {
        height: 1fr;
        layout: vertical;
    }
    """

    filter_text: reactive[str] = reactive("")

    def __init__(self, registry: FleetRegistry, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.registry = registry

    def compose(self) -> ComposeResult:
        """Build the fleet grid layout with search and scroll area."""
        with Container(id="fleet-search"):
            yield Input(
                placeholder="Search or run: agent/squad/combo [task]...",
                id="fleet-filter-input",
            )
        yield VerticalScroll(id="fleet-scroll")

    def on_mount(self) -> None:
        """Build the initial agent grid."""
        self._rebuild_grid()
        # Focus first card so / triggers the search binding instead of typing into Input
        self.call_later(self.focus_first_card)

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
            if self._matches_combo(combo):
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
            if squad.id == name:
                return ("squad", squad.id)
        for combo in self.registry.combos:
            if combo.id == name:
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
        self._rebuild_grid()

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
        scroll = self.query_one("#fleet-scroll", VerticalScroll)
        scroll.remove_children()

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
            if desc_text:
                desc = Static(
                    f"[dim italic]{desc_text}[/]",
                    classes="section-desc",
                )
                scroll.mount(desc)

            # Build rows of 4 cards
            for i in range(0, len(filtered), 4):
                row_agents = filtered[i : i + 4]
                row = Horizontal(classes="fleet-row")
                scroll.mount(row)
                for agent in row_agents:
                    card = AgentCard(agent, id=f"card-{agent.id}")
                    row.mount(card)

        # Squads and combos — also filterable
        self._mount_squads_and_combos(scroll)

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
