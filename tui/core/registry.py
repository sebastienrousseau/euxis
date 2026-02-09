# (c) 2026 Euxis Fleet. All rights reserved.
"""Agent registry and squad configuration loader."""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, ClassVar


EUXIS_HOME = Path.home() / ".euxis"


@dataclass(frozen=True)
class Agent:
    """A single Euxis agent."""

    id: str
    tier: str
    version: str
    tags: tuple[str, ...]
    activation: str
    capability_tags: tuple[str, ...] = ()

    TIER_LABELS: ClassVar[dict[str, str]] = {
        "core": "CORE",
        "fleet": "FLEET",
    }

    ACTIVATION_LABELS: ClassVar[dict[str, str]] = {
        "default": "Auto",
        "on-demand": "On-Demand",
        "specialist": "Specialist",
    }

    @property
    def tier_label(self) -> str:
        return self.TIER_LABELS.get(self.tier, self.tier.upper())

    @property
    def activation_label(self) -> str:
        return self.ACTIVATION_LABELS.get(self.activation, self.activation)


@dataclass(frozen=True)
class Squad:
    """An agent squad configuration."""

    id: str
    name: str
    purpose: str
    lead: str
    members: tuple[str, ...]


@dataclass(frozen=True)
class Combo:
    """A sequential agent chain."""

    id: str
    name: str
    description: str
    chain: tuple[str, ...]


@dataclass
class FleetRegistry:
    """Complete fleet configuration loaded from JSON files."""

    agents: list[Agent] = field(default_factory=list)
    squads: list[Squad] = field(default_factory=list)
    combos: list[Combo] = field(default_factory=list)
    version: str = "0.0.7"

    @classmethod
    def load(cls, euxis_home: Path | None = None) -> FleetRegistry:
        """Load registry from JSON files under euxis_home."""
        home = euxis_home or EUXIS_HOME
        registry = cls()

        registry_path = home / "registry.json"
        if registry_path.exists():
            registry._parse_agents(json.loads(registry_path.read_text()))

        squads_path = home / "squads.json"
        if squads_path.exists():
            registry._parse_squads(json.loads(squads_path.read_text()))

        return registry

    @classmethod
    def from_dicts(
        cls,
        agents_data: dict[str, Any] | None = None,
        squads_data: dict[str, Any] | None = None,
    ) -> FleetRegistry:
        """Create a registry from pre-loaded dictionaries (no file I/O)."""
        registry = cls()
        if agents_data:
            registry._parse_agents(agents_data)
        if squads_data:
            registry._parse_squads(squads_data)
        return registry

    def _parse_agents(self, data: dict[str, Any]) -> None:
        """Parse agent entries from a dictionary."""
        self.version = data.get("protocol_version", self.version)
        for entry in data.get("agents", []):
            self.agents.append(
                Agent(
                    id=entry["id"],
                    tier=entry.get("tier", "fleet"),
                    version=entry.get("version", self.version),
                    tags=tuple(entry.get("tags", [])),
                    activation=entry.get("activation", "default"),
                    capability_tags=tuple(entry.get("capability_tags", [])),
                )
            )

    def _parse_squads(self, data: dict[str, Any]) -> None:
        """Parse squad and combo entries from a dictionary."""
        for entry in data.get("squads", []):
            self.squads.append(
                Squad(
                    id=entry["id"],
                    name=entry["name"],
                    purpose=entry.get("purpose", ""),
                    lead=entry.get("lead", ""),
                    members=tuple(entry.get("members", [])),
                )
            )
        for entry in data.get("combos", []):
            self.combos.append(
                Combo(
                    id=entry["id"],
                    name=entry["name"],
                    description=entry.get("description", ""),
                    chain=tuple(entry.get("chain", [])),
                )
            )

    def get_agent(self, agent_id: str) -> Agent | None:
        for agent in self.agents:
            if agent.id == agent_id:
                return agent
        return None

    @property
    def core_agents(self) -> list[Agent]:
        return [a for a in self.agents if a.tier == "core"]

    @property
    def default_agents(self) -> list[Agent]:
        return [a for a in self.agents if a.activation == "default" and a.tier != "core"]

    @property
    def ondemand_agents(self) -> list[Agent]:
        return [a for a in self.agents if a.activation == "on-demand"]

    @property
    def specialist_agents(self) -> list[Agent]:
        return [a for a in self.agents if a.activation == "specialist"]
