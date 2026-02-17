# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Tests for subprocess cancellation and cleanup behavior."""

import shutil
import subprocess
import tempfile
import time
from pathlib import Path


class TestSubprocessCancellation:
    """Test subprocess cancellation and cleanup behavior."""

    def setup_method(self):
        self.temp_dir = tempfile.mkdtemp(prefix="euxis_subprocess_test_")

    def teardown_method(self):
        if Path(self.temp_dir).exists():
            shutil.rmtree(self.temp_dir)

    def test_subprocess_cancellation_cleanup(self):
        """Test that cancelled subprocesses clean up properly."""
        processes = []

        def start_long_running_process():
            cmd = ["python3", "-c", "import time; time.sleep(10)"]
            proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            processes.append(proc)
            return proc

        procs = []
        for _ in range(3):
            proc = start_long_running_process()
            procs.append(proc)

        time.sleep(0.1)

        for proc in procs:
            proc.terminate()

        for proc in procs:
            try:
                proc.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()

        for proc in procs:
            assert proc.returncode is not None
            assert proc.returncode != 0

    def test_subprocess_backpressure_handling(self):
        """Test handling of subprocess output backpressure."""
        large_output = "x" * 1000000

        script = Path(self.temp_dir) / "gen_output.py"
        with script.open("w") as f:
            f.write(f"print('{'x' * 1000000}')\n")

        cmd = ["python3", script]

        start_time = time.time()
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            bufsize=1024,
        )

        output_chunks = []
        while True:
            chunk = proc.stdout.read(1024)
            if not chunk:
                break
            output_chunks.append(chunk)
            time.sleep(0.001)

        proc.wait()
        elapsed = time.time() - start_time

        assert proc.returncode == 0
        reconstructed = b"".join(output_chunks).decode().strip()
        assert reconstructed == large_output
        assert elapsed < 5.0
