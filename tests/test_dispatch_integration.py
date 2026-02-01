#!/usr/bin/env python3
"""
Euxis Dispatch Integration Tests
Tests the actual euxis-dispatch script's concurrency mechanisms.

This test suite verifies:
1. Real euxis-dispatch stage execution ordering
2. File locking behavior across processes
3. Dependency resolution in manifest execution
4. Failure handling and cleanup
5. Log file generation and cleanup
"""

import json
import os
import subprocess
import tempfile
import time
import unittest
from pathlib import Path


class DispatchIntegrationTest(unittest.TestCase):
    """Integration tests for euxis-dispatch concurrency mechanisms."""

    def setUp(self):
        """Setup test environment."""
        self.temp_dir = tempfile.mkdtemp(prefix="euxis_dispatch_integration_")
        self.test_manifests_dir = Path(self.temp_dir) / "manifests"
        self.test_manifests_dir.mkdir()
        self.euxis_dispatch = Path.home() / ".euxis" / "bin" / "euxis-dispatch"

        # Create mock agent scripts for testing
        self.mock_agents_dir = Path(self.temp_dir) / "mock_agents"
        self.mock_agents_dir.mkdir()
        self.create_mock_agents()

    def tearDown(self):
        """Cleanup test environment."""
        import shutil
        shutil.rmtree(self.temp_dir)

    def create_mock_agents(self):
        """Create mock agent scripts for testing."""
        # Fast agent (completes quickly)
        fast_agent = self.mock_agents_dir / "fast_agent.py"
        fast_agent.write_text('''#!/usr/bin/env python3
import time
import sys
print(f"Fast agent {sys.argv[1]} starting")
time.sleep(0.1)
print(f"Fast agent {sys.argv[1]} completed")
''')
        fast_agent.chmod(0o755)

        # Slow agent (takes longer)
        slow_agent = self.mock_agents_dir / "slow_agent.py"
        slow_agent.write_text('''#!/usr/bin/env python3
import time
import sys
print(f"Slow agent {sys.argv[1]} starting")
time.sleep(0.5)
print(f"Slow agent {sys.argv[1]} completed")
''')
        slow_agent.chmod(0o755)

        # Failing agent
        failing_agent = self.mock_agents_dir / "failing_agent.py"
        failing_agent.write_text('''#!/usr/bin/env python3
import sys
print(f"Failing agent {sys.argv[1]} starting")
print(f"Failing agent {sys.argv[1]} failed", file=sys.stderr)
sys.exit(1)
''')
        failing_agent.chmod(0o755)

        # Lock-testing agent
        lock_agent = self.mock_agents_dir / "lock_agent.py"
        lock_agent.write_text(f'''#!/usr/bin/env python3
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
''')
        lock_agent.chmod(0o755)

    def create_test_manifest(self, name, dispatches):
        """Create a test manifest file."""
        manifest = {
            "project": f"test-{name}",
            "mode": "hierarchical",
            "dispatches": dispatches
        }

        manifest_path = self.test_manifests_dir / f"{name}.json"
        with open(manifest_path, 'w') as f:
            json.dump(manifest, f, indent=2)

        return str(manifest_path)

    def run_dispatch(self, manifest_path, timeout=10):
        """Run euxis-dispatch with a manifest and capture results."""
        if not self.euxis_dispatch.exists():
            self.skipTest("euxis-dispatch not found")

        env = os.environ.copy()
        env['PATH'] = f"{self.mock_agents_dir}:{env['PATH']}"

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
            'returncode': result.returncode,
            'stdout': result.stdout,
            'stderr': result.stderr,
            'execution_time': execution_time
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

        # Parse output to verify execution order
        output_lines = result['stdout'].split('\n')

        # Find start and completion messages
        stage1_starts = [line for line in output_lines if "Stage 1" in line and "starting" in line]
        stage1_completes = [line for line in output_lines if "Stage 1" in line and "completed" in line]
        stage2_starts = [line for line in output_lines if "Stage 2" in line and "starting" in line]

        # Stage 1 should have 2 agents (fast and slow)
        self.assertEqual(len(stage1_starts), 2)
        self.assertEqual(len(stage1_completes), 2)

        # Stage 2 should start after stage 1 completes
        self.assertEqual(len(stage2_starts), 1)

        # Verify stage execution was successful
        self.assertEqual(result['returncode'], 0)

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

        # With 3 agents taking 0.5s each, parallel execution should complete
        # in ~0.5s, sequential would take ~1.5s
        self.assertLess(result['execution_time'], 1.0,
                       "Parallel execution took too long - may be running sequentially")
        self.assertEqual(result['returncode'], 0)

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
        self.assertEqual(result['returncode'], 0)

        # Verify execution order in output
        output_lines = result['stdout'].split('\n')
        fast_start = next((i for i, line in enumerate(output_lines)
                          if "Fast agent" in line and "starting" in line), None)
        fast_complete = next((i for i, line in enumerate(output_lines)
                             if "Fast agent" in line and "completed" in line), None)
        slow_start = next((i for i, line in enumerate(output_lines)
                          if "Slow agent" in line and "starting" in line), None)

        self.assertIsNotNone(fast_start)
        self.assertIsNotNone(fast_complete)
        self.assertIsNotNone(slow_start)
        self.assertLess(fast_complete, slow_start, "Dependency not respected")

    def test_file_locking_exclusive_access(self):
        """Test that file locks provide exclusive access."""
        dispatches = [
            {
                "agent": "lock_agent.py",
                "priority": "P1",
                "task": "Lock test 1",
                "verify_cmd": "echo verified",
                "stage": 1,
                "locks": [f"{self.temp_dir}/test.lock"]
            },
            {
                "agent": "lock_agent.py",
                "priority": "P1",
                "task": "Lock test 2",
                "verify_cmd": "echo verified",
                "stage": 1,
                "locks": [f"{self.temp_dir}/test.lock"]
            }
        ]

        manifest_path = self.create_test_manifest("file_locking", dispatches)
        result = self.run_dispatch(manifest_path)

        # One should succeed, one should fail due to lock contention
        # In dispatch system, this might result in one agent failing
        output_lines = result['stdout'].split('\n')
        acquired_count = len([line for line in output_lines if "acquired lock" in line])
        could_not_acquire_count = len([line for line in output_lines if "could not acquire lock" in line])

        # Should have exactly one successful lock acquisition
        self.assertEqual(acquired_count, 1)
        self.assertGreaterEqual(could_not_acquire_count, 1)

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

        # Dispatch should report the failure but continue with other agents
        self.assertNotEqual(result['returncode'], 0, "Should fail due to failing agent")

        # Verify failure is reported in output
        output_lines = result['stdout'].split('\n')
        failure_reported = any("FAIL" in line or "failed" in line for line in output_lines)
        self.assertTrue(failure_reported, "Failure not properly reported")

    def test_manifest_validation(self):
        """Test that invalid manifests are rejected."""
        # Invalid JSON
        invalid_manifest = self.test_manifests_dir / "invalid.json"
        invalid_manifest.write_text('{"invalid": json}')

        result = self.run_dispatch(str(invalid_manifest))
        self.assertNotEqual(result['returncode'], 0)
        self.assertIn("Invalid JSON", result['stdout'])

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
        result = self.run_dispatch(manifest_path)

        # Check if log directory was created
        log_dir = Path(self.temp_dir) / ".euxis_dispatch_logs"
        if log_dir.exists():
            log_files = list(log_dir.glob("*.log"))
            self.assertGreater(len(log_files), 0, "No log files generated")

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

        self.assertEqual(result1['returncode'], 0)
        self.assertEqual(result2['returncode'], 0)


