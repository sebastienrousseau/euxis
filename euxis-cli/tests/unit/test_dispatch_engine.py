# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Comprehensive unit tests for DispatchEngine.

Tests cover:
- Manifest loading and validation
- Agent registry validation
- Branch protection checks
- All execution modes (flat, stages, phased)
- Error handling and edge cases
- Command availability checks
"""

from __future__ import annotations

import itertools
import json
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "bin"))
from euxis_engine import (
    DEFAULT_DISPATCH_MODES,
    DispatchEngine,
    DispatchResult,
    Manifest,
    ManifestDispatch,
    ValidationError,
)


def _monotonic_time():
    """Return a side_effect callable that yields 0, 1, 2, ... on each call.

    Unlike a finite list, this never exhausts, so ``time.time`` calls
    made internally by the logging framework won't trigger StopIteration.
    """
    counter = itertools.count()
    return lambda: next(counter)


class TestManifestDispatch(unittest.TestCase):
    """Test ManifestDispatch dataclass."""

    def test_dispatch_creation_minimal(self):
        """Test creating dispatch with minimal required fields."""
        dispatch = ManifestDispatch(
            agent="test-agent",
            task="test task",
            priority="P1"
        )
        assert dispatch.agent == "test-agent"
        assert dispatch.task == "test task"
        assert dispatch.priority == "P1"
        assert dispatch.verify_cmd is None
        assert dispatch.stage == 1
        assert dispatch.depends_on == []
        assert dispatch.locks == []

    def test_dispatch_creation_full(self):
        """Test creating dispatch with all fields."""
        dispatch = ManifestDispatch(
            agent="full-agent",
            task="full task",
            priority="P0",
            verify_cmd="pytest --version",
            stage=2,
            depends_on=["agent1", "agent2"],
            locks=["file1.lock", "file2.lock"]
        )
        assert dispatch.agent == "full-agent"
        assert dispatch.verify_cmd == "pytest --version"
        assert dispatch.stage == 2
        assert dispatch.depends_on == ["agent1", "agent2"]
        assert dispatch.locks == ["file1.lock", "file2.lock"]


class TestManifestValidation(unittest.TestCase):
    """Test Manifest loading and validation."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.temp_path = Path(self.temp_dir)
        (self.temp_path / "agents").mkdir(parents=True, exist_ok=True)

    def tearDown(self):
        import shutil
        shutil.rmtree(self.temp_dir)

    def test_manifest_from_file_minimal(self):
        """Test loading minimal valid manifest."""
        manifest_data = {
            "dispatches": [
                {"agent": "test", "task": "test task", "priority": "P1"}
            ]
        }
        manifest_file = self.temp_path / "test.json"
        manifest_file.write_text(json.dumps(manifest_data))

        manifest = Manifest.from_file(manifest_file)
        assert manifest.project == "unknown"
        assert manifest.mode == "hierarchical"
        assert len(manifest.dispatches) == 1
        assert manifest.dispatches[0].agent == "test"

    def test_manifest_from_file_complete(self):
        """Test loading complete manifest with all fields."""
        manifest_data = {
            "project": "test-project",
            "mode": "mesh",
            "dispatches": [
                {
                    "agent": "architect",
                    "task": "design system",
                    "priority": "P0",
                    "verify_cmd": "grep -q 'pattern' file.txt",
                    "stage": 1,
                    "depends_on": [],
                    "locks": ["design.lock"]
                }
            ],
            "rollout_phases": [
                {
                    "name": "Phase 1",
                    "phase": 1,
                    "parallel_group": [0],
                    "checkpoint": "Design complete"
                }
            ]
        }
        manifest_file = self.temp_path / "complete.json"
        manifest_file.write_text(json.dumps(manifest_data))

        manifest = Manifest.from_file(manifest_file)
        assert manifest.project == "test-project"
        assert manifest.mode == "mesh"
        assert len(manifest.rollout_phases) == 1
        assert manifest.rollout_phases[0]["name"] == "Phase 1"

    def test_manifest_from_file_missing_file(self):
        """Test loading non-existent manifest file."""
        missing_file = self.temp_path / "missing.json"

        with pytest.raises(ValidationError) as exc_info:
            Manifest.from_file(missing_file)
        assert "Failed to load manifest" in str(exc_info.value)

    def test_manifest_from_file_invalid_json(self):
        """Test loading manifest with invalid JSON."""
        manifest_file = self.temp_path / "invalid.json"
        manifest_file.write_text('{"invalid": json}')

        with pytest.raises(ValidationError) as exc_info:
            Manifest.from_file(manifest_file)
        assert "Failed to load manifest" in str(exc_info.value)

    def test_manifest_missing_dispatches(self):
        """Test manifest without dispatches field."""
        manifest_data = {"project": "test"}
        manifest_file = self.temp_path / "no_dispatches.json"
        manifest_file.write_text(json.dumps(manifest_data))

        with pytest.raises(ValidationError) as exc_info:
            Manifest.from_file(manifest_file)
        assert "missing 'dispatches' field" in str(exc_info.value)

    def test_manifest_invalid_dispatch_format(self):
        """Test manifest with invalid dispatch format."""
        manifest_data = {
            "dispatches": ["not a dict"]
        }
        manifest_file = self.temp_path / "invalid_dispatch.json"
        manifest_file.write_text(json.dumps(manifest_data))

        with pytest.raises(ValidationError) as exc_info:
            Manifest.from_file(manifest_file)
        assert "is not a valid object" in str(exc_info.value)

    def test_manifest_missing_required_fields(self):
        """Test dispatches missing required fields."""
        test_cases = [
            ({"task": "test", "priority": "P1"}, "missing 'agent' field"),
            ({"agent": "test", "priority": "P1"}, "missing 'task' field"),
            ({"agent": "test", "task": "test"}, "missing 'priority' field"),
        ]

        for dispatch_data, expected_error in test_cases:
            manifest_data = {"dispatches": [dispatch_data]}
            manifest_file = self.temp_path / f"missing_{expected_error.split("'")[1]}.json"
            manifest_file.write_text(json.dumps(manifest_data))

            with pytest.raises(ValidationError) as exc_info:
                Manifest.from_file(manifest_file)
            assert expected_error in str(exc_info.value)


