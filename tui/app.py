# (c) 2026 Euxis Fleet. All rights reserved.
"""ETX: Euxis Terminal Experience — Main Application.

The ETX application provides a modern, keyboard-first terminal interface
for the Euxis 41-agent fleet. Built on Textual with custom theming,
command palette, and streaming agent execution.
"""

from __future__ import annotations

from typing import Any

from textual.app import App, InvalidThemeError
from textual.binding import Binding

from tui.commands import AgentCommandProvider, SquadCommandProvider, SystemCommandProvider
from tui.core.config import ETXConfig
from tui.i18n import _
from tui.themes import ETX_THEMES, THEME_CYCLE
from tui.core.registry import FleetRegistry
from tui.core.runner import get_git_branch, get_project_name
from tui.screens.dashboard import DashboardScreen


TIPS = [
    "Press Ctrl+K to open the command palette",
    "Type @ to search agents by name",
    "Type # to search squads and combos",
    "Use Ctrl+T to cycle between themes",
    "Press / on the dashboard to focus search",
    "Use Ctrl+M to open the fleet monitor",
    "Press F1 for the full keyboard reference",
    "Use Ctrl+P to browse playbooks",
    "Press F5 to refresh the fleet registry",
    "Use Ctrl+O to view agent output logs",
    "Deploy a squad for multi-agent operations",
    "Press Escape to go back from any screen",
    "Use Ctrl+S to open settings",
    "Squads and combos are shown below agents — scroll down on the dashboard",
    "Press F2 to return to the welcome screen anytime",
    "Press ? to see all keyboard shortcuts",
    "Playbooks define multi-gate quality pipelines — Ctrl+P to browse",
]