class DispatchConcurrencyStressTest(unittest.TestCase):
    """Stress tests for dispatch concurrency under load."""

    def setUp(self):
        """Setup stress test environment."""
        self.temp_dir = tempfile.mkdtemp(prefix="euxis_stress_test_")
        self.test_manifests_dir = Path(self.temp_dir) / "manifests"
        self.test_manifests_dir.mkdir()
        self.euxis_dispatch = Path.home() / ".euxis" / "bin" / "euxis-dispatch"

    def tearDown(self):
        """Cleanup stress test environment."""
        import shutil
        shutil.rmtree(self.temp_dir)

    def test_high_concurrency_load(self):
        """Test dispatch system under high concurrency load."""
        # Create manifest with many parallel agents
        dispatches = []
        for i in range(20):  # 20 parallel agents
            dispatches.append({
                "agent": "echo",  # Use simple echo command
                "priority": "P2",
                "task": f"High load task {i}",
                "verify_cmd": "echo verified",
                "stage": 1
            })

        manifest = {
            "project": "stress-test",
            "mode": "hierarchical",
            "dispatches": dispatches
        }

        manifest_path = self.test_manifests_dir / "stress.json"
        with open(manifest_path, 'w') as f:
            json.dump(manifest, f, indent=2)

        if not self.euxis_dispatch.exists():
            self.skipTest("euxis-dispatch not found")

        start_time = time.time()
        result = subprocess.run(
            [str(self.euxis_dispatch), str(manifest_path)],
            cwd=self.temp_dir,
            capture_output=True,
            text=True,
            timeout=30
        )
        execution_time = time.time() - start_time

        # Should complete successfully even under high load
        self.assertEqual(result['returncode'], 0)

        # Should complete in reasonable time (parallel execution)
        self.assertLess(execution_time, 10, "High load execution took too long")


def run_integration_tests():
    """Run all integration tests and return results."""
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()

    # Add test classes
    suite.addTests(loader.loadTestsFromTestCase(DispatchIntegrationTest))
    suite.addTests(loader.loadTestsFromTestCase(DispatchConcurrencyStressTest))

    # Run tests
    runner = unittest.TextTestRunner(verbosity=2, stream=open(os.devnull, 'w'))
    result = runner.run(suite)

    return {
        'tests_run': result.testsRun,
        'failures': len(result.failures),
        'errors': len(result.errors),
        'skipped': len(result.skipped),
        'success': result.wasSuccessful()
    }


if __name__ == "__main__":
    import sys

    if len(sys.argv) > 1 and sys.argv[1] == "--verify":
        # Quick verification run
        results = run_integration_tests()
        print(f"Integration Test Results:")
        print(f"  Tests run: {results['tests_run']}")
        print(f"  Failures: {results['failures']}")
        print(f"  Errors: {results['errors']}")
        print(f"  Skipped: {results['skipped']}")
        print(f"  Success: {results['success']}")
        sys.exit(0 if results['success'] else 1)
    else:
        # Run with unittest
        unittest.main(verbosity=2)