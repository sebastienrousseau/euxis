# Euxis ETX API Reference

Consolidated reference for the Euxis Terminal Experience (ETX) public API.
Covers the core classes, screens, runner functions, and CLI entry points
that a developer needs to build on or extend the TUI.

---

## EuxisApp

**Module:** `tui.app`

Main application class. Subclasses `textual.App`.

| Attribute | Type | Description |
|-----------|------|-------------|
| `config` | `ETXConfig` | User settings loaded at startup |
| `fleet_registry` | `FleetRegistry` | Agent/squad/combo catalogue |
| `project_name` | `str` | Current working directory name |
| `git_branch` | `str \| None` | Active git branch |

### Global Key Bindings

| Key | Action | Description |
|-----|--------|-------------|
| `Ctrl+K` | `command_palette` | Open command palette |
| `Ctrl+Q` | `quit` | Exit the application |
| `Ctrl+T` | `toggle_theme` | Cycle dark / light / ANSI themes |
| `Ctrl+S` | `open_settings` | Settings screen |
| `Ctrl+M` | `fleet_monitor_screen` | Fleet monitor |
| `Ctrl+O` | `open_logs` | Log viewer |
| `Ctrl+P` | `open_playbooks` | Playbook browser |
| `F1` | `help` | Help screen |
| `F5` | `refresh` | Reload registry and project context |

### Key Methods

| Method | Description |
|--------|-------------|
| `action_deploy_agent(agent_id)` | Push `AgentScreen` for the given agent |
| `action_deploy_squad(squad_id)` | Push `FleetMonitorScreen` for a squad |
| `action_deploy_combo(combo_id)` | Push `FleetMonitorScreen` for a combo chain |
| `action_refresh()` | Reload registry, project name, and git branch |
| `run_system_command(command)` | Dispatch a palette command (see below) |

Palette commands accepted by `run_system_command`: `health`, `certify`,
`lint`, `cortex_status`, `sync_docs`, `toggle_theme`, `settings`,
`fleet_monitor`, `view_logs`, `help`, `about`, `playbooks`, `metrics`,
`cortex`, `squad_detail`, `refresh`.

---

## ETXConfig

**Module:** `tui.core.config`

User-configurable settings persisted to `~/.euxis/config/etx-settings.json`.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `theme` | `str` | `"etx-dark"` | Active theme name |
| `default_provider` | `str` | `"claude"` | Default AI provider |
| `show_agent_tags` | `bool` | `True` | Show tags on agent cards |
| `reduced_motion` | `bool` | `False` | Reduce animations |
| `accessible_mode` | `bool` | `False` | Enable accessibility features |
| `grid_columns` | `int` | `4` | Dashboard grid column count |
| `recent_agents` | `list[str]` | `[]` | Recently used agents (max 10) |
| `recent_commands` | `list[str]` | `[]` | Recently used commands (max 10) |

### Methods

```python
ETXConfig.load() -> ETXConfig          # Load from disk or return defaults
config.save() -> None                  # Persist to disk
config.add_recent_agent(agent_id)      # Track a recently used agent
config.add_recent_command(command)      # Track a recently used command
```

---

## FleetRegistry

**Module:** `tui.core.registry`

Loads agents, squads, and combos from `~/.euxis/agents/registry.json` and
`~/.euxis/agents/squads.json`.

| Attribute | Type | Description |
|-----------|------|-------------|
| `agents` | `list[Agent]` | All registered agents |
| `squads` | `list[Squad]` | All squad configurations |
| `combos` | `list[Combo]` | All combo chains |
| `version` | `str` | Protocol version (default `"v0.0.3"`) |

### Loading

```python
FleetRegistry.load(euxis_home=None)               # From JSON files on disk
FleetRegistry.from_dicts(agents_data, squads_data) # From pre-loaded dicts
```

### Lookups

```python
registry.get_agent(agent_id) -> Agent | None
registry.core_agents       -> list[Agent]   # tier == "core"
registry.default_agents    -> list[Agent]   # activation == "default", non-core
registry.ondemand_agents   -> list[Agent]   # activation == "on-demand"
registry.specialist_agents -> list[Agent]   # activation == "specialist"
```

### Data Classes

**Agent** (frozen dataclass)

| Field | Type |
|-------|------|
| `id` | `str` |
| `tier` | `str` (`"core"` or `"fleet"`) |
| `version` | `str` |
| `tags` | `tuple[str, ...]` |
| `activation` | `str` (`"default"`, `"on-demand"`, `"specialist"`) |
| `capability_tags` | `tuple[str, ...]` |

Properties: `tier_label`, `activation_label`.

**Squad** (frozen dataclass) -- `id`, `name`, `purpose`, `lead`, `members`.

**Combo** (frozen dataclass) -- `id`, `name`, `description`, `chain`.

---

## Runner Functions

**Module:** `tui.core.runner`

Async generators that execute agents, squads, and combos via subprocess
and yield output lines as they arrive.

```python
async def run_agent(agent_id, task, provider="claude", working_dir=None) -> AsyncIterator[str]
async def run_squad(squad_id, task, working_dir=None) -> AsyncIterator[str]
async def run_combo(combo_id, task, working_dir=None) -> AsyncIterator[str]
```

**AgentRun** dataclass tracks execution state:
`agent_id`, `task`, `provider`, `started_at`, `output_lines`, `return_code`.
Properties: `is_running`, `elapsed`, `elapsed_display`.

Supported providers: `claude`, `gemini`, `ollama`, `codex`, `qwen`,
`crush`, `kiro-cli`, `goose`.

Utility functions:

```python
get_project_name(working_dir=None) -> str
get_git_branch(working_dir=None) -> str | None
list_agent_outputs(agent_id, project=None) -> list[Path]
```

---

## Screens

ETX ships 11 screens, each a `textual.Screen` subclass.

| Screen | Module | Purpose |
|--------|--------|---------|
| `DashboardScreen` | `tui.screens.dashboard` | Fleet grid with agent cards |
| `AgentScreen` | `tui.screens.agent` | Streaming agent execution |
| `FleetMonitorScreen` | `tui.screens.fleet_monitor` | Squad/combo progress tracking |
| `SettingsScreen` | `tui.screens.settings` | Configuration editor |
| `PlaybookScreen` | `tui.screens.playbooks` | Playbook browser |
| `CortexScreen` | `tui.screens.cortex` | Cortex memory browser |
| `MetricsScreen` | `tui.screens.metrics` | Performance sparklines |
| `SquadDetailScreen` | `tui.screens.squad_detail` | Squad and combo detail view |
| `LogViewerScreen` | `tui.screens.logs` | Log viewer |
| `HelpScreen` | `tui.screens.help` | Keyboard shortcut reference |
| `AboutScreen` | `tui.screens.about` | Version and credits |

### Screen-Level Bindings

Dashboard adds `/` (focus search) and `Escape` (back).
Agent screen adds `Ctrl+L` (clear output) and `Escape` (back).
All screens inherit the global bindings from `EuxisApp`.

---

## CLI Usage

```bash
euxis-tui                                  # Launch the TUI
euxis <agent> "<task>" [provider]          # Single agent
euxis-squad deploy <squad> "<task>"        # Squad deployment
euxis-combo run <combo> "<task>"           # Combo chain
euxis-health | euxis-certify | euxis-lint  # System utilities
```

Run `euxis --help` for the full list of options.
