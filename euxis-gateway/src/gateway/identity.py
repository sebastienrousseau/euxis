# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Agent Identity and Governance management for Euxis Gateway."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict, List, Set, Optional
import json
import logging
from pathlib import Path

LOGGER = logging.getLogger("euxis.gateway.identity")

@dataclass
class AgentIdentity:
    """Unique digital identity for a Euxis Agent."""
    agent_id: str
    name: str
    trust_level: str = "restricted" # restricted, trusted, elevated
    permissions: Set[str] = field(default_factory=set)
    capabilities: Set[str] = field(default_factory=set)
    metadata: Dict[str, Any] = field(default_factory=dict)
    
    def to_dict(self) -> Dict[str, Any]:
        return {
            "agent_id": self.agent_id,
            "name": self.name,
            "trust_level": self.trust_level,
            "permissions": list(self.permissions),
            "capabilities": list(self.capabilities),
            "metadata": self.metadata
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> AgentIdentity:
        return cls(
            agent_id=data["agent_id"],
            name=data["name"],
            trust_level=data.get("trust_level", "restricted"),
            permissions=set(data.get("permissions", [])),
            capabilities=set(data.get("capabilities", [])),
            metadata=data.get("metadata", {})
        )

class IdentityManager:
    """Manages agent identities and their persistent state."""
    
    def __init__(self, data_dir: Path):
        """Initialize the IdentityManager with a data directory."""
        self.data_dir = data_dir
        self.identities: Dict[str, AgentIdentity] = {}
        self.identities_file = data_dir / "identities.json"
        self._load_identities()

    def _load_identities(self):
        """Load identities from the JSON file or initialize defaults."""
        if self.identities_file.exists():
            try:
                data = json.loads(self.identities_file.read_text())
                for agent_id, id_data in data.items():
                    self.identities[agent_id] = AgentIdentity.from_dict(id_data)
            except Exception as e:
                LOGGER.error(f"Failed to load identities: {e}")
        
        # Add default identities if none exist
        if not self.identities:
            self._add_defaults()

    def _add_defaults(self):
        """Register default core identities."""
        defaults = [
            AgentIdentity("architect", "Architect (Core)", "trusted", {"*"}, {"mcp", "mesh"}),
            AgentIdentity("orchestrator", "Orchestrator (Core)", "trusted", {"*"}, {"mcp", "mesh"}),
            AgentIdentity("researcher", "Researcher", "restricted", {"web_search", "web_fetch"}, {"mcp"}),
        ]
        for identity in defaults:
            self.identities[identity.agent_id] = identity
        self.save()

    def get_identity(self, agent_id: str) -> AgentIdentity:
        """Retrieve the identity for a given agent_id, defaulting to restricted."""
        return self.identities.get(agent_id, AgentIdentity(agent_id, f"Unknown Agent ({agent_id})"))

    def save(self):
        """Persist identities to the JSON file."""
        self.data_dir.mkdir(parents=True, exist_ok=True)
        data = {aid: identity.to_dict() for aid, identity in self.identities.items()}
        self.identities_file.write_text(json.dumps(data, indent=2))

    def check_permission(self, agent_id: str, permission: str) -> bool:
        """Check if an agent has a specific permission."""
        identity = self.get_identity(agent_id)
        if "*" in identity.permissions:
            return True
        return permission in identity.permissions