class TestDispatchEngineInitialization(unittest.TestCase):
    """Test DispatchEngine initialization and setup."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.temp_path = Path(self.temp_dir)
        (self.temp_path / "agents").mkdir(parents=True, exist_ok=True)

    def tearDown(self):
        import shutil
        shutil.rmtree(self.temp_dir)

    def test_dispatch_engine_default_init(self):
        """Test DispatchEngine with default EUXIS_HOME."""
        engine = DispatchEngine()
        assert engine.euxis_home == Path.home() / ".euxis"
        base_registry = engine.euxis_home / "euxis-core/agents" if (engine.euxis_home / "euxis-core").exists() else engine.euxis_home / "agents"
        assert engine.registry_path == base_registry / "registry.json"
        assert engine.registry_db == base_registry / "registry.db"
        assert engine.log_dir is None
        assert engine._valid_agents is None

    def test_dispatch_engine_custom_home(self):
        """Test DispatchEngine with custom euxis_home."""
        custom_home = self.temp_path / "custom_euxis"
        engine = DispatchEngine(euxis_home=custom_home)
        assert engine.euxis_home == custom_home
        base_registry = custom_home / "euxis-core/agents" if (custom_home / "euxis-core").exists() else custom_home / "agents"
        assert engine.registry_path == base_registry / "registry.json"
        assert engine.registry_db == base_registry / "registry.db"

    def _create_test_db(self, agents):
        """Create a test SQLite registry database."""
        (self.temp_path / "agents").mkdir(parents=True, exist_ok=True)
        db_path = self.temp_path / "agents/registry.db"
        conn = sqlite3.connect(str(db_path))
        conn.execute("""
            CREATE TABLE agents (
                id TEXT PRIMARY KEY, path TEXT NOT NULL,
                tier TEXT NOT NULL, version TEXT NOT NULL,
                activation TEXT
            )
        """)
        conn.execute("""
            CREATE TABLE registry_metadata (
                key TEXT PRIMARY KEY, value TEXT NOT NULL
            )
        """)
        conn.execute(
            "INSERT INTO registry_metadata VALUES ('protocol_version', 'v0.0.2')"
        )
        for agent in agents:
            conn.execute(
                "INSERT INTO agents (id, path, tier, version) VALUES (?, ?, ?, ?)",
                (agent["id"], agent.get("path", f"agents/prompts/fleet/{agent['id']}.txt"),
                 agent.get("tier", "fleet"), agent.get("version", "v0.0.2"))
            )
        conn.commit()
        conn.close()
        return db_path

    def test_load_agent_registry_from_sqlite(self):
        """Test loading agent registry from SQLite (preferred path)."""
        self._create_test_db([
            {"id": "agent1", "tier": "core"},
            {"id": "agent2", "tier": "fleet"}
        ])

        engine = DispatchEngine(euxis_home=self.temp_path)
        valid_agents = engine._load_agent_registry()

        assert len(valid_agents) == 2
        assert "agent1" in valid_agents
        assert "agent2" in valid_agents

    def test_load_agent_registry_sqlite_preferred_over_json(self):
        """Test that SQLite is preferred when both exist."""
        # Create SQLite with 2 agents
        self._create_test_db([
            {"id": "sql-agent1"},
            {"id": "sql-agent2"}
        ])
        # Create JSON with different agents
        (self.temp_path / "agents").mkdir(parents=True, exist_ok=True)
        registry_file = self.temp_path / "agents/registry.json"
        registry_file.write_text(json.dumps({
            "agents": [{"id": "json-agent1"}, {"id": "json-agent2"}, {"id": "json-agent3"}]
        }))

        engine = DispatchEngine(euxis_home=self.temp_path)
        valid_agents = engine._load_agent_registry()

        # Should load from SQLite
        assert len(valid_agents) == 2
        assert "sql-agent1" in valid_agents
        assert "json-agent1" not in valid_agents

    def test_load_agent_registry_json_fallback(self):
        """Test successful agent registry loading from JSON."""
        registry_data = {
            "agents": [
                {"id": "agent1", "tier": "core"},
                {"id": "agent2", "tier": "fleet"}
            ]
        }
        (self.temp_path / "agents").mkdir(parents=True, exist_ok=True)
        registry_file = self.temp_path / "agents/registry.json"
        registry_file.write_text(json.dumps(registry_data))

        engine = DispatchEngine(euxis_home=self.temp_path)
        valid_agents = engine._load_agent_registry()

        assert len(valid_agents) == 2
        assert "agent1" in valid_agents
        assert "agent2" in valid_agents

    def test_load_agent_registry_missing_file(self):
        """Test loading registry when no files exist."""
        engine = DispatchEngine(euxis_home=self.temp_path)
        valid_agents = engine._load_agent_registry()
        assert valid_agents == set()

    def test_load_agent_registry_invalid_json(self):
        """Test loading registry with invalid JSON (no SQLite)."""
        (self.temp_path / "agents").mkdir(parents=True, exist_ok=True)
        registry_file = self.temp_path / "agents/registry.json"
        registry_file.write_text('{"invalid": json}')

        engine = DispatchEngine(euxis_home=self.temp_path)
        valid_agents = engine._load_agent_registry()
        assert valid_agents == set()

    def test_load_agent_registry_caching(self):
        """Test that registry is cached on subsequent calls."""
        self._create_test_db([{"id": "agent1"}])

        engine = DispatchEngine(euxis_home=self.temp_path)

        # First call loads from file
        first_call = engine._load_agent_registry()

        # Second call uses cache
        second_call = engine._load_agent_registry()

        assert first_call == second_call == {"agent1"}


class TestDispatchEngineValidation(unittest.TestCase):
    """Test manifest validation logic."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.temp_path = Path(self.temp_dir)
        (self.temp_path / "agents").mkdir(parents=True, exist_ok=True)
        self.engine = DispatchEngine(euxis_home=self.temp_path)

    def tearDown(self):
        import shutil
        shutil.rmtree(self.temp_dir)

    def test_validate_manifest_valid_mode(self):
        """Test validation with valid dispatch modes."""
        for mode in DEFAULT_DISPATCH_MODES:
            manifest = Manifest(
                project="test",
                mode=mode,
                dispatches=[
                    ManifestDispatch(agent="test", task="test", priority="P1")
                ]
            )
            # Should not raise exception
            self.engine.validate_manifest(manifest)

    def test_validate_manifest_invalid_mode(self):
        """Test validation with invalid dispatch mode."""
        manifest = Manifest(
            project="test",
            mode="invalid_mode",
            dispatches=[
                ManifestDispatch(agent="test", task="test", priority="P1")
            ]
        )

        with pytest.raises(ValidationError) as exc_info:
            self.engine.validate_manifest(manifest)
        assert "Invalid dispatch mode" in str(exc_info.value)

    def test_validate_manifest_unknown_agents(self):
        """Test validation with unknown agents."""
        # Create registry with known agents
        registry_data = {"agents": [{"id": "known-agent"}]}
        registry_file = self.temp_path / "agents/registry.json"
        registry_file.write_text(json.dumps(registry_data))

        manifest = Manifest(
            project="test",
            mode="hierarchical",
            dispatches=[
                ManifestDispatch(agent="unknown-agent", task="test", priority="P1")
            ]
        )

        with pytest.raises(ValidationError) as exc_info:
            self.engine.validate_manifest(manifest)
        assert "unknown agents" in str(exc_info.value)
        assert "unknown-agent" in str(exc_info.value)

    def test_validate_manifest_no_registry_allows_any_agent(self):
        """Test that missing registry allows any agent."""
        manifest = Manifest(
            project="test",
            mode="hierarchical",
            dispatches=[
                ManifestDispatch(agent="any-agent", task="test", priority="P1")
            ]
        )

        # Should not raise exception when no registry exists
        self.engine.validate_manifest(manifest)

    @patch("euxis_engine.DispatchEngine._check_command_available")
    def test_validate_manifest_missing_verify_commands(self, mock_check_cmd):
        """Test validation with missing verify commands."""
        mock_check_cmd.return_value = False  # Command not available

        manifest = Manifest(
            project="test",
            mode="hierarchical",
            dispatches=[
                ManifestDispatch(
                    agent="test",
                    task="test",
                    priority="P1",
                    verify_cmd="missing-command --test"
                )
            ]
        )

        with pytest.raises(ValidationError) as exc_info:
            self.engine.validate_manifest(manifest)
        assert "missing tools" in str(exc_info.value)
        assert "missing-command" in str(exc_info.value)

    @patch("euxis_engine.DispatchEngine._check_command_available")
    def test_validate_manifest_negated_commands(self, mock_check_cmd):
        """Test validation handles negated verify commands."""
        mock_check_cmd.return_value = True  # Command available

        manifest = Manifest(
            project="test",
            mode="hierarchical",
            dispatches=[
                ManifestDispatch(
                    agent="test",
                    task="test",
                    priority="P1",
                    verify_cmd="! grep -q 'pattern' file.txt"
                )
            ]
        )

        # Should not raise exception - negated command should work
        self.engine.validate_manifest(manifest)
        mock_check_cmd.assert_called_with("grep")

    def test_check_command_available_success(self):
        """Test command availability check for existing command."""
        # Test with a command that should exist on all systems
        available = self.engine._check_command_available("echo")
        assert available is True

    def test_check_command_available_failure(self):
        """Test command availability check for non-existent command."""
        available = self.engine._check_command_available("definitely-not-a-real-command-12345")
        assert available is False


