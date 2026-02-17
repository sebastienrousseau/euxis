# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Agent registry and squad configuration loader."""

from __future__ import annotations

import json
import sqlite3
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, ClassVar

from tui.core import EUXIS_HOME


@dataclass(frozen=True)
class Agent:
    """A single Euxis agent."""

    id: str
    tier: str
    version: str
    tags: list[str]
    activation: str
    capability_tags: list[str] = field(default_factory=list)

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
        """Return human-readable tier label."""
        return self.TIER_LABELS.get(self.tier, self.tier.upper())

    @property
    def activation_label(self) -> str:
        """Return human-readable activation label."""
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
    version: str = "0.0.8"

    @classmethod
    def load(cls, euxis_home: Path | None = None) -> FleetRegistry:
        """Load registry (SQLite-first, JSON fallback) and squads from JSON."""
        home = euxis_home or EUXIS_HOME
        registry = cls()

        # Try SQLite first (new structure: euxis-core/agents/)
        db_candidates = [
            home / "euxis-core" / "agents" / "registry.db",
            home / "agents" / "registry.db",
            home / "registry.db",
        ]
        loaded = False
        for db_path in db_candidates:
            if db_path.exists():
                try:
                    loaded = registry._load_from_sqlite(db_path)
                except (sqlite3.Error, OSError):
                    loaded = False
                if loaded:
                    break

        # Fall back to JSON
        if not loaded:
            registry_candidates = [
                home / "euxis-core" / "agents" / "registry.json",
                home / "agents" / "registry.json",
                home / "registry.json",
            ]
            for registry_path in registry_candidates:
                if registry_path.exists():
                    registry._parse_agents(json.loads(registry_path.read_text()))
                    break

        squads_candidates = [
            home / "euxis-core" / "agents" / "squads.json",
            home / "agents" / "squads.json",
            home / "squads.json",
        ]
        for squads_path in squads_candidates:
            if squads_path.exists():
                registry._parse_squads(json.loads(squads_path.read_text()))
                break

        registry._rebuild_index()
        return registry

    def _load_from_sqlite(self, db_path: Path) -> bool:
        """Load agents from SQLite registry database."""
        conn = sqlite3.connect(str(db_path))
        conn.row_factory = sqlite3.Row

        # Get protocol version
        row = conn.execute(
            "SELECT value FROM registry_metadata WHERE key='protocol_version'"
        ).fetchone()
        if row:
            self.version = row[0]

        # Load agents via the agents_complete view
        rows = conn.execute(
            "SELECT id, path, tier, version, activation, tags, capability_tags "
            "FROM agents_complete ORDER BY id"
        ).fetchall()

        for row in rows:
            tags = row["tags"].split(",") if row["tags"] else []
            caps = row["capability_tags"].split(",") if row["capability_tags"] else []
            self.agents.append(
                Agent(
                    id=row["id"],
                    tier=row["tier"] or "fleet",
                    version=row["version"] or self.version,
                    tags=tags,
                    activation=row["activation"] or "default",
                    capability_tags=caps,
                )
            )

        conn.close()
        return len(self.agents) > 0

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
        registry._rebuild_index()
        return registry

    def _parse_agents(self, data: Any) -> None:
        """Parse agent entries from a dictionary."""
        if not isinstance(data, dict):
            msg = f"Expected dict for agent data, got {type(data).__name__}"
            raise TypeError(msg)
        self.version = data.get("protocol_version", self.version)
        for entry in data.get("agents", []):
            self.agents.append(
                Agent(
                    id=entry["id"],
                    tier=entry.get("tier", "fleet"),
                    version=entry.get("version", self.version),
                    tags=list(entry.get("tags", [])),
                    activation=entry.get("activation", "default"),
                    capability_tags=list(entry.get("capability_tags", [])),
                )
            )
        self._rebuild_index()

    def _parse_squads(self, data: Any) -> None:
        """Parse squad and combo entries from a dictionary."""
        if not isinstance(data, dict):
            msg = f"Expected dict for squad data, got {type(data).__name__}"
            raise TypeError(msg)
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
        self._rebuild_index()

    @property
    def core_agents(self) -> list[Agent]:
        """Return agents tagged as core."""
        return [agent for agent in self.agents if agent.tier == "core"]

    @property
    def default_agents(self) -> list[Agent]:
        """Return default-activated fleet agents."""
        return [
            agent
            for agent in self.agents
            if agent.tier != "core" and agent.activation == "default"
        ]

    @property
    def specialist_agents(self) -> list[Agent]:
        """Return specialist-activated fleet agents."""
        return [
            agent
            for agent in self.agents
            if agent.tier != "core" and agent.activation == "specialist"
        ]

    def list_agents(self) -> list[str]:
        """Return a list of agent IDs."""
        return [agent.id for agent in self.agents]

    def get_agent(self, agent_id: str) -> Agent | None:
        """Return the agent matching the given ID, if present."""
        if not hasattr(self, "_agents_by_id"):
            self._rebuild_index()
        return self._agents_by_id.get(agent_id)

    def _rebuild_index(self) -> None:
        """Rebuild the agent lookup index after agents change."""
        self._agents_by_id: dict[str, Agent] = {a.id: a for a in self.agents}

    def get_agent(self, agent_id: str) -> Agent | None:
        """Look up an agent by ID, rebuilding the index if needed."""
        if not hasattr(self, "_agents_by_id") or len(self._agents_by_id) != len(self.agents):
            self._rebuild_index()
        return self._agents_by_id.get(agent_id)

    @property
    def core_agents(self) -> list[Agent]:
        """Return agents with core tier designation."""
        return [a for a in self.agents if a.tier == "core"]

    @property
    def default_agents(self) -> list[Agent]:
        """Return non-core agents with default activation."""
        return [a for a in self.agents if a.activation == "default" and a.tier != "core"]

    @property
    def ondemand_agents(self) -> list[Agent]:
        """Return agents with on-demand activation."""
        return [a for a in self.agents if a.activation == "on-demand"]

    @property
    def specialist_agents(self) -> list[Agent]:
        """Return agents with specialist activation."""
        return [a for a in self.agents if a.activation == "specialist"]
