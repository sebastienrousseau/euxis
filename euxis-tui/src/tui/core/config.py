# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""ETX configuration management."""

from __future__ import annotations

import json
from dataclasses import dataclass, field

from tui.core import EUXIS_HOME

CONFIG_PATH = EUXIS_HOME / "euxis-runtime" / "config" / "etx-settings.json"


@dataclass
class ETXConfig:
    """User-configurable ETX settings."""

    theme: str = "etx-liquid-glass"
    default_provider: str = "claude"
    show_agent_tags: bool = True
    reduced_motion: bool = False
    accessible_mode: bool = False
    locale: str = "en"
    grid_columns: int = 4
    recent_agents: list[str] = field(default_factory=list)
    recent_commands: list[str] = field(default_factory=list)

    MAX_RECENT = 10

    @classmethod
    def load(cls) -> ETXConfig:
        """Load configuration from disk or return defaults."""
        if CONFIG_PATH.exists():
            data = json.loads(CONFIG_PATH.read_text())
            if not isinstance(data, dict):
                msg = f"Expected dict in config file, got {type(data).__name__}"
                raise TypeError(msg)
            return cls(**{k: v for k, v in data.items() if k in cls.__dataclass_fields__})
        return cls()

    def save(self) -> None:
        """Persist configuration to disk."""
        CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
        CONFIG_PATH.write_text(json.dumps(self.__dict__, indent=2) + "\n")

    def add_recent_agent(self, agent_id: str) -> None:
        """Track a recently used agent, moving it to front of list."""
        if not isinstance(agent_id, str) or not agent_id.strip():
            return
        if agent_id in self.recent_agents:
            self.recent_agents.remove(agent_id)
        self.recent_agents.insert(0, agent_id)
        self.recent_agents = self.recent_agents[: self.MAX_RECENT]

    def add_recent_command(self, command: str) -> None:
        """Track a recently used command, moving it to front of list."""
        if not isinstance(command, str) or not command.strip():
            return
        if command in self.recent_commands:
            self.recent_commands.remove(command)
        self.recent_commands.insert(0, command)
        self.recent_commands = self.recent_commands[: self.MAX_RECENT]
