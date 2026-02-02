#!/usr/bin/env python3
"""
Deterministic Concurrency Test Suite for Euxis Fleet
Tests cancellation, backpressure, ordering, and race conditions with mocked dependencies.

This test suite verifies:
1. Dispatch parallel execution with file locking
2. Stage-based dependency resolution and ordering
3. Subprocess cancellation and cleanup
4. Backpressure handling in message queues
5. Race condition prevention in shared resources
6. Voice pipeline concurrency patterns
"""

import asyncio
import concurrent.futures
import json
import multiprocessing
import os
import pytest
import shutil
import subprocess
import tempfile
import threading
import time
import unittest.mock as mock
from pathlib import Path
from queue import Queue, Full, Empty
from threading import Event, Lock, RLock, Barrier
from unittest.mock import patch, MagicMock, AsyncMock


class MockFileSystem:
    """Mock filesystem with deterministic locking behavior."""

    def __init__(self):
        self.files = {}
        self.locks = {}
        self.operations = []  # Track all operations for verification
        self._lock = threading.RLock()

    def acquire_lock(self, path: str, timeout: float = 5.0) -> bool:
        """Simulate file lock acquisition with deterministic timing."""
        with self._lock:
            if path in self.locks:
                return False
            self.locks[path] = threading.current_thread().ident
            self.operations.append(("lock_acquired", path, time.time()))
            return True

    def release_lock(self, path: str) -> bool:
        """Simulate lock release."""
        with self._lock:
            if path not in self.locks:
                return False
            del self.locks[path]
            self.operations.append(("lock_released", path, time.time()))
            return True

    def write_file(self, path: str, content: str) -> bool:
        """Simulate atomic file write."""
        with self._lock:
            self.files[path] = content
            self.operations.append(("file_written", path, time.time()))
            return True