class TestBranchProtection(unittest.TestCase):
    """Test branch protection checks."""

    def setUp(self):
        self.engine = DispatchEngine()

    @patch("subprocess.run")
    def test_check_branch_protection_protected_branch(self, mock_run):
        """Test branch protection for protected branches."""
        protected_branches = ["main", "master", "develop", "production", "staging"]

        for branch in protected_branches:
            mock_run.return_value = Mock(stdout=f"{branch}\n", returncode=0)

            with pytest.raises(ValidationError) as exc_info:
                self.engine.check_branch_protection()
            assert "protected branch" in str(exc_info.value)
            assert branch in str(exc_info.value)

    @patch("subprocess.run")
    def test_check_branch_protection_compliant_branch(self, mock_run):
        """Test branch protection for compliant feature branches."""
        compliant_branches = ["feat/new-feature", "fix/bug-123", "refactor/cleanup", "chore/update"]

        for branch in compliant_branches:
            mock_run.return_value = Mock(stdout=f"{branch}\n", returncode=0)
            # Should not raise exception
            self.engine.check_branch_protection()

    @patch("subprocess.run")
    def test_check_branch_protection_non_compliant_branch(self, mock_run):
        """Test branch protection for non-compliant but allowed branches."""
        mock_run.return_value = Mock(stdout="random-branch-name\n", returncode=0)
        # Should not raise exception, only warn
        self.engine.check_branch_protection()

    @patch("subprocess.run")
    def test_check_branch_protection_git_error(self, mock_run):
        """Test branch protection when git command fails."""
        mock_run.side_effect = subprocess.CalledProcessError(1, ["git"])
        # Should not raise exception, only warn
        self.engine.check_branch_protection()