class EuxisApp(App):
    """ETX: Euxis Terminal Experience.

    A modern terminal interface for the Euxis agent fleet.
    41 AI specialists, accessible via keyboard-first design.
    """

    TITLE = "Euxis ETX"
    SUB_TITLE = "Enterprise Agent Fleet"

    CSS_PATH = "etx.tcss"

    COMMANDS = {
        AgentCommandProvider, SquadCommandProvider, SystemCommandProvider,
    }

    BINDINGS = [
        Binding("ctrl+k", "command_palette", "Commands", show=True, priority=True),
        Binding("ctrl+q", "quit", "Quit", show=True),
        Binding("ctrl+t", "toggle_theme", "Theme", show=True),
        Binding("ctrl+s", "open_settings", "Settings"),
        Binding("ctrl+m", "fleet_monitor_screen", "Monitor"),
        Binding("ctrl+o", "open_logs", "Logs"),
        Binding("ctrl+p", "open_playbooks", "Playbooks"),
        Binding("f1", "help", "Help"),
        Binding("f2", "welcome", "Welcome"),
        Binding("f5", "refresh", "Refresh"),
        Binding("question_mark", "help", "?", show=False),
    ]

    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)
        self.config = ETXConfig.load()
        self.fleet_registry = FleetRegistry.load()
        self.project_name = get_project_name()
        self.git_branch = get_git_branch()

        # Register custom Liquid Glass themes and apply immediately
        for theme in ETX_THEMES.values():
            self.register_theme(theme)

        # Set theme in __init__ so it's active before first render
        try:
            self.theme = self.config.theme or "etx-liquid-glass"
        except (ValueError, KeyError, InvalidThemeError):
            self.theme = "etx-liquid-glass"

        # Apply saved locale
        from tui.i18n import set_locale
        set_locale(self.config.locale)

    def on_mount(self) -> None:
        """Handle application mount."""
        # Push the dashboard as the default screen
        self._welcome_shown = False
        self.push_screen(DashboardScreen())

    def action_toggle_theme(self) -> None:
        """Cycle through all ETX themes."""
        try:
            idx = THEME_CYCLE.index(self.theme)
            next_theme = THEME_CYCLE[(idx + 1) % len(THEME_CYCLE)]
        except ValueError:
            next_theme = THEME_CYCLE[0]
        self.theme = next_theme
        self.config.theme = next_theme
        self.config.save()
        self.notify(_("Theme: {}").format(self.theme), timeout=2)

    def action_deploy_agent(self, agent_id: str, task: str = "") -> None:
        """Open the agent execution screen for a specific agent."""
        from tui.screens.agent import AgentScreen

        agent = self.fleet_registry.get_agent(agent_id)
        if not agent:
            self.notify(_("Unknown agent: {}").format(agent_id), severity="error")
            return

        self.push_screen(AgentScreen(agent, self.config.default_provider, initial_task=task))

    def action_deploy_squad(self, squad_id: str, task: str = "") -> None:
        """Open the fleet monitor for a squad deployment."""
        from tui.screens.fleet_monitor import FleetMonitorScreen

        squad = None
        for s in self.fleet_registry.squads:
            if s.id == squad_id:
                squad = s
                break

        if not squad:
            self.notify(_("Unknown squad: {}").format(squad_id), severity="error")
            return

        self.push_screen(
            FleetMonitorScreen(
                operation_type="squad",
                operation_id=squad_id,
                members=list(squad.members),
                task_description=task,
            )
        )

    def action_deploy_combo(self, combo_id: str, task: str = "") -> None:
        """Open the fleet monitor for a combo chain."""
        from tui.screens.fleet_monitor import FleetMonitorScreen

        combo = None
        for c in self.fleet_registry.combos:
            if c.id == combo_id:
                combo = c
                break

        if not combo:
            self.notify(_("Unknown combo: {}").format(combo_id), severity="error")
            return

        self.push_screen(
            FleetMonitorScreen(
                operation_type="combo",
                operation_id=combo_id,
                members=list(combo.chain),
                task_description=task,
            )
        )

    def action_open_settings(self) -> None:
        """Open the settings screen."""
        from tui.screens.settings import SettingsScreen

        self.push_screen(SettingsScreen())

    def action_fleet_monitor_screen(self) -> None:
        """Open fleet monitor for active operations."""
        from tui.screens.fleet_monitor import FleetMonitorScreen

        self.push_screen(
            FleetMonitorScreen(
                operation_type="fleet",
                operation_id="overview",
                members=[a.id for a in self.fleet_registry.agents[:8]],
            )
        )

    def action_open_logs(self) -> None:
        """Open the log viewer screen."""
        from tui.screens.logs import LogViewerScreen

        self.push_screen(LogViewerScreen())

    def action_open_playbooks(self) -> None:
        """Open the playbook browser."""
        from tui.screens.playbooks import PlaybookScreen

        self.push_screen(PlaybookScreen())

    def action_open_cortex(self) -> None:
        """Open the Cortex memory browser."""
        from tui.screens.cortex import CortexScreen

        self.push_screen(CortexScreen())

    def action_open_metrics(self) -> None:
        """Open the performance metrics dashboard."""
        from tui.screens.metrics import MetricsScreen

        self.push_screen(MetricsScreen())

    def action_open_squad_detail(self) -> None:
        """Open the squad and combo detail view."""
        from tui.screens.squad_detail import SquadDetailScreen

        self.push_screen(SquadDetailScreen())

    def action_welcome(self) -> None:
        """Return to the welcome screen."""
        from tui.screens.welcome import WelcomeScreen

        self.push_screen(WelcomeScreen())

    def action_help(self) -> None:
        """Open the help screen."""
        from tui.screens.help import HelpScreen

        self.push_screen(HelpScreen())

    def action_about(self) -> None:
        """Open the about screen."""
        from tui.screens.about import AboutScreen

        self.push_screen(AboutScreen())

    def action_refresh(self) -> None:
        """Reload registry and refresh display."""
        self.fleet_registry = FleetRegistry.load()
        self.project_name = get_project_name()
        self.git_branch = get_git_branch()
        self.notify(_("Fleet registry refreshed"), timeout=2)

    def run_system_command(self, command: str) -> None:
        """Execute a system command from the command palette."""
        from tui.screens.tool_runner import ToolRunnerScreen

        tool_map = {
            "health": ("euxis-health", "Fleet Health Check"),
            "certify": ("euxis-certify", "Certification Pipeline"),
            "lint": ("euxis-lint", "Fleet Lint"),
            "cortex_status": ("euxis-cortex", "Cortex Status"),
            "sync_docs": ("euxis-sync-docs", "Documentation Sync"),
        }

        if command == "cortex_status":
            self.action_open_cortex()
            return

        if command in tool_map:
            tool_name, label = tool_map[command]
            self.push_screen(ToolRunnerScreen(tool_name, label))

        elif command == "toggle_theme":
            self.action_toggle_theme()

        elif command == "settings":
            self.action_open_settings()

        elif command == "fleet_monitor":
            self.action_fleet_monitor_screen()

        elif command == "view_logs":
            self.action_open_logs()

        elif command == "help":
            self.action_help()

        elif command == "about":
            self.action_about()

        elif command == "playbooks":
            self.action_open_playbooks()

        elif command == "metrics":
            self.action_open_metrics()

        elif command == "cortex":
            self.action_open_cortex()

        elif command == "squad_detail":
            self.action_open_squad_detail()

        elif command == "refresh":
            self.action_refresh()