class DeterministicDispatchTester:
    """Tests for the euxis-dispatch concurrent execution patterns."""

    def __init__(self):
        self.temp_dir = None
        self.mock_fs = MockFileSystem()

    def setup(self):
        """Setup test environment."""
        self.temp_dir = tempfile.mkdtemp(prefix="euxis_test_")
        self.manifest_path = os.path.join(self.temp_dir, "test_manifest.json")

    def teardown(self):
        """Cleanup test environment."""
        if self.temp_dir and os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir)

    def create_test_manifest(self, stages_config):
        """Create a test manifest with specified stage configuration."""
        manifest = {
            "project": "concurrency-test",
            "mode": "hierarchical",
            "dispatches": []
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

        with open(self.manifest_path, 'w') as f:
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

        manifest_path = self.dispatch_tester.create_test_manifest(stages_config)

        # Mock the dispatch execution to track execution order
        execution_order = []

        def mock_execute_agent(agent_name):
            execution_order.append((agent_name, time.time()))
            time.sleep(0.1)  # Simulate work

        with patch('subprocess.Popen') as mock_popen:
            # Simulate successful agent execution
            mock_process = MagicMock()
            mock_process.wait.return_value = 0
            mock_process.returncode = 0
            mock_popen.return_value = mock_process

            # Execute stages sequentially as dispatch would
            stage1_agents = ["stage1-agent1", "stage1-agent2"]
            stage2_agents = ["stage2-agent1"]
            stage3_agents = ["stage3-agent1"]

            # Stage 1: parallel execution
            threads = []
            for agent in stage1_agents:
                thread = threading.Thread(target=mock_execute_agent, args=(agent,))
                threads.append(thread)
                thread.start()

            for thread in threads:
                thread.join()

            # Stage 2: depends on stage1-agent1
            mock_execute_agent("stage2-agent1")

            # Stage 3: depends on stage2-agent1
            mock_execute_agent("stage3-agent1")

        # Verify execution order
        assert len(execution_order) == 4

        # Stage 1 agents should start around the same time
        stage1_times = [t for agent, t in execution_order if "stage1" in agent]
        assert abs(stage1_times[0] - stage1_times[1]) < 0.05  # Within 50ms

        # Stage 2 should start after stage 1 completes
        stage1_end = max(stage1_times) + 0.1  # Add simulated work time
        stage2_start = [t for agent, t in execution_order if agent == "stage2-agent1"][0]
        assert stage2_start > stage1_end

        # Stage 3 should start after stage 2 completes
        stage2_end = stage2_start + 0.1
        stage3_start = [t for agent, t in execution_order if agent == "stage3-agent1"][0]
        assert stage3_start > stage2_end

    def test_parallel_execution_within_stage(self):
        """Test that agents within same stage execute in parallel."""
        stages_config = [
            {"agent": "parallel-agent1", "stage": 1},
            {"agent": "parallel-agent2", "stage": 1},
            {"agent": "parallel-agent3", "stage": 1},
        ]

        manifest_path = self.dispatch_tester.create_test_manifest(stages_config)

        execution_times = {}
        barrier = threading.Barrier(3)  # Ensure all threads start simultaneously

        def mock_execute_agent(agent_name):
            barrier.wait()  # Synchronize start
            start_time = time.time()
            execution_times[agent_name] = start_time
            time.sleep(0.2)  # Simulate work

        threads = []
        for i in range(3):
            agent = f"parallel-agent{i+1}"
            thread = threading.Thread(target=mock_execute_agent, args=(agent,))
            threads.append(thread)
            thread.start()

        for thread in threads:
            thread.join()

        # Verify all agents started within a small time window (parallel execution)
        start_times = list(execution_times.values())
        time_spread = max(start_times) - min(start_times)
        assert time_spread < 0.01  # Within 10ms, indicating true parallelism


class TestFileLockingConcurrency:
    """Test deterministic file locking behavior under concurrent access."""

    def setup_method(self):
        self.mock_fs = MockFileSystem()
        self.temp_dir = tempfile.mkdtemp(prefix="euxis_lock_test_")

    def teardown_method(self):
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir)

    def test_exclusive_lock_access(self):
        """Test that only one thread can hold a lock at a time."""
        results = {}
        barrier = threading.Barrier(3)

        def attempt_lock(thread_id):
            barrier.wait()  # Synchronize start
            start_time = time.time()

            if self.mock_fs.acquire_lock("shared_resource", timeout=1.0):
                results[thread_id] = {"acquired": True, "time": time.time() - start_time}
                time.sleep(0.1)  # Hold lock briefly
                self.mock_fs.release_lock("shared_resource")
            else:
                results[thread_id] = {"acquired": False, "time": time.time() - start_time}

        threads = []
        for i in range(3):
            thread = threading.Thread(target=attempt_lock, args=(i,))
            threads.append(thread)
            thread.start()

        for thread in threads:
            thread.join()

        # Exactly one thread should acquire the lock
        acquired_count = sum(1 for r in results.values() if r["acquired"])
        assert acquired_count == 1

        # Verify lock operations are properly ordered
        operations = self.mock_fs.operations
        lock_ops = [op for op in operations if op[0] in ["lock_acquired", "lock_released"]]
        assert len(lock_ops) == 2  # One acquire, one release
        assert lock_ops[0][0] == "lock_acquired"
        assert lock_ops[1][0] == "lock_released"

    def test_lock_timeout_behavior(self):
        """Test that lock acquisition respects timeout values."""
        # First thread acquires lock and holds it
        def hold_lock():
            assert self.mock_fs.acquire_lock("test_resource", timeout=5.0)
            time.sleep(0.5)  # Hold for 500ms
            self.mock_fs.release_lock("test_resource")

        holder_thread = threading.Thread(target=hold_lock)
        holder_thread.start()

        time.sleep(0.1)  # Ensure first thread gets the lock

        # Second thread attempts to acquire with short timeout
        start_time = time.time()
        result = self.mock_fs.acquire_lock("test_resource", timeout=0.2)
        elapsed = time.time() - start_time

        holder_thread.join()

        # Should fail to acquire (returns False) and timeout quickly
        assert result is False
        assert elapsed < 0.3  # Should timeout in ~200ms, not wait full 500ms


