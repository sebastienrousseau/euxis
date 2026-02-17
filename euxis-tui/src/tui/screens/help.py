# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Help screen with keyboard shortcut reference, pro tips, and quick start guide."""

from __future__ import annotations

from typing import TYPE_CHECKING

from textual.containers import VerticalScroll
from textual.screen import Screen
from textual.widgets import Markdown

from tui.i18n import _
from tui.widgets.header import ETXHeader
from tui.widgets.shortcut_bar import ShortcutBar

if TYPE_CHECKING:
    from textual.app import ComposeResult

HELP_CONTENT = """\
# Welcome to ETX

**ETX** (Euxis Terminal Experience) is a keyboard-first terminal interface
for the Euxis 42-agent fleet. Deploy AI specialists, monitor operations,
and manage your fleet — all from the terminal.

---

## Pro Tips

- **Command palette is your hub** — press `Ctrl+K` to access everything
- **Use prefixes** — `@agent`, `#squad`, `>command` narrow searches instantly
- **Theme cycling** — `Ctrl+T` rotates Dark → Light → Contrast
- **Quick search** — press `/` on the dashboard to filter agents
- **Refresh anytime** — `F5` reloads the fleet registry

---

## Keyboard Reference

### Navigation

| Shortcut | Action |
|----------|--------|
| `Ctrl+K` | Open command palette |
| `Tab` | Next focusable element |
| `Shift+Tab` | Previous focusable element |
| `Arrow Keys` | Navigate within groups |
| `Enter` | Activate / select |
| `Escape` | Go back / dismiss |
| `?` | Show help (keyboard reference) |
| `/` | Focus search on dashboard |
| `F5` | Refresh fleet registry |

### Screens

| Shortcut | Action |
|----------|--------|
| `Ctrl+T` | Cycle theme (Glass → Light → Contrast) |
| `Ctrl+S` | Open settings |
| `Ctrl+M` | Open fleet monitor |
| `Ctrl+O` | Open log viewer |
| `Ctrl+P` | Browse playbooks |
| `Ctrl+Q` | Quit ETX |
| `F1` | Show this help |
| `F2` | Return to welcome screen |

### Screens Available

| Screen | Access | Description |
|--------|--------|-------------|
| Welcome | `F2` | Splash screen with fleet stats |
| Dashboard | Default | Fleet grid with all 42 agents, squads, and combos |
| Agent Execution | Click agent / command palette | Deploy and stream agent output |
| Fleet Monitor | `Ctrl+M` / command palette | Monitor squad/dispatch operations |
| Settings | `Ctrl+S` / command palette | Theme, provider, accessibility |
| Log Viewer | `Ctrl+O` / command palette | Browse agent output history |
| Playbook Browser | `Ctrl+P` / command palette | Inspect playbook gate structure |
| Metrics Dashboard | Command palette | Performance sparklines and stats |
| Squad Details | Command palette | Squad compositions and combo chains |
| Cortex Memory | Command palette | Browse and query semantic memory |
| Help | `F1` / command palette | This keyboard reference |
| About | Command palette | Version and system information |

### Agent Execution

| Shortcut | Action |
|----------|--------|
| `Enter` | Submit task |
| `Ctrl+L` | Clear output |
| `Escape` | Return to dashboard |

### Command Palette Prefixes

| Prefix | Category | Example |
|--------|----------|---------|
| *(none)* | Search all | `architect` |
| `@` | Agents | `@pentester` |
| `#` | Squads | `#quality` |
| `>` | System commands | `>health` |

---

## Quick Start

1. **Search**: Press `Ctrl+K` to open the command palette
2. **Deploy**: Type an agent name and press `Enter`
3. **Execute**: Enter your task and press `Enter`
4. **Monitor**: Use `Ctrl+M` to watch fleet operations
5. **Review**: Use `Ctrl+O` to browse output history

---

## Agent Tiers

- **CORE** (9 agents): Define direction and block progress when needed
- **DEFAULT** (21 agents): Execute domain work automatically
- **ON-DEMAND** (8 agents): Growth and communication tasks
- **SPECIALIST** (4 agents): Deep domain expertise

## Squads

| Squad | Purpose | Members |
|-------|---------|---------|
| Vision | Strategy & Discovery | orchestrator, architect, planner, deep-researcher |
| Build | Engineering & Execution | debugger, maintainer, automaton, tester |
| Quality | Assurance & Security | reviewer, inspector, sentinel, pentester |
| Growth | Branding & Documentation | writer, evangelist, strategist, ambassador |
| Experience | UI Excellence | designer, tactician, animator, interactor |
| Specialist | Domain Expertise | cryptographer, ledger, conduit, custodian |

## Combos

| Combo | Chain |
|-------|-------|
| Envision | deep-researcher → planner → architect → evangelist → reviewer |
| Protect | sentinel → pentester → auditor → inspector → reviewer |
| Craft | writer → strategist → evangelist → reviewer |
| Refine | designer → animator → interactor → reviewer |
| Seal | sentinel → cryptographer → pentester → reviewer |
| Settle | ledger → auditor → tester → reviewer |
| Inspect | researcher → polyglot → watchdog → pentester → reviewer |
| Discover | deep-researcher → architect → reviewer |
"""


class HelpScreen(Screen[None]):
    """Comprehensive help and keyboard shortcut reference."""

    BINDINGS = [
        ("escape", "go_back", "Back"),
        ("q", "go_back", "Close"),
    ]

    def compose(self) -> ComposeResult:
        """Build the help screen layout."""
        yield ETXHeader(id="header")
        with VerticalScroll():
            yield Markdown(HELP_CONTENT, id="help-content")

        yield ShortcutBar()

    def on_mount(self) -> None:
        """Configure header for help screen."""
        header = self.query_one(ETXHeader)
        header.project = _("Help")

    def action_go_back(self) -> None:
        """Return to the previous screen."""
        self.app.pop_screen()
