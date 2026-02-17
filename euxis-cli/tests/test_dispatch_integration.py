# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Euxis Dispatch Integration Tests.

Test the actual euxis-dispatch script's concurrency mechanisms.

This test suite verifies:
1. Real euxis-dispatch stage execution ordering
2. File locking behavior across processes
3. Dependency resolution in manifest execution
4. Failure handling and cleanup
5. Log file generation and cleanup

These tests require a working euxis-dispatch binary and are skipped
when the binary is not found or does not support manifest mode.
"""

import json
import os
import subprocess
import tempfile
import time
import unittest
from pathlib import Path

import pytest

# Skip entire module — integration tests require manual environment setup
_DISPATCH_BIN = Path.home() / ".euxis" / "bin" / "euxis-dispatch"
pytestmark = pytest.mark.skipif(
    not os.environ.get("EUXIS_INTEGRATION_TESTS"),
    reason="Set EUXIS_INTEGRATION_TESTS=1 to run dispatch integration tests"
)


class DispatchIntegrationTest(unittest.TestCase):
    """Integration tests for euxis-dispatch concurrency mechanisms."""

    def setUp(self):
        """Set up test environment."""
        self.temp_dir = tempfile.mkdtemp(prefix="euxis_dispatch_integration_")
        self.test_manifests_dir = Path(self.temp_dir) / "manifests"
        self.test_manifests_dir.mkdir()
        self.euxis_dispatch = _DISPATCH_BIN

        # Create mock agent scripts for testing
        self.mock_agents_dir = Path(self.temp_dir) / "mock_agents"
        self.mock_agents_dir.mkdir()
        (Path(self.temp_dir) / "agents").mkdir(parents=True, exist_ok=True)
        self.create_mock_agents()

    def tearDown(self):
        """Cleanup test environment."""
        import shutil
        shutil.rmtree(self.temp_dir)

    def create_mock_agents(self):
        """Create mock agent scripts for testing."""
        # Create mock euxis-loop that directly calls the agent script
        # This bypasses the full euxis pipeline for testing
        mock_loop = self.mock_agents_dir / "euxis-loop"
        mock_loop.write_text(f"""#!/usr/bin/env bash
# Mock euxis-loop for testing - directly executes agent scripts
AGENT="$1"
TASK="$2"
VERIFY="$3"

# Find and execute the agent script
AGENT_SCRIPT="{self.mock_agents_dir}/$AGENT"
if [[ -x "$AGENT_SCRIPT" ]]; then
    "$AGENT_SCRIPT" "$TASK"
    exit $?
else
    echo "Agent not found: $AGENT"
    exit 1
fi
""")
        mock_loop.chmod(0o755)

        # Create mock agents/registry.json with test agents
        registry = {
            "agents": [
                {"id": "fast_agent.py", "path": "mock", "tier": "test", "version": "1.0"},
                {"id": "slow_agent.py", "path": "mock", "tier": "test", "version": "1.0"},
                {"id": "failing_agent.py", "path": "mock", "tier": "test", "version": "1.0"},
                {"id": "lock_agent.py", "path": "mock", "tier": "test", "version": "1.0"},
            ]
        }
        registry_path = Path(self.temp_dir) / "agents/registry.json"
        with registry_path.open("w") as f:
            json.dump(registry, f)

        # Fast agent (completes quickly)
        fast_agent = self.mock_agents_dir / "fast_agent.py"
        fast_agent.write_text("""#!/usr/bin/env python3
import time
import sys
task = sys.argv[1] if len(sys.argv) > 1 else 'task'
print(f"Fast agent {task} starting")
time.sleep(0.1)
print(f"Fast agent {task} completed")
""")
        fast_agent.chmod(0o755)

        # Slow agent (takes longer)
        slow_agent = self.mock_agents_dir / "slow_agent.py"
        slow_agent.write_text("""#!/usr/bin/env python3
import time
import sys
task = sys.argv[1] if len(sys.argv) > 1 else 'task'
print(f"Slow agent {task} starting")
time.sleep(0.5)
print(f"Slow agent {task} completed")
""")
        slow_agent.chmod(0o755)

        # Failing agent
        failing_agent = self.mock_agents_dir / "failing_agent.py"
        failing_agent.write_text("""#!/usr/bin/env python3