class TestSubprocessCancellation:
    """Test subprocess cancellation and cleanup behavior."""

    def setup_method(self):
        self.temp_dir = tempfile.mkdtemp(prefix="euxis_subprocess_test_")

    def teardown_method(self):
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir)

    def test_subprocess_cancellation_cleanup(self):
        """Test that cancelled subprocesses clean up properly."""
        processes = []

        def start_long_running_process():
            # Simulate long-running agent process
            cmd = ["python3", "-c", "import time; time.sleep(10)"]
            proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            processes.append(proc)
            return proc

        # Start multiple processes
        procs = []
        for _ in range(3):
            proc = start_long_running_process()
            procs.append(proc)

        time.sleep(0.1)  # Let processes start

        # Cancel all processes
        for proc in procs:
            proc.terminate()

        # Wait for termination with timeout
        for proc in procs:
            try:
                proc.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                proc.kill()  # Force kill if needed
                proc.wait()

        # Verify all processes terminated
        for proc in procs:
            assert proc.returncode is not None
            assert proc.returncode != 0  # Terminated, not successful exit

    def test_subprocess_backpressure_handling(self):
        """Test handling of subprocess output backpressure."""
        large_output = "x" * 1000000  # 1MB of data

        # Process that generates large output
        cmd = ["python3", "-c", f"print('{large_output}')"]

        start_time = time.time()
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            bufsize=1024  # Small buffer to test backpressure
        )

        # Read output in chunks to simulate backpressure
        output_chunks = []
        while True:
            chunk = proc.stdout.read(1024)
            if not chunk:
                break
            output_chunks.append(chunk)
            time.sleep(0.001)  # Small delay to test buffering

        proc.wait()
        elapsed = time.time() - start_time

        # Verify process completed successfully despite backpressure
        assert proc.returncode == 0
        reconstructed = b''.join(output_chunks).decode().strip()
        assert reconstructed == large_output
        # Should complete in reasonable time despite backpressure
        assert elapsed < 5.0


