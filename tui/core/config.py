# (c) 2026 Euxis Fleet. All rights reserved.
"""ETX configuration management."""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path

EUXIS_HOME = Path.home() / ".euxis"
CONFIG_PATH = EUXIS_HOME / "config" / "etx-settings.json"


@dataclass
class ETXConfig:
    """User-configurable ETX settings."""

    theme: str = "etx-dark"
    default_provider: str = "claude"
    show_agent_tags: bool = True
    reduced_motion: bool = False
    accessible_mode: bool = False
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
        if agent_id in self.recent_agents:
            self.recent_agents.remove(agent_id)
        self.recent_agents.insert(0, agent_id)
        self.recent_agents = self.recent_agents[: self.MAX_RECENT]

    def add_recent_command(self, command: str) -> None:
        """Track a recently used command, moving it to front of list."""
        if command in self.recent_commands:
            self.recent_commands.remove(command)
        self.recent_commands.insert(0, command)
        self.recent_commands = self.recent_commands[: self.MAX_RECENT]