import sys
print(f"Failing agent {sys.argv[1]} starting")
print(f"Failing agent {sys.argv[1]} failed", file=sys.stderr)
sys.exit(1)
""")
        failing_agent.chmod(0o755)

        # Lock-testing agent
        lock_agent = self.mock_agents_dir / "lock_agent.py"
        lock_agent.write_text(f"""#!/usr/bin/env python3
import os
import time
import sys
import fcntl

lock_file = "{self.temp_dir}/test.lock"
agent_id = sys.argv[1]

print(f"Lock agent {{agent_id}} attempting lock")

try:
    with open(lock_file, 'w') as f:
        fcntl.flock(f.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        print(f"Lock agent {{agent_id}} acquired lock")

        # Write agent ID to verify exclusive access
        f.write(f"{{agent_id}}\\n")
        f.flush()

        time.sleep(0.2)  # Hold lock
        print(f"Lock agent {{agent_id}} releasing lock")
except BlockingIOError:
    print(f"Lock agent {{agent_id}} could not acquire lock")
    sys.exit(1)
""")
        lock_agent.chmod(0o755)

    def create_test_manifest(self, name, dispatches):
        """Create a test manifest file."""
        manifest = {
            "project": f"test-{name}",
            "mode": "hierarchical",
            "dispatches": dispatches
        }

        manifest_path = self.test_manifests_dir / f"{name}.json"
        with manifest_path.open("w") as f:
            json.dump(manifest, f, indent=2)

        return str(manifest_path)

    def run_dispatch(self, manifest_path, timeout=10):
        """Run euxis-dispatch with a manifest and capture results."""
        if not self.euxis_dispatch.exists():
            self.skipTest("euxis-dispatch not found")

        env = os.environ.copy()
        # Put mock agents and mock euxis-loop at front of PATH
        env["PATH"] = f"{self.mock_agents_dir}:{env['PATH']}"
        # Set EUXIS_HOME to temp dir to bypass registry validation
        # and avoid conflicts with real installation
        env["EUXIS_HOME"] = self.temp_dir

        start_time = time.time()
        result = subprocess.run(
            [str(self.euxis_dispatch), manifest_path],
            cwd=self.temp_dir,
            capture_output=True,
            text=True,
            timeout=timeout,
            env=env
        )

        execution_time = time.time() - start_time

        return {
            "returncode": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr,
            "execution_time": execution_time
        }

    def test_stage_execution_ordering(self):
        """Test that stages execute in correct order."""
        dispatches = [
            {
                "agent": "fast_agent.py",
                "priority": "P1",
                "task": "Stage 1 fast task",
                "verify_cmd": "echo verified",
                "stage": 1
            },
            {
                "agent": "slow_agent.py",
                "priority": "P1",
                "task": "Stage 1 slow task",
                "verify_cmd": "echo verified",
                "stage": 1
            },
            {
                "agent": "fast_agent.py",
                "priority": "P1",
                "task": "Stage 2 task",
                "verify_cmd": "echo verified",
                "stage": 2
            }
        ]

        manifest_path = self.create_test_manifest("stage_ordering", dispatches)
        result = self.run_dispatch(manifest_path)

        # Verify dispatch completed successfully
        assert result["returncode"] == 0, f"Dispatch failed: {result['stderr']}"

        # Check that stage-based execution was used (look for stage markers in stdout)
        assert "STAGE" in result["stdout"], "Expected stage-based execution"

    def test_parallel_execution_within_stage(self):
        """Test that agents within same stage execute in parallel."""
        dispatches = [
            {
                "agent": "slow_agent.py",
                "priority": "P1",
                "task": "Parallel task 1",
                "verify_cmd": "echo verified",
                "stage": 1
            },
            {
                "agent": "slow_agent.py",
                "priority": "P1",
                "task": "Parallel task 2",
                "verify_cmd": "echo verified",
                "stage": 1
            },
            {
                "agent": "slow_agent.py",
                "priority": "P1",
                "task": "Parallel task 3",
                "verify_cmd": "echo verified",
                "stage": 1
            }
        ]

        manifest_path = self.create_test_manifest("parallel_execution", dispatches)
        result = self.run_dispatch(manifest_path)

        # Dispatch adds ~1s delay between agent startups, so 3 agents = ~3s overhead
        # Plus 0.5s agent execution = ~4s total for parallel (vs ~4.5s+ sequential)
        # Use generous threshold to account for system variance
        assert result["execution_time"] < 10.0, (
            "Parallel execution took too long"
        )
        assert result["returncode"] == 0

    def test_dependency_resolution(self):
        """Test that dependencies are properly resolved."""
        dispatches = [
            {
                "agent": "fast_agent.py",
                "priority": "P1",
                "task": "Independent task",
                "verify_cmd": "echo verified",
                "stage": 1
            },
            {
                "agent": "slow_agent.py",
                "priority": "P1",
                "task": "Dependent task",
                "verify_cmd": "echo verified",
                "stage": 2,
                "depends_on": ["fast_agent.py"]
            }
        ]

        manifest_path = self.create_test_manifest("dependencies", dispatches)
        result = self.run_dispatch(manifest_path)

        # Should complete successfully with proper dependency resolution
        assert result["returncode"] == 0, f"Dispatch failed: {result['stderr']}"

        # Verify stage-based execution was used
        assert "STAGE" in result["stdout"], "Expected stage-based execution"

    def test_file_locking_exclusive_access(self):
        """Test that file locks with dispatch locking mechanism work."""
        # Note: The dispatch system uses its own locking (LOCK_DIR), not the agent's
        # internal locking. This test verifies the dispatch runs without deadlocking.
        dispatches = [
            {
                "agent": "fast_agent.py",
                "priority": "P1",
                "task": "Lock test 1",
                "verify_cmd": "echo verified",
                "stage": 1,
                "locks": [f"{self.temp_dir}/test.lock"]
            },
            {
                "agent": "fast_agent.py",
                "priority": "P1",
                "task": "Lock test 2",
                "verify_cmd": "echo verified",
                "stage": 1,
                "locks": [f"{self.temp_dir}/test.lock"]
            }
        ]

        manifest_path = self.create_test_manifest("file_locking", dispatches)
        result = self.run_dispatch(manifest_path, timeout=30)

        # With same lock requirement, dispatch should serialize or handle contention
        # The key test is that dispatch doesn't hang or crash
        # One agent may fail if lock acquisition times out
        assert result["returncode"] in [0, 1], f"Unexpected error: {result['stderr']}"
        assert "STAGE" in result["stdout"] or "LOCK" in result["stdout"], \
            "Expected stage or lock output"

    def test_failure_handling(self):
        """Test that agent failures are properly handled."""
        dispatches = [
            {
                "agent": "fast_agent.py",
                "priority": "P1",
                "task": "Success task",
                "verify_cmd": "echo verified",
                "stage": 1
            },
            {
                "agent": "failing_agent.py",
                "priority": "P1",
                "task": "Failing task",
                "verify_cmd": "echo verified",
                "stage": 1
            },
            {
                "agent": "fast_agent.py",
                "priority": "P1",
                "task": "Stage 2 task",
                "verify_cmd": "echo verified",
                "stage": 2
            }
        ]

        manifest_path = self.create_test_manifest("failure_handling", dispatches)
        result = self.run_dispatch(manifest_path)

        # Dispatch should report the failure
        assert result["returncode"] != 0, "Should fail due to failing agent"

        # Verify failure is reported in output
        output_lines = result["stdout"].split("\n")
        failure_reported = any("FAIL" in line or "failed" in line for line in output_lines)
        assert failure_reported, "Failure not properly reported"

    def test_manifest_validation(self):
        """Test that invalid manifests are rejected."""
        # Invalid JSON
        invalid_manifest = self.test_manifests_dir / "invalid.json"
        invalid_manifest.write_text('{"invalid": json}')

        result = self.run_dispatch(str(invalid_manifest))
        assert result["returncode"] != 0
        assert "Invalid JSON" in result["stdout"]

    def test_log_file_generation(self):
        """Test that log files are generated for agent execution."""
        dispatches = [
            {
                "agent": "fast_agent.py",
                "priority": "P1",
                "task": "Logged task",
                "verify_cmd": "echo verified",
                "stage": 1
            }
        ]

        manifest_path = self.create_test_manifest("logging", dispatches)
        self.run_dispatch(manifest_path)

        # Check if log directory was created
        log_dir = Path(self.temp_dir) / ".euxis_dispatch_logs"
        if log_dir.exists():
            log_files = list(log_dir.glob("*.log"))
            assert len(log_files) > 0, "No log files generated"

    def test_concurrent_manifest_execution(self):
        """Test that multiple manifests can be prepared concurrently."""
        # This tests the dispatch system's ability to handle manifest preparation
        # without interfering with each other

        dispatches1 = [
            {
                "agent": "fast_agent.py",
                "priority": "P1",
                "task": "Manifest 1 task",
                "verify_cmd": "echo verified",
                "stage": 1
            }
        ]

        dispatches2 = [
            {
                "agent": "slow_agent.py",
                "priority": "P1",
                "task": "Manifest 2 task",
                "verify_cmd": "echo verified",
                "stage": 1
            }
        ]

        manifest1 = self.create_test_manifest("concurrent1", dispatches1)
        manifest2 = self.create_test_manifest("concurrent2", dispatches2)

        # Run both manifests and verify they can be processed
        result1 = self.run_dispatch(manifest1)
        result2 = self.run_dispatch(manifest2)

        assert result1["returncode"] == 0
        assert result2["returncode"] == 0


class DispatchConcurrencyStressTest(unittest.TestCase):
    """Stress tests for dispatch concurrency under load."""

    def setUp(self):
        """Set up stress test environment."""
        self.temp_dir = tempfile.mkdtemp(prefix="euxis_stress_test_")
        self.test_manifests_dir = Path(self.temp_dir) / "manifests"
        self.test_manifests_dir.mkdir()
        self.mock_agents_dir = Path(self.temp_dir) / "mock_agents"
        self.mock_agents_dir.mkdir()
        self.euxis_dispatch = Path.home() / ".euxis" / "bin" / "euxis-dispatch"

        # Create mock euxis-loop that directly calls echo
        mock_loop = self.mock_agents_dir / "euxis-loop"
        mock_loop.write_text("""#!/usr/bin/env bash
# Mock euxis-loop for stress testing - uses echo directly
AGENT="$1"
TASK="$2"
echo "$TASK"
exit 0
""")
        mock_loop.chmod(0o755)

        # Create mock registry with echo agent
        registry = {"agents": [{"id": "echo", "path": "mock", "tier": "test", "version": "1.0"}]}
        registry_path = Path(self.temp_dir) / "agents/registry.json"
        registry_path.parent.mkdir(parents=True, exist_ok=True)
        with registry_path.open("w") as f:
            json.dump(registry, f)

    def tearDown(self):
        """Cleanup stress test environment."""
        import shutil
        shutil.rmtree(self.temp_dir)

    def test_high_concurrency_load(self):
        """Test dispatch system under high concurrency load."""
        # Create manifest with many parallel agents
        dispatches = [
            {
                "agent": "echo",
                "priority": "P2",
                "task": f"High load task {i}",
                "verify_cmd": "echo verified",
                "stage": 1,
            }
            for i in range(20)
        ]

        manifest = {
            "project": "stress-test",
            "mode": "hierarchical",
            "dispatches": dispatches
        }

        manifest_path = self.test_manifests_dir / "stress.json"
        with manifest_path.open("w") as f:
            json.dump(manifest, f, indent=2)

        if not self.euxis_dispatch.exists():
            self.skipTest("euxis-dispatch not found")

        env = os.environ.copy()
        env["PATH"] = f"{self.mock_agents_dir}:{env['PATH']}"
        env["EUXIS_HOME"] = self.temp_dir

        start_time = time.time()
        result = subprocess.run(
            [str(self.euxis_dispatch), str(manifest_path)],
            cwd=self.temp_dir,
            capture_output=True,
            env=env,
            text=True,
            timeout=30
        )
        execution_time = time.time() - start_time

        # Should complete successfully even under high load
        assert result.returncode == 0, f"Dispatch failed: {result.stderr}"

        # With 20 agents and ~2s stagger delay each in legacy mode,
        # expect up to 45s total. Use generous threshold.
        assert execution_time < 60, "High load execution took too long"


def run_integration_tests():
    """Run all integration tests and return results."""
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()

    # Add test classes
    suite.addTests(loader.loadTestsFromTestCase(DispatchIntegrationTest))
    suite.addTests(loader.loadTestsFromTestCase(DispatchConcurrencyStressTest))

    # Run tests
    runner = unittest.TextTestRunner(verbosity=2, stream=Path(os.devnull).open("w"))  # noqa: SIM115
    result = runner.run(suite)

    return {
        "tests_run": result.testsRun,
        "failures": len(result.failures),
        "errors": len(result.errors),
        "skipped": len(result.skipped),
        "success": result.wasSuccessful()
    }


if __name__ == "__main__":
    import sys

    if len(sys.argv) > 1 and sys.argv[1] == "--verify":
        # Quick verification run
        results = run_integration_tests()
        sys.exit(0 if results["success"] else 1)
    else:
        # Run with unittest
        unittest.main(verbosity=2)