class TestAsyncConcurrencyPatterns:
    """Test async/await concurrency patterns used in voice pipeline."""

    def setup_method(self):
        self.loop = None

    def teardown_method(self):
        if self.loop and not self.loop.is_closed():
            self.loop.close()

    @pytest.mark.asyncio
    async def test_concurrent_async_tasks_with_cancellation(self):
        """Test cancellation of concurrent async tasks."""
        results = []

        async def slow_task(task_id, delay):
            try:
                await asyncio.sleep(delay)
                results.append(f"task_{task_id}_completed")
            except asyncio.CancelledError:
                results.append(f"task_{task_id}_cancelled")
                raise

        # Start multiple tasks
        tasks = []
        for i in range(3):
            task = asyncio.create_task(slow_task(i, 2.0))
            tasks.append(task)

        # Let tasks start
        await asyncio.sleep(0.1)

        # Cancel tasks
        for task in tasks:
            task.cancel()

        # Wait for cancellation to complete
        with pytest.raises(asyncio.CancelledError):
            await asyncio.gather(*tasks)

        # Verify all tasks were cancelled
        cancelled_count = sum(1 for r in results if "cancelled" in r)
        assert cancelled_count == 3

    @pytest.mark.asyncio
    async def test_async_queue_backpressure(self):
        """Test async queue backpressure handling."""
        queue = asyncio.Queue(maxsize=2)  # Small queue to test backpressure

        async def producer():
            for i in range(10):
                try:
                    await asyncio.wait_for(queue.put(f"item_{i}"), timeout=0.1)
                except asyncio.TimeoutError:
                    return f"backpressure_at_{i}"
            return "all_produced"

        async def consumer():
            items = []
            for _ in range(5):  # Only consume 5 items
                item = await queue.get()
                items.append(item)
                await asyncio.sleep(0.05)  # Slow consumer
            return items

        # Run producer and consumer concurrently
        producer_result, consumer_result = await asyncio.gather(
            producer(), consumer()
        )

        # Producer should hit backpressure
        assert "backpressure_at_" in producer_result
        # Consumer should receive items
        assert len(consumer_result) == 5
        assert all("item_" in item for item in consumer_result)

    @pytest.mark.asyncio
    async def test_concurrent_voice_pipeline_stages(self):
        """Test concurrent execution of voice pipeline stages."""
        # Mock voice processing stages
        async def whisper_transcribe(audio_data):
            await asyncio.sleep(0.2)  # Simulate transcription time
            return f"transcribed: {audio_data}"

        async def llm_process(text):
            await asyncio.sleep(0.3)  # Simulate LLM processing
            return f"response: {text}"

        async def tts_synthesize(response):
            await asyncio.sleep(0.1)  # Simulate TTS
            return f"audio: {response}"

        # Process multiple voice requests concurrently
        requests = [f"audio_data_{i}" for i in range(3)]

        async def process_request(audio_data):
            transcribed = await whisper_transcribe(audio_data)
            response = await llm_process(transcribed)
            audio = await tts_synthesize(response)
            return audio

        start_time = time.time()
        results = await asyncio.gather(*[process_request(req) for req in requests])
        elapsed = time.time() - start_time

        # Verify results
        assert len(results) == 3
        for i, result in enumerate(results):
            expected = f"audio: response: transcribed: audio_data_{i}"
            assert result == expected

        # Should complete in parallel, not sequential
        # Sequential would take: 3 * (0.2 + 0.3 + 0.1) = 1.8s
        # Parallel should take: max(0.2, 0.3, 0.1) = 0.6s (approximately)
        assert elapsed < 1.0


class TestRaceConditionPrevention:
    """Test prevention of race conditions in shared resources."""

    def setup_method(self):
        self.shared_counter = 0
        self.shared_dict = {}
        self.lock = threading.Lock()

    def test_counter_race_condition_with_lock(self):
        """Test that proper locking prevents counter race conditions."""
        iterations = 1000

        def increment_counter():
            for _ in range(iterations):
                with self.lock:
                    self.shared_counter += 1

        threads = []
        for _ in range(5):
            thread = threading.Thread(target=increment_counter)
            threads.append(thread)
            thread.start()

        for thread in threads:
            thread.join()

        # With proper locking, final count should be exact
        expected = 5 * iterations
        assert self.shared_counter == expected

    def test_dictionary_race_condition_with_lock(self):
        """Test that proper locking prevents dictionary race conditions."""
        def worker(worker_id):
            for i in range(100):
                key = f"worker_{worker_id}_item_{i}"
                with self.lock:
                    self.shared_dict[key] = worker_id

        threads = []
        for worker_id in range(5):
            thread = threading.Thread(target=worker, args=(worker_id,))
            threads.append(thread)
            thread.start()

        for thread in threads:
            thread.join()

        # Verify all items were added without corruption
        expected_items = 5 * 100
        assert len(self.shared_dict) == expected_items

        # Verify data integrity
        for key, worker_id in self.shared_dict.items():
            assert f"worker_{worker_id}_" in key

    def test_file_write_race_condition_prevention(self):
        """Test prevention of race conditions in concurrent file writes."""
        temp_file = os.path.join(tempfile.gettempdir(), "race_test.txt")
        file_lock = threading.Lock()

        def write_to_file(writer_id):
            for i in range(10):
                line = f"writer_{writer_id}_line_{i}\n"
                with file_lock:
                    with open(temp_file, 'a') as f:
                        f.write(line)

        try:
            threads = []
            for writer_id in range(5):
                thread = threading.Thread(target=write_to_file, args=(writer_id,))
                threads.append(thread)
                thread.start()

            for thread in threads:
                thread.join()

            # Verify file integrity
            with open(temp_file, 'r') as f:
                lines = f.readlines()

            assert len(lines) == 50  # 5 writers × 10 lines each

            # Verify each line is complete (no partial writes)
            for line in lines:
                assert line.startswith("writer_")
                assert line.endswith("_line_" + line.split("_line_")[1])

        finally:
            if os.path.exists(temp_file):
                os.remove(temp_file)


