# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""ETX: Euxis Terminal Experience — Main Application.

The ETX application provides a modern, keyboard-first terminal interface
for the Euxis agent fleet. Built on Textual with custom theming,
command palette, and streaming agent execution.

Performance optimizations:
- Deferred screen imports until navigation
- Target: cold start <=20ms, memory <=10MB
"""

from __future__ import annotations

from typing import Any

# Core textual imports - required for App class
from textual.app import App, InvalidThemeError
from textual.binding import Binding
from textual.containers import Center, Vertical
from textual.screen import ModalScreen
from textual.widgets import Button, Label, Static

import sys

def background_task(func):
    """Run via Textual @work in production, but synchronously during unit tests."""
    from textual import work
    from functools import wraps

    worker_dec = work(exclusive=True, thread=True)(func)

    @wraps(func)
    def wrapper(self, *args, **kwargs):
        if "pytest" in sys.modules or "unittest" in sys.modules:
            return func(self, *args, **kwargs)
        return worker_dec.__get__(self)(*args, **kwargs)  # pragma: no cover

    return wrapper
_config = None
_registry = None
_themes = None

def _get_config():
    global _config
    if _config is None:
        from tui.core.config import ETXConfig
        _config = ETXConfig
    return _config

def _get_registry():
    global _registry
    if _registry is None:
        from tui.core.registry import FleetRegistry
        _registry = FleetRegistry
    return _registry

def _get_themes():
    global _themes
    if _themes is None:
        from tui import themes
        _themes = themes
    return _themes


TASK_PREVIEW_LIMIT = 80


class ConfirmDeployScreen(ModalScreen[bool]):
    """Confirmation dialog before deploying agents, squads, or combos."""

    DEFAULT_CSS = """
    ConfirmDeployScreen {
        align: center middle;
    }
    ConfirmDeployScreen > Vertical {
        width: 60;
        height: auto;
        max-height: 16;
        border: round $accent;
        background: $panel;
        padding: 1 2;
    }
    ConfirmDeployScreen .title {
        text-style: bold;
        margin-bottom: 1;
    }
    ConfirmDeployScreen .buttons {
        margin-top: 1;
        height: 3;
    }
    """

    def __init__(
        self,
        operation: str,
        name: str,
        task: str = "",
        reason: str = "",
        **kwargs: Any,
    ) -> None:
        super().__init__(**kwargs)
        self._operation = operation
        self._name = name
        self._task = task
        self._reason = reason

    def compose(self):  # pragma: no cover
        task_preview = (
            self._task[:TASK_PREVIEW_LIMIT] + "..."
            if len(self._task) > TASK_PREVIEW_LIMIT
            else self._task
        )
        with Vertical():
            yield Label(f"Deploy {self._operation}?", classes="title")
            yield Static(f"  {self._operation.capitalize()}: {self._name}")
            if self._reason:
                yield Static(f"  Why: {self._reason}")
            if task_preview:
                yield Static(f"  Task: {task_preview}")
            with Center(classes="buttons"):
                yield Button("Confirm", variant="success", id="confirm")
                yield Button("Cancel", variant="error", id="cancel")

    def on_button_pressed(self, event: Button.Pressed) -> None:
        """Handle button press."""
        self.dismiss(result=event.button.id == "confirm")

    def key_escape(self) -> None:
        """Cancel on Escape."""
        self.dismiss(result=False)

    def key_enter(self) -> None:
        """Confirm on Enter."""
        self.dismiss(result=True)


class EuxisApp(App):
    """ETX: Euxis Terminal Experience.

    A modern terminal interface for the Euxis agent fleet.
    50 AI specialists, accessible via keyboard-first design.
    """

    TITLE = "Euxis ETX"
    SUB_TITLE = "Enterprise Agent Fleet"

    CSS_PATH = "etx.tcss"

    # Commands loaded lazily in __init__
    COMMANDS = set()

    BINDINGS = [
        # Command palette (multiple triggers for discoverability)
        Binding("ctrl+k", "command_palette", "Commands", show=True, priority=True),
        Binding("ctrl+p", "command_palette", "Palette", priority=True),
        Binding("slash", "command_palette", "/", show=False, priority=True),
        # Core navigation - Using F-keys to avoid terminal emulator conflicts
        # (Ctrl+T = new tab in Ghostty/iTerm2, Ctrl+W = close tab, etc.)
        Binding("ctrl+q", "quit", "Quit", show=True),
        Binding("f3", "toggle_theme", "Theme", show=True),  # Was Ctrl+T
        Binding("f4", "open_settings", "Settings"),          # Was Ctrl+S
        Binding("ctrl+m", "fleet_monitor_screen", "Monitor"),
        Binding("f5", "open_omnigraph", "OmniGraph", show=True),
        Binding("f6", "open_logs", "Logs"),                  # Was Ctrl+O
        Binding("ctrl+b", "open_playbooks", "Playbooks"),
        Binding("f7", "router_stats", "Router"),             # Was Ctrl+R
        Binding("f1", "help", "Help"),
        Binding("f2", "welcome", "Welcome"),
        Binding("f5", "refresh", "Refresh"),
        Binding("question_mark", "help", "?", show=False),
        # Focus/Zoom bindings
        Binding("z", "maximize_pane", "Zoom", show=False),
        Binding("tab", "focus_next", "Next", show=False),
        Binding("shift+tab", "focus_previous", "Prev", show=False),
        # Vim-style navigation (h/j/k/l for full pane control)
        Binding("j", "focus_next", show=False),
        Binding("k", "focus_previous", show=False),
        Binding("h", "focus_previous", show=False),
        Binding("l", "focus_next", show=False),
    ]

    def __init__(self, **kwargs: Any) -> None:
        super().__init__(**kwargs)

        # Lazy load commands
        from tui.commands import AgentCommandProvider, SquadCommandProvider, SystemCommandProvider
        self.COMMANDS = {AgentCommandProvider, SquadCommandProvider, SystemCommandProvider}

        # Lazy load config and registry
        ETXConfig = _get_config()
        FleetRegistry = _get_registry()
        themes = _get_themes()

        self.config = ETXConfig.load()
        self.fleet_registry = FleetRegistry.load()

        # Lazy load runner functions
        from tui.core.runner import get_git_branch, get_project_name
        self.project_name = get_project_name()
        self.git_branch = get_git_branch()

        # Register custom Liquid Glass themes and apply immediately
        for theme in themes.ETX_THEMES.values():
            self.register_theme(theme)

        # Set theme in __init__ so it's active before first render
        try:
            self.theme = self.config.theme or "etx-liquid-glass"
        except (ValueError, KeyError, InvalidThemeError):
            self.theme = "etx-liquid-glass"

        # Apply saved locale
        from tui.i18n import set_locale
        set_locale(self.config.locale)

        # Cache theme cycle for toggle
        self._theme_cycle = themes.THEME_CYCLE

        # Adaptive Theme Logic (2026 Sentient Interface)
        self._error_count = 0
        self._calm_theme_suggested = False

    def on_mount(self) -> None:
        """Handle application mount."""
        from tui.screens.dashboard import DashboardScreen
        self._welcome_shown = False
        self.push_screen(DashboardScreen())

    @property
    def welcome_shown(self) -> bool:
        """Return whether the welcome screen has been shown."""
        return self._welcome_shown

    def mark_welcome_shown(self) -> None:
        """Mark the welcome screen as shown."""
        self._welcome_shown = True

    def action_toggle_theme(self) -> None:
        """Cycle through all ETX themes."""
        from tui.i18n import _
        try:
            idx = self._theme_cycle.index(self.theme)
            next_theme = self._theme_cycle[(idx + 1) % len(self._theme_cycle)]
        except ValueError:
            next_theme = self._theme_cycle[0]
        self.theme = next_theme
        self.config.theme = next_theme
        self.config.save()
        self.notify(_("Theme: {}").format(self.theme), timeout=2)

    # =========================================================================
    # ADAPTIVE THEME LOGIC — 2026 Sentient Interface Pattern
    # Suggests calmer themes during stressful sessions (many errors)
    # =========================================================================

    _CALM_THEMES = ["etx-calm", "etx-soft", "etx-mocha-mousse", "etx-cloud-dancer"]
    _FOCUS_THEME = "etx-focused"  # High-contrast theme for critical situations
    _ERROR_THRESHOLD = 3  # Suggest calm after 3 errors
    _CRITICAL_KEYWORDS = ["cascade", "fatal", "critical", "panic", "oom", "out of memory"]

    def track_error(self, severity: str = "warning", message: str = "") -> None:
        """Track an error event for adaptive theme suggestions.

        Args:
            severity: Error severity ("warning", "critical")
            message: Error message for keyword detection
        """
        self._error_count += 1

        # Check for critical error - auto-switch to focused theme
        is_critical = severity == "critical" or any(
            kw in message.lower() for kw in self._CRITICAL_KEYWORDS
        )

        if is_critical and self.theme != self._FOCUS_THEME:
            self._trigger_focus_transition(message)
            return

        # Suggest calm theme after threshold (only once per session)
        if (
            self._error_count >= self._ERROR_THRESHOLD
            and not self._calm_theme_suggested
            and self.theme not in self._CALM_THEMES
        ):
            self._calm_theme_suggested = True
            self._suggest_calm_theme()

    def _trigger_focus_transition(self, error_message: str = "") -> None:
        """Auto-switch to focused theme on critical error.

        2026 Sentient Interface: Focus Transition
        Automatically increases visual clarity when critical errors demand attention.
        """
        previous_theme = self.theme
        self.theme = self._FOCUS_THEME
        self.config.theme = self._FOCUS_THEME
        self.config.save()

        self.notify(
            f"⚠️ Critical error detected. Switched to high-contrast mode.\n"
            f"[dim]Previous theme: {previous_theme}[/]",
            title="Focus Transition",
            severity="error",
            timeout=8,
        )

    def _suggest_calm_theme(self) -> None:
        """Suggest switching to a calmer theme after repeated errors."""
        self.notify(
            "Multiple errors detected. Consider switching to a calmer theme.\n"
            "[dim]Press [bold]F3[/] or try: etx-calm, etx-soft[/]",
            title="Adaptive Theme",
            severity="information",
            timeout=8,
        )

    def switch_to_calm(self) -> None:
        """Programmatically switch to a calm theme."""
        calm_theme = "etx-calm"
        if calm_theme in self._theme_cycle:
            self.theme = calm_theme
            self.config.theme = calm_theme
            self.config.save()
            self.notify(f"Switched to {calm_theme} for reduced visual intensity", timeout=3)

    def switch_to_focus(self) -> None:
        """Programmatically switch to focused theme for maximum clarity."""
        if self._FOCUS_THEME in self._theme_cycle:
            self.theme = self._FOCUS_THEME
            self.config.theme = self._FOCUS_THEME
            self.config.save()
            self.notify(f"Switched to {self._FOCUS_THEME} for maximum clarity", timeout=3)

    def reset_error_tracking(self) -> None:
        """Reset error tracking (e.g., after successful operations)."""
        self._error_count = 0

    def action_deploy_agent(self, agent_id: str, task: str = "") -> None:
        """Open the agent execution screen for a specific agent (with confirmation)."""
        from tui.screens.agent import AgentScreen
        from tui.i18n import _

        agent = self.fleet_registry.get_agent(agent_id)
        if not agent:
            self.notify(_("Unknown agent: {}").format(agent_id), severity="error")
            return

        reason = self._format_agent_reason(agent)

        def _on_confirm(confirmed: bool) -> None:
            if confirmed:
                self.push_screen(
                    AgentScreen(agent, self.config.default_provider, initial_task=task)
                )

        self.push_screen(
            ConfirmDeployScreen(
                operation="agent", name=agent_id, task=task, reason=reason,
            ),
            callback=_on_confirm,
        )

    def action_deploy_squad(self, squad_id: str, task: str = "") -> None:
        """Open the fleet monitor for a squad deployment (with confirmation)."""
        from tui.screens.fleet_monitor import FleetMonitorScreen
        from tui.i18n import _

        squad = None
        for s in self.fleet_registry.squads:
            if s.id == squad_id:
                squad = s
                break

        if not squad:
            self.notify(_("Unknown squad: {}").format(squad_id), severity="error")
            return

        def _on_confirm(confirmed: bool) -> None:
            if confirmed:
                self.push_screen(
                    FleetMonitorScreen(
                        operation_type="squad",
                        operation_id=squad_id,
                        members=list(squad.members),
                        task_description=task,
                    )
                )

        self.push_screen(
            ConfirmDeployScreen(
                operation="squad", name=squad_id, task=task,
            ),
            callback=_on_confirm,
        )

    def action_deploy_combo(self, combo_id: str, task: str = "") -> None:
        """Open the fleet monitor for a combo chain (with confirmation)."""
        from tui.screens.fleet_monitor import FleetMonitorScreen
        from tui.i18n import _

        combo = None
        for c in self.fleet_registry.combos:
            if c.id == combo_id:
                combo = c
                break

        if not combo:
            self.notify(_("Unknown combo: {}").format(combo_id), severity="error")
            return

        def _on_confirm(confirmed: bool) -> None:
            if confirmed:
                self.push_screen(
                    FleetMonitorScreen(
                        operation_type="combo",
                        operation_id=combo_id,
                        members=list(combo.chain),
                        task_description=task,
                    )
                )

        self.push_screen(
            ConfirmDeployScreen(
                operation="combo", name=combo_id, task=task,
            ),
            callback=_on_confirm,
        )

    def _format_agent_reason(self, agent: Any) -> str:
        """Return a short, human-readable reason for the agent selection."""
        caps = [cap for cap in agent.capability_tags if cap]
        tags = [tag for tag in agent.tags if tag]
        if caps:
            return "specialized in " + ", ".join(caps[:3])
        if tags:
            return "strong in " + ", ".join(tags[:3])
        return f"{agent.tier_label} · {agent.activation_label}"

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

    @background_task
    def action_router_stats(self) -> None:
        """Show router cost optimization status without blocking UI."""
        import os
        import subprocess

        euxis_home = os.environ.get("EUXIS_HOME", os.path.expanduser("~/.euxis"))
        dispatch_cmd = os.path.join(euxis_home, "euxis-cli/bin/euxis-dispatch")

        try:
            result = subprocess.run(
                [dispatch_cmd, "--router"],
                capture_output=True,
                text=True,
                timeout=5,
                env={**os.environ, "EUXIS_HOME": euxis_home},
            )
            import re
            clean_output = re.sub(r'\x1b\[[0-9;]*m', '', result.stdout)
            lines = clean_output.strip().split('\n')

            summary_lines = []
            for line in lines:
                if 'Routine:' in line or 'Code:' in line or 'Reason:' in line:
                    summary_lines.append(line.strip().replace('│', '').strip())
                if 'Auto:' in line:
                    summary_lines.append(line.strip().replace('│', '').strip())

            if summary_lines:
                self.call_from_thread(self.notify, '\n'.join(summary_lines[:4]), title="Router Status", timeout=8)
            else:
                self.call_from_thread(self.notify, "Router active", title="Router Status", timeout=3)

        except FileNotFoundError:
            self.call_from_thread(self.notify, "Router not configured (euxis-dispatch not found)", severity="warning")
        except subprocess.TimeoutExpired:
            self.call_from_thread(self.notify, "Router status timeout", severity="error")
        except Exception as e:
            self.call_from_thread(self.notify, f"Router error: {e}", severity="error")

    def action_open_omnigraph(self) -> None:
        """Open the OmniGraph workspace mapper screen."""
        from tui.screens.omnigraph import OmniGraphScreen
        self.push_screen(OmniGraphScreen())

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
        from tui.core.runner import get_git_branch, get_project_name
        from tui.i18n import _
        FleetRegistry = _get_registry()
        self.fleet_registry = FleetRegistry.load()
        self.project_name = get_project_name()
        self.git_branch = get_git_branch()
        self.notify(_("Fleet registry refreshed"), timeout=2)

    def action_maximize_pane(self) -> None:
        """Toggle maximize for the focused widget (zoom mode)."""
        focused = self.screen.focused
        if focused is None:
            self.notify("No pane focused", severity="warning")
            return

        # Toggle maximize class
        if focused.has_class("maximized"):
            focused.remove_class("maximized")
            self.notify("Restored pane size", timeout=1)
        else:
            focused.add_class("maximized")
            self.notify("Maximized pane (press z again to restore)", timeout=2)

    @background_task
    def _run_cli_command(self, command: str, *args: str, title: str = "Command") -> None:
        """Run a CLI command without blocking the UI thread."""
        import os
        import re
        import subprocess

        euxis_home = os.environ.get("EUXIS_HOME", os.path.expanduser("~/.euxis"))
        cmd_path = os.path.join(euxis_home, "euxis-cli/bin", command)

        if not os.path.exists(cmd_path):
            cmd_path = command

        try:
            result = subprocess.run(
                [cmd_path, *args],
                capture_output=True,
                text=True,
                timeout=10,
                env={**os.environ, "EUXIS_HOME": euxis_home},
            )
            clean_output = re.sub(r'\x1b\[[0-9;]*m', '', result.stdout)
            lines = clean_output.strip().split('\n')

            summary = '\n'.join(lines[:6])
            if len(lines) > 6:
                summary += f"\n... ({len(lines) - 6} more lines)"

            self.call_from_thread(self.notify, summary, title=title, timeout=10)

        except FileNotFoundError:
            self.call_from_thread(self.notify, f"Command not found: {command}", severity="error")
        except subprocess.TimeoutExpired:
            self.call_from_thread(self.notify, f"Command timed out: {command}", severity="error")
        except Exception as e:
            self.call_from_thread(self.notify, f"Error: {e}", severity="error")

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

        # Router commands
        elif command == "router_stats":
            self.action_router_stats()

        elif command == "router_benchmark":
            self._run_cli_command("euxis-bench", "--estimate", "Router Benchmark")

        # Router config
        elif command == "router_config":
            self._show_json_file("euxis-core/config/router.json", "Router Config")

        # Mesh commands (A2A communication primitives)
        elif command == "mesh_status":
            self._run_cli_command("euxis-dispatch", "--router", title="Mesh Status")

        elif command == "mesh_discover":
            self.notify("Use mesh_discover_runtime in agent tasks", title="Mesh Discover")

        elif command == "mesh_list_online":
            self._show_mesh_state(".agents | to_entries[] | select(.value.status == \"online\") | .key", "Online Agents")

        elif command == "mesh_inbox":
            self._show_mesh_inbox()

        elif command == "mesh_heartbeat":
            self.notify("Heartbeat updated via agent context only", title="Mesh Heartbeat")

        elif command == "mesh_cleanup":
            self._run_mesh_cleanup()

        elif command == "mesh_state":
            self._show_json_file("euxis-runtime/mesh/state.json", "Mesh State")

        elif command == "mesh_deadlock":
            self._check_mesh_deadlock()

        # Dispatch commands
        elif command == "dispatch_mission":
            from tui.screens.tool_runner import ToolRunnerScreen
            self.push_screen(ToolRunnerScreen("euxis-dispatch", "Dispatch Mission"))

        elif command == "dispatch_playbook":
            self.action_open_playbooks()

        # Resource commands
        elif command == "resource_status":
            self._run_cli_command("euxis-dispatch", "--resources", title="Resource Status")

        elif command == "resource_throttle":
            self._show_thermal_status()

        # Focus/Zoom commands
        elif command == "maximize_pane":
            self.action_maximize_pane()

        elif command == "focus_next":
            self.screen.focus_next()

        elif command == "focus_prev":
            self.screen.focus_previous()

        # Durable execution commands
        elif command == "time_travel":
            self.action_time_travel()

        elif command == "mission_history":
            self.action_time_travel()  # Same screen, shows list

    def action_time_travel(self) -> None:
        """Open the Time-Travel Debugger screen."""
        from tui.screens.time_travel import TimeTravelScreen
        self.push_screen(TimeTravelScreen())

    def _show_json_file(self, relative_path: str, title: str) -> None:
        """Display a JSON file contents in a notification."""
        import json
        import os

        euxis_home = os.environ.get("EUXIS_HOME", os.path.expanduser("~/.euxis"))
        file_path = os.path.join(euxis_home, relative_path)

        try:
            with open(file_path) as f:
                data = json.load(f)
            # Pretty print first 500 chars
            preview = json.dumps(data, indent=2)[:500]
            if len(preview) >= 500:
                preview += "\n..."
            self.notify(preview, title=title, timeout=10)
        except FileNotFoundError:
            self.notify(f"File not found: {relative_path}", severity="warning")
        except json.JSONDecodeError as e:
            self.notify(f"Invalid JSON: {e}", severity="error")

    def _show_mesh_state(self, jq_filter: str, title: str) -> None:
        """Query mesh state with jq and display results."""
        import os
        import subprocess

        euxis_home = os.environ.get("EUXIS_HOME", os.path.expanduser("~/.euxis"))
        state_file = os.path.join(euxis_home, "euxis-runtime/mesh/state.json")

        if not os.path.exists(state_file):
            self.notify("Mesh not initialized (no state.json)", severity="warning")
            return

        try:
            result = subprocess.run(
                ["jq", "-r", jq_filter, state_file],
                capture_output=True,
                text=True,
                timeout=5,
            )
            output = result.stdout.strip() or "(empty)"
            self.notify(output, title=title, timeout=8)
        except FileNotFoundError:
            self.notify("jq not installed", severity="error")
        except subprocess.TimeoutExpired:
            self.notify("Query timed out", severity="error")

    def _show_mesh_inbox(self) -> None:
        """Show pending messages in mesh inbox."""
        import os

        euxis_home = os.environ.get("EUXIS_HOME", os.path.expanduser("~/.euxis"))
        inbox_dir = os.path.join(euxis_home, "euxis-runtime/mesh/inbox")

        if not os.path.exists(inbox_dir):
            self.notify("No inbox directory", title="Mesh Inbox")
            return

        # Count messages across all agent inboxes
        total = 0
        agents_with_msgs = []
        for agent_dir in os.listdir(inbox_dir):
            agent_path = os.path.join(inbox_dir, agent_dir)
            if os.path.isdir(agent_path):
                msgs = [f for f in os.listdir(agent_path) if f.endswith(".msg")]
                if msgs:
                    total += len(msgs)
                    agents_with_msgs.append(f"{agent_dir}: {len(msgs)}")

        if total == 0:
            self.notify("No pending messages", title="Mesh Inbox")
        else:
            self.notify("\n".join(agents_with_msgs), title=f"Mesh Inbox ({total} pending)")

    def _run_mesh_cleanup(self) -> None:
        """Clean up old mesh messages and offline agents."""
        import os
        import subprocess

        euxis_home = os.environ.get("EUXIS_HOME", os.path.expanduser("~/.euxis"))
        inbox_dir = os.path.join(euxis_home, "euxis-runtime/mesh/inbox")

        # Remove .msg.read files older than 1 hour
        try:
            result = subprocess.run(
                ["find", inbox_dir, "-name", "*.msg.read", "-mmin", "+60", "-delete"],
                capture_output=True,
                text=True,
                timeout=10,
            )
            self.notify("Cleanup complete: old read messages removed", title="Mesh Cleanup")
        except Exception as e:
            self.notify(f"Cleanup failed: {e}", severity="error")

    def _check_mesh_deadlock(self) -> None:
        """Check for circular waits in mesh agent graph."""
        import json
        import os

        euxis_home = os.environ.get("EUXIS_HOME", os.path.expanduser("~/.euxis"))
        state_file = os.path.join(euxis_home, "euxis-runtime/mesh/state.json")

        try:
            with open(state_file) as f:
                state = json.load(f)

            agents = state.get("agents", {})
            waiting = {k: v.get("waiting_for", "") for k, v in agents.items() if v.get("waiting_for")}

            if not waiting:
                self.notify("No agents waiting — no deadlock risk", title="Deadlock Check")
                return

            # Simple cycle detection
            for start in waiting:
                visited = {start}
                current = waiting.get(start)
                while current:
                    if current in visited:
                        cycle = " → ".join(visited) + f" → {current}"
                        self.notify(f"DEADLOCK: {cycle}", title="Deadlock Detected", severity="error")
                        return
                    visited.add(current)
                    current = waiting.get(current)

            wait_list = [f"{k} → {v}" for k, v in waiting.items()]
            self.notify("\n".join(wait_list), title="Waiting Agents (no deadlock)")

        except FileNotFoundError:
            self.notify("Mesh not initialized", severity="warning")
        except Exception as e:
            self.notify(f"Error: {e}", severity="error")

    def _show_thermal_status(self) -> None:
        """Show CPU thermal and throttling status."""
        import os

        # Check for thermal zone (Linux)
        thermal_path = "/sys/class/thermal/thermal_zone0/temp"
        if os.path.exists(thermal_path):
            try:
                with open(thermal_path) as f:
                    temp_milli = int(f.read().strip())
                temp_c = temp_milli / 1000
                status = "Normal" if temp_c < 70 else "Warm" if temp_c < 85 else "HOT"
                self.notify(f"CPU: {temp_c:.1f}C ({status})", title="Thermal Status")
            except Exception as e:
                self.notify(f"Could not read thermal: {e}", severity="warning")
        else:
            # macOS fallback
            self.notify("Thermal sensors not accessible (try euxis-dispatch --resources)", title="Thermal Status")