class TestLogDirectorySetup(unittest.TestCase):
    """Test log directory creation and management."""

    def test_setup_log_directory(self):
        """Test log directory creation."""
        engine = DispatchEngine()
        log_dir = engine.setup_log_directory()

        assert log_dir.exists()
        assert log_dir.is_dir()
        assert log_dir == engine.log_dir
        assert "euxis_dispatch_" in str(log_dir)

    def test_setup_log_directory_multiple_calls(self):
        """Test multiple log directory setups."""
        engine = DispatchEngine()
        log_dir1 = engine.setup_log_directory()
        log_dir2 = engine.setup_log_directory()

        # Second call should create new directory
        assert log_dir1 != log_dir2
        assert engine.log_dir == log_dir2


@patch("euxis_engine.subprocess.Popen")
@patch("euxis_engine.DispatchEngine.setup_log_directory")
@patch("euxis_engine.DispatchEngine._resolve_euxis_loop_path", return_value=Path("/usr/bin/euxis-loop"))
class TestSingleAgentDispatch(unittest.TestCase):
    """Test single agent dispatch functionality."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.temp_path = Path(self.temp_dir)
        (self.temp_path / "agents").mkdir(parents=True, exist_ok=True)
        self.engine = DispatchEngine()
        self.engine.log_dir = self.temp_path

    def tearDown(self):
        import shutil
        shutil.rmtree(self.temp_dir)

    def test_dispatch_single_agent_basic(self, mock_loop_path, mock_setup_log, mock_popen):
        """Test basic single agent dispatch."""
        mock_process = Mock()
        mock_popen.return_value = mock_process

        dispatch = ManifestDispatch(
            agent="test-agent",
            task="test task",
            priority="P1"
        )

        result = self.engine.dispatch_single_agent(dispatch, 0, "hierarchical", "test-project")

        assert result == mock_process
        mock_popen.assert_called_once()

        # Verify command structure
        call_args = mock_popen.call_args[0][0]
        assert "test-agent" in call_args
        assert "test task" in call_args[2]

    def test_dispatch_single_agent_mesh_mode(self, mock_loop_path, mock_setup_log, mock_popen):
        """Test single agent dispatch in mesh mode with dispatch authority."""
        mock_popen.return_value = Mock()

        dispatch = ManifestDispatch(
            agent="architect",  # Has dispatch authority
            task="design system",
            priority="P0"
        )

        self.engine.dispatch_single_agent(dispatch, 0, "mesh", "test-project")

        # Verify mesh mode instructions were added
        call_args = mock_popen.call_args[0][0]
        task_arg = call_args[2]
        assert "[MESH MODE]" in task_arg
        assert "dispatch authority" in task_arg

    def test_dispatch_single_agent_federated_mode(self, mock_loop_path, mock_setup_log, mock_popen):
        """Test single agent dispatch in federated mode."""
        mock_popen.return_value = Mock()

        dispatch = ManifestDispatch(
            agent="test-agent",
            task="autonomous task",
            priority="P1"
        )

        self.engine.dispatch_single_agent(dispatch, 0, "federated", "test-project")

        # Verify federated mode instructions were added
        call_args = mock_popen.call_args[0][0]
        task_arg = call_args[2]
        assert "[FEDERATED MODE]" in task_arg
        assert "autonomously" in task_arg

    def test_dispatch_single_agent_with_verify_cmd(self, mock_loop_path, mock_setup_log, mock_popen):
        """Test single agent dispatch with verify command."""
        mock_popen.return_value = Mock()

        dispatch = ManifestDispatch(
            agent="test-agent",
            task="test task",
            priority="P1",
            verify_cmd="pytest --version"
        )

        self.engine.dispatch_single_agent(dispatch, 0, "hierarchical", "test-project")

        call_args = mock_popen.call_args[0][0]
        assert "pytest --version" in call_args

    def test_dispatch_single_agent_atomic_instructions(self, mock_loop_path, mock_setup_log, mock_popen):
        """Test that atomic file operation instructions are added."""
        mock_popen.return_value = Mock()

        dispatch = ManifestDispatch(
            agent="test-agent",
            task="file operations",
            priority="P1"
        )

        self.engine.dispatch_single_agent(dispatch, 0, "hierarchical", "test-project")

        call_args = mock_popen.call_args[0][0]
        task_arg = call_args[2]
        assert "ATOMIC FILE OPERATIONS" in task_arg
        assert "temp files" in task_arg


class TestExecuteDispatches(unittest.TestCase):
    """Test dispatch execution coordination."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.temp_path = Path(self.temp_dir)
        (self.temp_path / "agents").mkdir(parents=True, exist_ok=True)
        self.engine = DispatchEngine(euxis_home=self.temp_path)

    def tearDown(self):
        import shutil
        shutil.rmtree(self.temp_dir)

    def test_execute_dispatches_empty_manifest(self):
        """Test execution with empty manifest."""
        manifest = Manifest(
            project="test",
            mode="hierarchical",
            dispatches=[]
        )

        results = self.engine.execute_dispatches(manifest)
        assert results == []

    @patch("euxis_engine.DispatchEngine._execute_flat_dispatch")
    def test_execute_dispatches_flat_mode(self, mock_flat):
        """Test execution routes to flat dispatch mode."""
        mock_flat.return_value = []

        manifest = Manifest(
            project="test",
            mode="hierarchical",
            dispatches=[
                ManifestDispatch(agent="test", task="test", priority="P1")
            ]
        )

        self.engine.execute_dispatches(manifest)
        mock_flat.assert_called_once_with(manifest, "hierarchical")

    @patch("euxis_engine.DispatchEngine._execute_by_stages")
    def test_execute_dispatches_stage_mode(self, mock_stages):
        """Test execution routes to stage mode when stages present."""
        mock_stages.return_value = []

        manifest = Manifest(
            project="test",
            mode="hierarchical",
            dispatches=[
                ManifestDispatch(agent="test", task="test", priority="P1", stage=2)
            ]
        )

        self.engine.execute_dispatches(manifest)
        mock_stages.assert_called_once_with(manifest, "hierarchical")

    @patch("euxis_engine.DispatchEngine._execute_phased_rollout")
    def test_execute_dispatches_phased_mode(self, mock_phased):
        """Test execution routes to phased mode when phases present."""
        mock_phased.return_value = []

        manifest = Manifest(
            project="test",
            mode="hierarchical",
            dispatches=[
                ManifestDispatch(agent="test", task="test", priority="P1")
            ],
            rollout_phases=[{"name": "Phase 1", "parallel_group": [0]}]
        )

        self.engine.execute_dispatches(manifest)
        mock_phased.assert_called_once_with(manifest, "hierarchical")

    def test_execute_dispatches_mode_override(self):
        """Test execution with mode override."""
        manifest = Manifest(
            project="test",
            mode="hierarchical",
            dispatches=[
                ManifestDispatch(agent="test", task="test", priority="P1")
            ]
        )

        with patch.object(self.engine, "_execute_flat_dispatch", return_value=[]) as mock_flat:
            self.engine.execute_dispatches(manifest, mode="mesh")
            mock_flat.assert_called_once_with(manifest, "mesh")