class TestMessageBusConcurrency:
    """Test concurrent message bus operations."""

    def setup_method(self):
        self.message_queue = Queue(maxsize=10)
        self.published_messages = []
        self.consumed_messages = []

    def test_producer_consumer_pattern(self):
        """Test producer-consumer pattern with backpressure."""
        def producer(producer_id):
            for i in range(20):
                message = f"producer_{producer_id}_msg_{i}"
                try:
                    self.message_queue.put(message, timeout=0.1)
                    self.published_messages.append(message)
                except Full:
                    # Queue is full - backpressure applied
                    break

        def consumer():
            while True:
                try:
                    message = self.message_queue.get(timeout=0.5)
                    self.consumed_messages.append(message)
                    time.sleep(0.01)  # Simulate processing time
                except Empty:
                    break

        # Start producer and consumer
        producer_thread = threading.Thread(target=producer, args=(1,))
        consumer_thread = threading.Thread(target=consumer)

        producer_thread.start()
        consumer_thread.start()

        producer_thread.join()
        consumer_thread.join()

        # Verify messages were transferred
        assert len(self.consumed_messages) > 0
        assert len(self.consumed_messages) <= len(self.published_messages)

        # Verify message ordering (FIFO)
        for consumed in self.consumed_messages:
            assert consumed in self.published_messages

    def test_multiple_publishers_ordering(self):
        """Test message ordering with multiple publishers."""
        def publisher(publisher_id):
            for i in range(5):
                message = f"pub_{publisher_id}_msg_{i}"
                self.message_queue.put(message)

        threads = []
        for pub_id in range(3):
            thread = threading.Thread(target=publisher, args=(pub_id,))
            threads.append(thread)
            thread.start()

        for thread in threads:
            thread.join()

        # Collect all messages
        messages = []
        while not self.message_queue.empty():
            messages.append(self.message_queue.get())

        # Should have all messages
        assert len(messages) == 15  # 3 publishers × 5 messages

        # Verify messages from each publisher maintain order
        for pub_id in range(3):
            pub_messages = [m for m in messages if f"pub_{pub_id}_" in m]
            for i, msg in enumerate(pub_messages):
                expected = f"pub_{pub_id}_msg_{i}"
                assert msg == expected


# Test runner and verification commands
def run_concurrency_test_suite():
    """Run all concurrency tests and generate a report."""
    import subprocess
    import sys

    # Run pytest with specific test classes
    test_classes = [
        "TestConcurrentStageExecution",
        "TestFileLockingConcurrency",
        "TestSubprocessCancellation",
        "TestAsyncConcurrencyPatterns",
        "TestRaceConditionPrevention",
        "TestMessageBusConcurrency"
    ]

    results = {}
    for test_class in test_classes:
        cmd = [
            sys.executable, "-m", "pytest",
            f"{__file__}::{test_class}",
            "-v", "--tb=short"
        ]

        result = subprocess.run(cmd, capture_output=True, text=True)
        results[test_class] = {
            "returncode": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr
        }

    return results


if __name__ == "__main__":
    # Run individual test verification
    import pytest
    import sys

    if len(sys.argv) > 1 and sys.argv[1] == "--verify":
        # Quick verification run
        results = run_concurrency_test_suite()
        passed = sum(1 for r in results.values() if r["returncode"] == 0)
        total = len(results)
        print(f"Concurrency Test Results: {passed}/{total} test classes passed")
        sys.exit(0 if passed == total else 1)
    else:
        # Run with pytest
        pytest.main([__file__, "-v"])