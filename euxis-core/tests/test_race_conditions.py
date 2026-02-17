# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Tests for prevention of race conditions in shared resources."""

import tempfile
import threading
from pathlib import Path


class TestRaceConditionPrevention:
    """Test prevention of race conditions in shared resources."""

    def setup_method(self):
        self.shared_counter = 0
        self.shared_dict = {}
        self.lock = threading.Lock()

    def test_counter_race_condition_with_lock(self):
        """Test that proper locking prevents counter race conditions."""
        iterations = 1000

        def increment_counter() -> None:
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

        expected = 5 * iterations
        assert self.shared_counter == expected

    def test_dictionary_race_condition_with_lock(self):
        """Test that proper locking prevents dictionary race conditions."""

        def worker(worker_id) -> None:
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

        expected_items = 5 * 100
        assert len(self.shared_dict) == expected_items

        for key, worker_id in self.shared_dict.items():
            assert f"worker_{worker_id}_" in key

    def test_file_write_race_condition_prevention(self):
        """Test prevention of race conditions in concurrent file writes."""
        temp_file = Path(tempfile.gettempdir()) / "race_test.txt"
        file_lock = threading.Lock()

        def write_to_file(writer_id) -> None:
            for i in range(10):
                line = f"writer_{writer_id}_line_{i}\n"
                with file_lock, temp_file.open("a") as f:
                    f.write(line)

        try:
            threads = []
            for writer_id in range(5):
                thread = threading.Thread(target=write_to_file, args=(writer_id,))
                threads.append(thread)
                thread.start()

            for thread in threads:
                thread.join()

            with temp_file.open() as f:
                lines = f.readlines()

            assert len(lines) == 50

            for line in lines:
                assert line.startswith("writer_")
                assert line.endswith("_line_" + line.split("_line_")[1])

        finally:
            if temp_file.exists():
                temp_file.unlink()
