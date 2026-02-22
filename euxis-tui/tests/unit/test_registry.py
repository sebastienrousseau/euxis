# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Unit tests for FleetRegistry load/parse methods.

Tests cover: load (from JSON files), from_dicts, _parse_agents, _parse_squads.
"""

from __future__ import annotations

import json
import shutil
import tempfile
import unittest
from pathlib import Path

import pytest

from tui.core.registry import FleetRegistry


class TestFleetRegistryLoad(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="euxis_registry_")
        self.home = Path(self.tmpdir)

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def test_load_empty_directory(self):
        reg = FleetRegistry.load(euxis_home=self.home)
        assert len(reg.agents) == 0
        assert len(reg.squads) == 0
        assert len(reg.combos) == 0

    def test_load_registry_json(self):
        data = {
            "protocol_version": "0.1.0",
            "agents": [
                {"id": "architect", "tier": "core", "tags": ["design"], "activation": "default"},
                {"id": "debugger", "tier": "fleet", "tags": ["fix"], "activation": "on-demand"},
            ],
        }
        (self.home / "agents").mkdir(parents=True, exist_ok=True)
        (self.home / "agents/registry.json").write_text(json.dumps(data))

        reg = FleetRegistry.load(euxis_home=self.home)
        assert len(reg.agents) == 2
        assert reg.version == "0.1.0"
        assert reg.agents[0].id == "architect"
        assert reg.agents[0].tier == "core"
        assert reg.agents[1].activation == "on-demand"

    def test_load_squads_json(self):
        squads_data = {
            "squads": [
                {
                    "id": "dev-squad",
                    "name": "Dev Team",
                    "purpose": "Build stuff",
                    "lead": "architect",
                    "members": ["architect", "debugger"],
                },
            ],
            "combos": [
                {
                    "id": "build-combo",
                    "name": "Build Pipeline",
                    "description": "Build and test",
                    "chain": ["debugger", "tester"],
                },
            ],
        }
        (self.home / "agents").mkdir(parents=True, exist_ok=True)
        (self.home / "agents/squads.json").write_text(json.dumps(squads_data))

        reg = FleetRegistry.load(euxis_home=self.home)
        assert len(reg.squads) == 1
        assert reg.squads[0].id == "dev-squad"
        assert reg.squads[0].members == ("architect", "debugger")
        assert len(reg.combos) == 1
        assert reg.combos[0].chain == ("debugger", "tester")

    def test_load_both_files(self):
        (self.home / "agents").mkdir(parents=True, exist_ok=True)
        (self.home / "agents/registry.json").write_text(json.dumps({
            "agents": [{"id": "a1", "tier": "core"}],
        }))
        (self.home / "agents/squads.json").write_text(json.dumps({
            "squads": [{"id": "s1", "name": "S1", "members": ["a1"]}],
            "combos": [],
        }))

        reg = FleetRegistry.load(euxis_home=self.home)
        assert len(reg.agents) == 1
        assert len(reg.squads) == 1


class TestFleetRegistryFromDicts(unittest.TestCase):
    def test_from_dicts_agents_only(self):
        agents_data = {
            "agents": [
                {"id": "test-agent", "tier": "fleet", "tags": ["testing"]},
            ],
        }
        reg = FleetRegistry.from_dicts(agents_data=agents_data)
        assert len(reg.agents) == 1
        assert reg.agents[0].id == "test-agent"

    def test_from_dicts_squads_only(self):
        squads_data = {
            "squads": [
                {"id": "s1", "name": "Squad 1", "members": ["a1"]},
            ],
            "combos": [
                {"id": "c1", "name": "Combo 1", "chain": ["a1", "a2"]},
            ],
        }
        reg = FleetRegistry.from_dicts(squads_data=squads_data)
        assert len(reg.squads) == 1
        assert len(reg.combos) == 1

    def test_from_dicts_both(self):
        reg = FleetRegistry.from_dicts(
            agents_data={"agents": [{"id": "a1"}]},
            squads_data={"squads": [], "combos": []},
        )
        assert len(reg.agents) == 1

    def test_from_dicts_none(self):
        reg = FleetRegistry.from_dicts()
        assert len(reg.agents) == 0
        assert len(reg.squads) == 0


class TestFleetRegistryParseAgents(unittest.TestCase):
    def test_parse_agents_full(self):
        reg = FleetRegistry()
        reg._parse_agents({
            "protocol_version": "1.0",
            "agents": [
                {
                    "id": "myagent",
                    "tier": "core",
                    "version": "2.0",
                    "tags": ["a", "b"],
                    "activation": "specialist",
                    "capability_tags": ["cap1"],
                },
            ],
        })
        assert len(reg.agents) == 1
        a = reg.agents[0]
        assert a.id == "myagent"
        assert a.tier == "core"
        assert a.version == "2.0"
        assert a.tags == ["a", "b"]
        assert a.activation == "specialist"
        assert a.capability_tags == ["cap1"]

    def test_parse_agents_defaults(self):
        reg = FleetRegistry()
        reg._parse_agents({"agents": [{"id": "minimal"}]})
        a = reg.agents[0]
        assert a.tier == "fleet"
        assert a.activation == "default"
        assert a.tags == []

    def test_parse_agents_invalid_type(self):
        reg = FleetRegistry()
        with pytest.raises(TypeError):
            reg._parse_agents("not a dict")

    def test_parse_agents_empty(self):
        reg = FleetRegistry()
        reg._parse_agents({"agents": []})
        assert len(reg.agents) == 0


class TestFleetRegistryParseSquads(unittest.TestCase):
    def test_parse_squads_full(self):
        reg = FleetRegistry()
        reg._parse_squads({
            "squads": [
                {
                    "id": "s1",
                    "name": "Squad One",
                    "purpose": "Testing",
                    "lead": "architect",
                    "members": ["architect", "debugger"],
                },
            ],
            "combos": [
                {
                    "id": "c1",
                    "name": "Combo One",
                    "description": "A chain",
                    "chain": ["a1", "a2", "a3"],
                },
            ],
        })
        assert len(reg.squads) == 1
        assert reg.squads[0].purpose == "Testing"
        assert reg.squads[0].members == ("architect", "debugger")
        assert len(reg.combos) == 1
        assert reg.combos[0].chain == ("a1", "a2", "a3")

    def test_parse_squads_defaults(self):
        reg = FleetRegistry()
        reg._parse_squads({
            "squads": [{"id": "s1", "name": "S1"}],
            "combos": [{"id": "c1", "name": "C1"}],
        })
        assert reg.squads[0].purpose == ""
        assert reg.squads[0].lead == ""
        assert reg.squads[0].members == ()
        assert reg.combos[0].description == ""
        assert reg.combos[0].chain == ()

    def test_parse_squads_invalid_type(self):
        reg = FleetRegistry()
        with pytest.raises(TypeError):
            reg._parse_squads([1, 2, 3])

    def test_parse_squads_no_combos(self):
        reg = FleetRegistry()
        reg._parse_squads({"squads": [{"id": "s1", "name": "S1"}]})
        assert len(reg.squads) == 1
        assert len(reg.combos) == 0


if __name__ == "__main__":
    unittest.main()