class TestFlatExecution(unittest.TestCase):
    """Test flat dispatch execution mode."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.temp_path = Path(self.temp_dir)
        (self.temp_path / "agents").mkdir(parents=True, exist_ok=True)
        self.engine = DispatchEngine(euxis_home=self.temp_path)
        self.engine.log_dir = self.temp_path

    def tearDown(self):
        import shutil
        shutil.rmtree(self.temp_dir)

    @patch("euxis_engine.DispatchEngine.dispatch_single_agent")
    @patch("time.sleep")
    def test_execute_flat_dispatch_success(self, mock_sleep, mock_dispatch):
        """Test successful flat execution."""
        # Mock processes
        mock_process1 = Mock()
        mock_process1.wait.return_value = 0
        mock_process2 = Mock()
        mock_process2.wait.return_value = 0

        mock_dispatch.side_effect = [mock_process1, mock_process2]

        manifest = Manifest(
            project="test",
            mode="hierarchical",
            dispatches=[
                ManifestDispatch(agent="agent1", task="task1", priority="P1"),
                ManifestDispatch(agent="agent2", task="task2", priority="P2")
            ]
        )

        with patch("euxis_engine.time.time", side_effect=_monotonic_time()):
            results = self.engine._execute_flat_dispatch(manifest, "hierarchical")

        assert len(results) == 2
        assert all(r.success for r in results)
        assert results[0].agent == "agent1"
        assert results[1].agent == "agent2"
        assert results[0].duration > 0
        assert results[1].duration > 0

    @patch("euxis_engine.DispatchEngine.dispatch_single_agent")
    @patch("time.sleep")
    def test_execute_flat_dispatch_failure(self, mock_sleep, mock_dispatch):
        """Test flat execution with failures."""
        # Mock processes - one success, one failure
        mock_process1 = Mock()
        mock_process1.wait.return_value = 0
        mock_process2 = Mock()
        mock_process2.wait.return_value = 1

        mock_dispatch.side_effect = [mock_process1, mock_process2]

        manifest = Manifest(
            project="test",
            mode="hierarchical",
            dispatches=[
                ManifestDispatch(agent="success-agent", task="task1", priority="P1"),
                ManifestDispatch(agent="fail-agent", task="task2", priority="P1")
            ]
        )

        with patch("euxis_engine.time.time", side_effect=_monotonic_time()):
            results = self.engine._execute_flat_dispatch(manifest, "hierarchical")

        assert len(results) == 2
        assert results[0].success is True
        assert results[1].success is False
        assert results[0].return_code == 0
        assert results[1].return_code == 1


class TestStageExecution(unittest.TestCase):
    """Test stage-based execution mode."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.temp_path = Path(self.temp_dir)
        (self.temp_path / "agents").mkdir(parents=True, exist_ok=True)
        self.engine = DispatchEngine(euxis_home=self.temp_path)
        self.engine.log_dir = self.temp_path

    def tearDown(self):
        import shutil
        shutil.rmtree(self.temp_dir)

    @patch("euxis_engine.DispatchEngine.dispatch_single_agent")
    @patch("time.sleep")
    def test_execute_by_stages_success(self, mock_sleep, mock_dispatch):
        """Test successful stage execution."""
        # Mock successful processes
        mock_process = Mock()
        mock_process.wait.return_value = 0
        mock_dispatch.return_value = mock_process

        manifest = Manifest(
            project="test",
            mode="hierarchical",
            dispatches=[
                ManifestDispatch(agent="stage1-agent", task="task1", priority="P1", stage=1),
                ManifestDispatch(agent="stage2-agent", task="task2", priority="P1", stage=2)
            ]
        )

        with patch("euxis_engine.time.time", side_effect=_monotonic_time()):
            results = self.engine._execute_by_stages(manifest, "hierarchical")

        assert len(results) == 2
        assert all(r.success for r in results)

    @patch("euxis_engine.DispatchEngine.dispatch_single_agent")
    @patch("time.sleep")
    def test_execute_by_stages_with_dependencies(self, mock_sleep, mock_dispatch):
        """Test stage execution with dependencies across stages."""
        mock_process = Mock()
        mock_process.wait.return_value = 0
        mock_dispatch.return_value = mock_process

        manifest = Manifest(
            project="test",
            mode="hierarchical",
            dispatches=[
                ManifestDispatch(agent="independent", task="task1", priority="P1", stage=1),
                ManifestDispatch(
                    agent="dependent",
                    task="task2",
                    priority="P1",
                    stage=2,
                    depends_on=["independent"]
                )
            ]
        )

        with patch("euxis_engine.time.time", side_effect=_monotonic_time()):
            results = self.engine._execute_by_stages(manifest, "hierarchical")

        # independent runs in stage 1, dependent runs in stage 2
        assert len(results) == 2
        assert all(r.success for r in results)

    @patch("euxis_engine.DispatchEngine.dispatch_single_agent")
    @patch("time.sleep")
    def test_execute_by_stages_failure_aborts(self, mock_sleep, mock_dispatch):
        """Test that stage failure aborts remaining stages."""
        # First process succeeds, second fails
        mock_success = Mock()
        mock_success.wait.return_value = 0
        mock_failure = Mock()
        mock_failure.wait.return_value = 1

        mock_dispatch.side_effect = [mock_success, mock_failure]

        manifest = Manifest(
            project="test",
            mode="hierarchical",
            dispatches=[
                ManifestDispatch(agent="success", task="task1", priority="P1", stage=1),
                ManifestDispatch(agent="failure", task="task2", priority="P1", stage=1),
                ManifestDispatch(agent="skipped", task="task3", priority="P1", stage=2)
            ]
        )

        with patch("euxis_engine.time.time", side_effect=_monotonic_time()):
            results = self.engine._execute_by_stages(manifest, "hierarchical")

        # Should only have 2 results, third stage should be skipped
        assert len(results) == 2
        assert results[0].success is True
        assert results[1].success is False


