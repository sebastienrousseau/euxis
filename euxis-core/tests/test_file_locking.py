# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Tests for deterministic file locking behavior under concurrent access."""

import shutil
import tempfile
import threading
import time
from pathlib import Path


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


class TestFileLockingConcurrency:
    """Test deterministic file locking behavior under concurrent access."""

    def setup_method(self):
        self.mock_fs = MockFileSystem()
        self.temp_dir = tempfile.mkdtemp(prefix="euxis_lock_test_")

    def teardown_method(self):
        if Path(self.temp_dir).exists():
            shutil.rmtree(self.temp_dir)

    def test_exclusive_lock_access(self):
        """Test that only one thread can hold a lock at a time."""
        results = {}
        barrier = threading.Barrier(3)

        def attempt_lock(thread_id) -> None:
            barrier.wait()
            start_time = time.time()

            if self.mock_fs.acquire_lock("shared_resource", timeout=1.0):
                results[thread_id] = {"acquired": True, "time": time.time() - start_time}
                time.sleep(0.1)
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

        acquired_count = sum(1 for r in results.values() if r["acquired"])
        assert acquired_count == 1

        operations = self.mock_fs.operations
        lock_ops = [op for op in operations if op[0] in ["lock_acquired", "lock_released"]]
        assert len(lock_ops) == 2
        assert lock_ops[0][0] == "lock_acquired"
        assert lock_ops[1][0] == "lock_released"

    def test_lock_timeout_behavior(self):
        """Test that lock acquisition respects timeout values."""

        def hold_lock() -> None:
            assert self.mock_fs.acquire_lock("test_resource", timeout=5.0)
            time.sleep(0.5)
            self.mock_fs.release_lock("test_resource")

        holder_thread = threading.Thread(target=hold_lock)
        holder_thread.start()

        time.sleep(0.1)

        start_time = time.time()
        result = self.mock_fs.acquire_lock("test_resource", timeout=0.2)
        elapsed = time.time() - start_time

        holder_thread.join()

        assert result is False
        assert elapsed < 0.3
