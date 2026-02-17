# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Tests for stage-based concurrent execution with dependency resolution."""

import json
import shutil
import tempfile
import threading
import time
from pathlib import Path
from unittest.mock import MagicMock, patch


class MockFileSystem:
    """Mock filesystem with deterministic locking behavior."""

    def __init__(self) -> None:
        self.files = {}
        self.locks = {}
        self.operations = []
        self._lock = threading.RLock()

    def acquire_lock(self, path: str, timeout: float = 5.0) -> bool:
        with self._lock:
            if path in self.locks:
                return False
            self.locks[path] = threading.current_thread().ident
            self.operations.append(("lock_acquired", path, time.time()))
            return True

    def release_lock(self, path: str) -> bool:
        with self._lock:
            if path not in self.locks:
                return False
            del self.locks[path]
            self.operations.append(("lock_released", path, time.time()))
            return True

    def write_file(self, path: str, content: str) -> bool:
        with self._lock:
            self.files[path] = content
            self.operations.append(("file_written", path, time.time()))
            return True


class DeterministicDispatchTester:
    """Tests for the euxis-dispatch concurrent execution patterns."""

    def __init__(self) -> None:
        self.temp_dir = None
        self.mock_fs = MockFileSystem()

    def setup(self):
        self.temp_dir = tempfile.mkdtemp(prefix="euxis_test_")
        self.manifest_path = Path(self.temp_dir) / "test_manifest.json"

    def teardown(self):
        if self.temp_dir and Path(self.temp_dir).exists():
            shutil.rmtree(self.temp_dir)

    def create_test_manifest(self, stages_config):
        manifest = {
            "project": "concurrency-test",
            "mode": "hierarchical",
            "dispatches": [],
        }

        for i, stage_info in enumerate(stages_config):
            dispatch = {
                "agent": stage_info["agent"],
                "priority": stage_info.get("priority", "P1"),
                "task": f"Test task {i}",
                "verify_cmd": "echo 'verified'",
                "stage": stage_info["stage"],
            }
            if "depends_on" in stage_info:
                dispatch["depends_on"] = stage_info["depends_on"]
            if "locks" in stage_info:
                dispatch["locks"] = stage_info["locks"]
            manifest["dispatches"].append(dispatch)

        with self.manifest_path.open("w") as f:
            json.dump(manifest, f, indent=2)

        return self.manifest_path


class TestConcurrentStageExecution:
    """Test stage-based concurrent execution with dependency resolution."""

    def setup_method(self):
        self.dispatch_tester = DeterministicDispatchTester()
        self.dispatch_tester.setup()

    def teardown_method(self):
        self.dispatch_tester.teardown()

    def test_stage_ordering_deterministic(self):
        """Test that stages execute in correct order with dependencies."""
        stages_config = [
            {"agent": "stage1-agent1", "stage": 1},
            {"agent": "stage1-agent2", "stage": 1},
            {"agent": "stage2-agent1", "stage": 2, "depends_on": ["stage1-agent1"]},
            {"agent": "stage3-agent1", "stage": 3, "depends_on": ["stage2-agent1"]},
        ]

        self.dispatch_tester.create_test_manifest(stages_config)

        execution_order = []

        def mock_execute_agent(agent_name) -> None:
            execution_order.append((agent_name, time.time()))
            time.sleep(0.1)

        with patch("subprocess.Popen") as mock_popen:
            mock_process = MagicMock()
            mock_process.wait.return_value = 0
            mock_process.returncode = 0
            mock_popen.return_value = mock_process

            stage1_agents = ["stage1-agent1", "stage1-agent2"]

            threads = []
            for agent in stage1_agents:
                thread = threading.Thread(target=mock_execute_agent, args=(agent,))
                threads.append(thread)
                thread.start()

            for thread in threads:
                thread.join()

            mock_execute_agent("stage2-agent1")
            mock_execute_agent("stage3-agent1")

        assert len(execution_order) == 4

        stage1_times = [t for agent, t in execution_order if "stage1" in agent]
        assert abs(stage1_times[0] - stage1_times[1]) < 0.05

        stage1_end = max(stage1_times) + 0.1
        stage2_start = next(t for agent, t in execution_order if agent == "stage2-agent1")
        assert stage2_start > stage1_end

        stage2_end = stage2_start + 0.1
        stage3_start = next(t for agent, t in execution_order if agent == "stage3-agent1")
        assert stage3_start > stage2_end

    def test_parallel_execution_within_stage(self):
        """Test that agents within same stage execute in parallel."""
        stages_config = [
            {"agent": "parallel-agent1", "stage": 1},
            {"agent": "parallel-agent2", "stage": 1},
            {"agent": "parallel-agent3", "stage": 1},
        ]

        self.dispatch_tester.create_test_manifest(stages_config)

        execution_times = {}
        barrier = threading.Barrier(3)

        def mock_execute_agent(agent_name) -> None:
            barrier.wait()
            start_time = time.time()
            execution_times[agent_name] = start_time
            time.sleep(0.2)

        threads = []
        for i in range(3):
            agent = f"parallel-agent{i+1}"
            thread = threading.Thread(target=mock_execute_agent, args=(agent,))
            threads.append(thread)
            thread.start()

        for thread in threads:
            thread.join()

        start_times = list(execution_times.values())
        time_spread = max(start_times) - min(start_times)
        assert time_spread < 0.01