class TestPhasedExecution(unittest.TestCase):
    """Test phased rollout execution mode."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.temp_path = Path(self.temp_dir)
        (self.temp_path / "agents").mkdir(parents=True, exist_ok=True)
        self.engine = DispatchEngine(euxis_home=self.temp_path)
        self.engine.log_dir = self.temp_path

    def tearDown(self):
        import shutil
        shutil.rmtree(self.temp_dir)

    @patch("euxis_engine.DispatchEngine.dispatch_single_agent")
    @patch("time.sleep")
    def test_execute_phased_rollout_success(self, mock_sleep, mock_dispatch):
        """Test successful phased execution."""
        mock_process = Mock()
        mock_process.wait.return_value = 0
        mock_dispatch.return_value = mock_process

        manifest = Manifest(
            project="test",
            mode="hierarchical",
            dispatches=[
                ManifestDispatch(agent="agent1", task="task1", priority="P1"),
                ManifestDispatch(agent="agent2", task="task2", priority="P1")
            ],
            rollout_phases=[
                {
                    "name": "Phase 1",
                    "phase": 1,
                    "parallel_group": [0, 1],
                    "checkpoint": "All tasks complete"
                }
            ]
        )

        with patch("euxis_engine.time.time", side_effect=_monotonic_time()):
            results = self.engine._execute_phased_rollout(manifest, "hierarchical")

        assert len(results) == 2
        assert all(r.success for r in results)

    @patch("euxis_engine.DispatchEngine.dispatch_single_agent")
    @patch("time.sleep")
    def test_execute_phased_rollout_invalid_index(self, mock_sleep, mock_dispatch):
        """Test phased execution with invalid dispatch index."""
        mock_process = Mock()
        mock_process.wait.return_value = 0
        mock_dispatch.return_value = mock_process

        manifest = Manifest(
            project="test",
            mode="hierarchical",
            dispatches=[
                ManifestDispatch(agent="agent1", task="task1", priority="P1")
            ],
            rollout_phases=[
                {
                    "name": "Phase 1",
                    "phase": 1,
                    "parallel_group": [0, 99],  # Invalid index
                    "checkpoint": "Phase complete"
                }
            ]
        )

        with patch("euxis_engine.time.time", side_effect=_monotonic_time()):
            results = self.engine._execute_phased_rollout(manifest, "hierarchical")

        # Should only process valid index
        assert len(results) == 1
        assert results[0].success is True

    @patch("euxis_engine.DispatchEngine.dispatch_single_agent")
    @patch("time.sleep")
    def test_execute_phased_rollout_failure_aborts(self, mock_sleep, mock_dispatch):
        """Test that phase failure aborts remaining phases."""
        # First process succeeds, second fails
        mock_success = Mock()
        mock_success.wait.return_value = 0
        mock_failure = Mock()
        mock_failure.wait.return_value = 1

        mock_dispatch.side_effect = [mock_success, mock_failure]

        manifest = Manifest(
            project="test",
            mode="hierarchical",
            dispatches=[
                ManifestDispatch(agent="success", task="task1", priority="P1"),
                ManifestDispatch(agent="failure", task="task2", priority="P1"),
                ManifestDispatch(agent="skipped", task="task3", priority="P1")
            ],
            rollout_phases=[
                {
                    "name": "Phase 1",
                    "phase": 1,
                    "parallel_group": [0, 1],
                    "checkpoint": "First phase"
                },
                {
                    "name": "Phase 2",
                    "phase": 2,
                    "parallel_group": [2],
                    "checkpoint": "Second phase"
                }
            ]
        )

        with patch("euxis_engine.time.time", side_effect=_monotonic_time()):
            results = self.engine._execute_phased_rollout(manifest, "hierarchical")

        # Should only have 2 results from first phase
        assert len(results) == 2
        assert results[0].success is True
        assert results[1].success is False


class TestMainFunction(unittest.TestCase):
    """Test CLI main function."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.temp_path = Path(self.temp_dir)
        (self.temp_path / "agents").mkdir(parents=True, exist_ok=True)

    def tearDown(self):
        import shutil
        shutil.rmtree(self.temp_dir)

    @patch("sys.argv", ["euxis_engine.py", "test.json"])
    @patch("euxis_engine.DispatchEngine")
    @patch("euxis_engine.Manifest")
    def test_main_success(self, mock_manifest_class, mock_engine_class):
        """Test successful main execution."""
        # Setup mocks
        mock_manifest = Mock()
        mock_manifest.mode = "hierarchical"
        mock_manifest_class.from_file.return_value = mock_manifest

        mock_engine = Mock()
        mock_engine.execute_dispatches.return_value = [
            DispatchResult(agent="test", index=0, success=True, log_file=Path("test.log"))
        ]
        mock_engine_class.return_value = mock_engine

        # Import and run main
        from euxis_engine import main
        result = main()

        assert result == 0
        mock_engine.validate_manifest.assert_called_once()
        mock_engine.check_branch_protection.assert_called_once()
        mock_engine.execute_dispatches.assert_called_once()

    @patch("sys.argv", ["euxis_engine.py", "test.json", "--validate-only"])
    @patch("euxis_engine.DispatchEngine")
    @patch("euxis_engine.Manifest")
    def test_main_validate_only(self, mock_manifest_class, mock_engine_class):
        """Test main with validate-only flag."""
        mock_manifest = Mock()
        mock_manifest_class.from_file.return_value = mock_manifest

        mock_engine = Mock()
        mock_engine_class.return_value = mock_engine

        from euxis_engine import main
        result = main()

        assert result == 0
        mock_engine.validate_manifest.assert_called_once()
        mock_engine.execute_dispatches.assert_not_called()

    @patch("sys.argv", ["euxis_engine.py", "test.json"])
    @patch("euxis_engine.DispatchEngine")
    @patch("euxis_engine.Manifest")
    def test_main_with_failures(self, mock_manifest_class, mock_engine_class):
        """Test main execution with failures."""
        mock_manifest = Mock()
        mock_manifest.mode = "hierarchical"
        mock_manifest_class.from_file.return_value = mock_manifest

        mock_engine = Mock()
        mock_engine.execute_dispatches.return_value = [
            DispatchResult(agent="success", index=0, success=True, log_file=Path("success.log")),
            DispatchResult(agent="failure", index=1, success=False, log_file=Path("failure.log"))
        ]
        mock_engine_class.return_value = mock_engine

        from euxis_engine import main
        result = main()

        assert result == 1

    @patch("sys.argv", ["euxis_engine.py", "nonexistent.json"])
    @patch("euxis_engine.DispatchEngine")
    @patch("euxis_engine.Manifest")
    def test_main_validation_error(self, mock_manifest_class, mock_engine_class):
        """Test main with validation error."""
        mock_manifest_class.from_file.side_effect = ValidationError("Invalid manifest")

        from euxis_engine import main
        result = main()

        assert result == 1

    @patch("sys.argv", ["euxis_engine.py", "test.json"])
    @patch("euxis_engine.DispatchEngine")
    def test_main_unexpected_error(self, mock_engine_class):
        """Test main with unexpected error."""
        mock_engine_class.side_effect = Exception("Unexpected error")

        from euxis_engine import main
        result = main()

        assert result == 1


if __name__ == "__main__":
    unittest.main()
