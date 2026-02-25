# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

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
for the Euxis agent fleet. Deploy AI specialists, monitor operations,
and manage your fleet — all from the terminal.

---

## Pro Tips

- **Command palette is your hub** — press `/` or `Ctrl+K` or `Ctrl+P` to access everything
- **Use prefixes** — `@agent`, `#squad`, `>command` narrow searches instantly
- **Vim navigation** — use `h/j/k/l` to navigate between panes
- **Maximize any pane** — press `z` to zoom the focused pane full-screen
- **Theme cycling** — `F3` rotates through 24 themes
- **Refresh anytime** — `F5` reloads the fleet registry

---

## Keyboard Reference

### Navigation

| Shortcut | Action |
|----------|--------|
| `/` | Open command palette (fuzzy search) |
| `Ctrl+K` | Open command palette (alias) |
| `Ctrl+P` | Open command palette (alias) |
| `Tab` / `j` / `l` | Next focusable element |
| `Shift+Tab` / `k` / `h` | Previous focusable element |
| `z` | Maximize/zoom focused pane |
| `Arrow Keys` | Navigate within groups |
| `Enter` | Activate / select |
| `Escape` | Go back / dismiss |
| `?` | Show help (keyboard reference) |
| `F5` | Refresh fleet registry |

### Screens

| Shortcut | Action |
|----------|--------|
| `F3` | Cycle theme (24 themes) |
| `F4` | Open settings |
| `Ctrl+M` | Open fleet monitor |
| `F6` | Open log viewer |
| `Ctrl+B` | Browse playbooks |
| `F7` | Router status |
| `Ctrl+Q` | Quit ETX |
| `F1` | Show this help |
| `F2` | Return to welcome screen |

### Screens Available

| Screen | Access | Description |
|--------|--------|-------------|
| Welcome | `F2` | Splash screen with fleet stats |
| Dashboard | Default | Fleet grid with all agents, squads, and combos |
| Agent Execution | Click agent / command palette | Deploy and stream agent output |
| Fleet Monitor | `Ctrl+M` / command palette | Monitor squad/dispatch operations with CPU/RAM sparklines |
| Settings | `F4` / command palette | Theme, provider, accessibility |
| Log Viewer | `F6` / command palette | Browse agent output history |
| Playbook Browser | `Ctrl+B` / command palette | Inspect playbook gate structure |
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

### Log Viewer

| Shortcut | Action |
|----------|--------|
| `/` | Focus search |
| `t` | Toggle live tail |
| `c` | Clear search filter |
| `g` | Scroll to top |
| `G` | Scroll to bottom |
| `Escape` | Return to previous screen |

### Command Palette Prefixes

| Prefix | Category | Example |
|--------|----------|---------|
| *(none)* | Search all | `architect` |
| `@` | Agents | `@pentester` |
| `#` | Squads | `#quality` |
| `>` | System commands | `>health`, `>mesh`, `>router` |

---

## Quick Start

1. **Search**: Press `Ctrl+K` to open the command palette
2. **Deploy**: Type an agent name and press `Enter`
3. **Execute**: Enter your task and press `Enter`
4. **Monitor**: Use `Ctrl+M` to watch fleet operations
5. **Review**: Use `F6` to browse output history

---

## Agent Tiers

- **CORE** (12 agents): Define direction and block progress when needed
- **DEFAULT** (24 agents): Execute domain work automatically
- **ON-DEMAND** (10 agents): Growth and communication tasks
- **SPECIALIST** (4 agents): Deep domain expertise

## Squads

| Squad | Purpose | Members |
|-------|---------|---------|
| Vision | Strategy & Research | orchestrator, architect, planner, deep-researcher, researcher, historian, accountant |
| Build | Engineering & Execution | debugger, maintainer, automaton, tester, investigator, repairer |
| Quality | Assurance & Security | reviewer, inspector, sentinel, pentester, auditor, optimizer, watchdog, polyglot, arbiter |
| Growth | Branding & Documentation | writer, evangelist, strategist, ambassador, marketer, localizer |
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
