# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Command Palette Dispatcher for ETX (Euxis Terminal Experience)."""

class CommandDispatcher:
    """Handles mapping and execution of system commands for the Euxis App."""

    def __init__(self, app):
        self.app = app

    def dispatch(self, command: str) -> None:
        """Execute a system command from the command palette."""
        from tui.screens.tool_runner import ToolRunnerScreen

        # 1. Tool Runners (Shared Screen Logic)
        tool_map = {
            "health": ("euxis-health", "Fleet Health Check"),
            "certify": ("euxis-certify", "Certification Pipeline"),
            "lint": ("euxis-lint", "Fleet Lint"),
            "sync_docs": ("euxis-sync-docs", "Documentation Sync"),
            "dispatch_mission": ("euxis-dispatch", "Dispatch Mission"),
        }
        if command in tool_map:
            tool_name, label = tool_map[command]
            self.app.push_screen(ToolRunnerScreen(tool_name, label))
            return

        # 2. Action Mappings (Direct method calls)
        action_map = {
            "toggle_theme": self.app.action_toggle_theme,
            "settings": self.app.action_open_settings,
            "fleet_monitor": self.app.action_fleet_monitor_screen,
            "view_logs": self.app.action_open_logs,
            "help": self.app.action_help,
            "about": self.app.action_about,
            "playbooks": self.app.action_open_playbooks,
            "metrics": self.app.action_open_metrics,
            "cortex": self.app.action_open_cortex,
            "cortex_status": self.app.action_open_cortex,
            "squad_detail": self.app.action_open_squad_detail,
            "refresh": self.app.action_refresh,
            "router_stats": self.app.action_router_stats,
            "dispatch_playbook": self.app.action_open_playbooks,
            "maximize_pane": self.app.action_maximize_pane,
            "focus_next": lambda: self.app.screen.focus_next(),
            "focus_prev": lambda: self.app.screen.focus_previous(),
            "time_travel": self.app.action_time_travel,
            "mission_history": self.app.action_time_travel,
            "omnigraph": self.app.action_open_omnigraph,
        }
        if command in action_map:
            action_map[command]()
            return

        # 3. CLI One-liners (Subprocess Logic)
        cli_commands = {
            "router_benchmark": ("euxis-bench", "--estimate", "Router Benchmark"),
            "mesh_status": ("euxis-dispatch", "--router", "Mesh Status"),
            "resource_status": ("euxis-dispatch", "--resources", "Resource Status"),
        }
        if command in cli_commands:
            bin_name, args, title = cli_commands[command]
            self.app._run_cli_command(bin_name, args, title=title)
            return

        # 4. State Previews (JSON/Internal)
        json_previews = {
            "router_config": ("euxis-core/config/router.json", "Router Config"),
            "mesh_state": ("euxis-runtime/mesh/state.json", "Mesh State"),
        }
        if command in json_previews:
            path, title = json_previews[command]
            self.app._show_json_file(path, title)
            return

        # 5. Specialized Primitives
        if command == "mesh_list_online":
            self.app._show_mesh_state(".agents | to_entries[] | select(.value.status == \"online\") | .key", "Online Agents")
        elif command == "mesh_discover":
            self.app.notify("Use mesh_discover_runtime in agent tasks", title="Mesh Discover")
        elif command == "mesh_inbox":
            self.app._show_mesh_inbox()
        elif command == "mesh_heartbeat":
            self.app.notify("Heartbeat updated via agent context only", title="Mesh Heartbeat")
        elif command == "mesh_cleanup":
            self.app._run_mesh_cleanup()
        elif command == "mesh_deadlock":
            self.app._check_mesh_deadlock()
        elif command == "resource_throttle":
            self.app._show_thermal_status()
