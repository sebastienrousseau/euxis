# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

#!/usr/bin/env python3
"""Concurrency Test Verification Suite.

Run all deterministic concurrency tests and validate results.

This script:
1. Executes all concurrency test modules
2. Verifies deterministic behavior across multiple runs
3. Checks for race conditions and flaky tests
4. Generates comprehensive test report
"""

import contextlib
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any


class ConcurrencyTestVerifier:
    """Verifies deterministic behavior of concurrency tests."""

    def __init__(self) -> None:
        self.repo_root = Path(__file__).resolve().parents[2]
        self.tests_dir = self.repo_root
        self.euxis_home = Path.home() / ".euxis"
        self.results = {}
        self.test_modules = [
            self.tests_dir / "cli" / "tests" / "test_stage_execution.py",
            self.tests_dir / "core" / "tests" / "test_file_locking.py",
            self.tests_dir / "core" / "tests" / "test_subprocess_cancel.py",
            self.tests_dir / "core" / "tests" / "test_async_patterns.py",
            self.tests_dir / "core" / "tests" / "test_race_conditions.py",
            self.tests_dir / "core" / "tests" / "test_message_bus.py",
            self.tests_dir / "cli" / "tests" / "test_dispatch_integration.py",
            self.tests_dir / "cli" / "tests" / "test_dispatch_concurrency.sh",
        ]

    def run_python_test_module(self, module_path: Path, runs: int = 3) -> dict[str, Any]:
        """Run a Python test module multiple times to verify determinism."""
        results = {
            "module": module_path.name,
            "runs": [],
            "deterministic": True,
            "total_time": 0,
            "average_time": 0
        }


        for run_num in range(runs):

            start_time = time.time()

            # Run with pytest for Python modules
            cmd = [
                sys.executable, "-m", "pytest",
                str(module_path),
                "--tb=short",
                "-v",
                "--durations=10"
            ]

            try:
                result = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True,
                    timeout=120,  # 2 minute timeout
                    cwd=str(self.tests_dir)
                )

                execution_time = time.time() - start_time
                results["total_time"] += execution_time

                run_result = {
                    "run": run_num + 1,
                    "returncode": result.returncode,
                    "execution_time": execution_time,
                    "stdout_lines": len(result.stdout.split("\n")),
                    "stderr_lines": len(result.stderr.split("\n")),
                    "tests_passed": result.stdout.count(" PASSED"),
                    "tests_failed": result.stdout.count(" FAILED"),
                    "tests_skipped": result.stdout.count(" SKIPPED")
                }

                results["runs"].append(run_result)

            except subprocess.TimeoutExpired:
                results["runs"].append({
                    "run": run_num + 1,
                    "returncode": -1,
                    "execution_time": 120,
                    "error": "timeout"
                })
                results["deterministic"] = False

            except (OSError, subprocess.SubprocessError) as e:
                results["runs"].append({
                    "run": run_num + 1,
                    "returncode": -1,
                    "execution_time": 0,
                    "error": str(e)
                })
                results["deterministic"] = False

        # Calculate average and check determinism
        if results["runs"]:
            results["average_time"] = results["total_time"] / len(results["runs"])

            # Check if results are consistent (deterministic)
            return_codes = [r.get("returncode", -1) for r in results["runs"]]
            tests_passed = [r.get("tests_passed", 0) for r in results["runs"]]

            if len(set(return_codes)) > 1 or len(set(tests_passed)) > 1:
                results["deterministic"] = False

        return results

    def run_bash_test_script(self, script_path: Path, runs: int = 3) -> dict[str, Any]:
        """Run a bash test script multiple times to verify determinism."""
        results = {
            "module": script_path.name,
            "runs": [],
            "deterministic": True,
            "total_time": 0,
            "average_time": 0
        }


        for run_num in range(runs):

            start_time = time.time()

            try:
                result = subprocess.run(
                    [str(script_path)],
                    capture_output=True,
                    text=True,
                    timeout=60,
                    cwd=str(self.tests_dir)
                )

                execution_time = time.time() - start_time
                results["total_time"] += execution_time

                # Parse bash test output
                tests_passed = result.stdout.count("✓")
                tests_failed = result.stdout.count("✗")

                run_result = {
                    "run": run_num + 1,
                    "returncode": result.returncode,
                    "execution_time": execution_time,
                    "tests_passed": tests_passed,
                    "tests_failed": tests_failed,
                    "output_lines": len(result.stdout.split("\n"))
                }

                results["runs"].append(run_result)

            except subprocess.TimeoutExpired:
                results["runs"].append({
                    "run": run_num + 1,
                    "returncode": -1,
                    "execution_time": 60,
                    "error": "timeout"
                })
                results["deterministic"] = False

            except (OSError, subprocess.SubprocessError) as e:
                results["runs"].append({
                    "run": run_num + 1,
                    "returncode": -1,
                    "execution_time": 0,
                    "error": str(e)
                })
                results["deterministic"] = False

        # Calculate averages and check determinism
        if results["runs"]:
            results["average_time"] = results["total_time"] / len(results["runs"])

            return_codes = [r.get("returncode", -1) for r in results["runs"]]
            tests_passed = [r.get("tests_passed", 0) for r in results["runs"]]

            if len(set(return_codes)) > 1 or len(set(tests_passed)) > 1:
                results["deterministic"] = False

        return results

    def check_test_dependencies(self) -> dict[str, bool]:
        """Check if required dependencies for tests are available."""
        dependencies = {}

        # Check Python dependencies
        python_deps = ["pytest", "asyncio"]
        for dep in python_deps:
            try:
                __import__(dep)
                dependencies[f"python-{dep}"] = True
            except ImportError:
                dependencies[f"python-{dep}"] = False

        # Check system dependencies
        system_deps = ["bash", "jq", "timeout"]
        for dep in system_deps:
            result = subprocess.run(["which", dep], capture_output=True)
            dependencies[f"system-{dep}"] = result.returncode == 0

        # Check euxis-dispatch availability
        euxis_dispatch = self.euxis_home / "bin" / "euxis-dispatch"
        dependencies["euxis-dispatch"] = (
            euxis_dispatch.exists() and os.access(euxis_dispatch, os.X_OK)
        )

        return dependencies

    def run_stress_test(self) -> dict[str, Any]:
        """Run stress tests to detect race conditions under load."""
        stress_results = {
            "concurrent_pytest_runs": None,
            "resource_exhaustion": None,
            "timing_variance": None
        }

        # Test 1: Run multiple pytest processes concurrently
        try:
            concurrent_procs = []
            start_time = time.time()

            for _i in range(3):  # 3 concurrent pytest processes
                cmd = [
                    sys.executable, "-m", "pytest",
                    str(self.tests_dir / "core" / "tests" / "test_race_conditions.py::TestRaceConditionPrevention"),
                    "-q"
                ]
                proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                concurrent_procs.append(proc)

            # Wait for all to complete
            return_codes = []
            for proc in concurrent_procs:
                proc.wait()
                return_codes.append(proc.returncode)

            execution_time = time.time() - start_time

            stress_results["concurrent_pytest_runs"] = {
                "return_codes": return_codes,
                "all_passed": all(rc == 0 for rc in return_codes),
                "execution_time": execution_time
            }

        except (OSError, subprocess.SubprocessError) as e:
            stress_results["concurrent_pytest_runs"] = {"error": str(e)}

        # Test 2: Resource exhaustion test
        try:
            # Test with many temporary files and processes
            temp_files = []
            for _i in range(100):
                with tempfile.NamedTemporaryFile(delete=False) as temp_file:
                    temp_files.append(temp_file.name)

            # Clean up
            for temp_file in temp_files:
                with contextlib.suppress(OSError):
                    Path(temp_file).unlink()

            stress_results["resource_exhaustion"] = {"status": "passed"}

        except OSError as e:
            stress_results["resource_exhaustion"] = {"error": str(e)}

        # Test 3: Timing variance analysis
        try:
            execution_times = []
            for _i in range(10):
                start = time.time()
                # Quick deterministic test
                subprocess.run([
                    sys.executable, "-c",
                    "import threading; import time; "
                    "def task(): time.sleep(0.01); "
                    "threads = [threading.Thread(target=task) for _ in range(5)]; "
                    "[t.start() for t in threads]; [t.join() for t in threads]"
                ], capture_output=True)
                execution_times.append(time.time() - start)

            if execution_times:
                avg_time = sum(execution_times) / len(execution_times)
                variance = sum((t - avg_time) ** 2 for t in execution_times) / len(execution_times)
                std_dev = variance ** 0.5

                stress_results["timing_variance"] = {
                    "average_time": avg_time,
                    "standard_deviation": std_dev,
                    "coefficient_variation": std_dev / avg_time if avg_time > 0 else 0,
                    "stable": std_dev / avg_time < 0.1  # Less than 10% variation
                }

        except (OSError, subprocess.SubprocessError) as e:
            stress_results["timing_variance"] = {"error": str(e)}

        return stress_results

    def generate_report(self) -> str:
        """Generate comprehensive test report."""
        report = ["# Euxis Concurrency Test Verification Report", ""]
        report.append(f"Generated: {time.strftime('%Y-%m-%d %H:%M:%S')}")
        report.append("")

        # Dependencies check
        dependencies = self.check_test_dependencies()
        report.append("## Dependencies Status")
        for dep, status in dependencies.items():
            status_str = "✓" if status else "✗"
            report.append(f"- {dep}: {status_str}")
        report.append("")

        # Test results summary
        report.append("## Test Results Summary")
        total_modules = len(self.results)
        deterministic_modules = sum(
            1 for r in self.results.values() if r.get("deterministic", False)
        )

        report.append(f"- **Total test modules**: {total_modules}")
        report.append(f"- **Deterministic modules**: {deterministic_modules}")
        report.append(f"- **Non-deterministic modules**: {total_modules - deterministic_modules}")
        report.append("")

        # Individual module results
        report.append("## Individual Module Results")
        for module_name, result in self.results.items():
            report.append(f"### {module_name}")

            if result.get("runs"):
                avg_time = result.get("average_time", 0)
                deterministic = result.get("deterministic", False)
                runs_count = len(result["runs"])

                report.append(f"- **Runs**: {runs_count}")
                report.append(f"- **Average execution time**: {avg_time:.3f}s")
                report.append(f"- **Deterministic**: {'✓' if deterministic else '✗'}")

                # Success rates
                successful_runs = sum(1 for run in result["runs"] if run.get("returncode") == 0)
                report.append(f"- **Success rate**: {successful_runs}/{runs_count}")

                if not deterministic:
                    report.append("- **⚠ Warning**: Non-deterministic behavior detected!")

            report.append("")

        # Stress test results
        if hasattr(self, "stress_results"):
            report.append("## Stress Test Results")
            stress = self.stress_results

            if stress.get("concurrent_pytest_runs"):
                concurrent = stress["concurrent_pytest_runs"]
                if "all_passed" in concurrent:
                    status = "✓" if concurrent["all_passed"] else "✗"
                    report.append(f"- **Concurrent pytest execution**: {status}")

            if stress.get("timing_variance"):
                timing = stress["timing_variance"]
                if "stable" in timing:
                    status = "✓" if timing["stable"] else "✗"
                    cv = timing.get("coefficient_variation", 0)
                    report.append(f"- **Timing stability**: {status} (CV: {cv:.3f})")

            report.append("")

        # Recommendations
        report.append("## Recommendations")
        failed_deps = [dep for dep, status in dependencies.items() if not status]
        if failed_deps:
            report.append("### Missing Dependencies")
            report.extend(f"- Install {dep}" for dep in failed_deps)
            report.append("")

        non_deterministic = [name for name, result in self.results.items()
                           if not result.get("deterministic", True)]
        if non_deterministic:
            report.append("### Non-Deterministic Tests")
            report.extend(f"- Review and fix {module}" for module in non_deterministic)
            report.append("")

        slow_tests = [(name, result.get("average_time", 0))
                     for name, result in self.results.items()
                     if result.get("average_time", 0) > 30]
        if slow_tests:
            report.append("### Slow Tests (>30s)")
            for module, time_taken in slow_tests:
                report.append(f"- Optimize {module} (avg: {time_taken:.1f}s)")
            report.append("")

        report.append("---")
        report.append("*Report generated by Euxis Concurrency Test Verifier*")

        return "\n".join(report)

    def run_all_tests(self, runs_per_module: int = 3) -> None:
        """Run all concurrency tests and generate report."""
        # Check dependencies first
        dependencies = self.check_test_dependencies()
        missing_deps = [dep for dep, status in dependencies.items() if not status]
        if missing_deps:
            pass

        # Run each test module
        for module_path in self.test_modules:
            module_path = Path(module_path)

            if not module_path.exists():
                continue

            if module_name.endswith(".py"):
                result = self.run_python_test_module(module_path, runs_per_module)
            else:
                result = self.run_bash_test_script(module_path, runs_per_module)

            self.results[module_path.name] = result

        # Run stress tests
        self.stress_results = self.run_stress_test()

        # Generate and save report
        report = self.generate_report()
        report_path = self.tests_dir / "cli" / "tests" / "concurrency_test_report.md"

        with report_path.open("w") as f:
            f.write(report)


        # Print summary
        total_modules = len(self.results)
        deterministic_modules = sum(
            1 for r in self.results.values() if r.get("deterministic", False)
        )
        return deterministic_modules == total_modules




def main():
    """Run the concurrency test verification suite."""
    verifier = ConcurrencyTestVerifier()

    if len(sys.argv) > 1:
        if sys.argv[1] == "--quick":
            # Quick verification with single run
            success = verifier.run_all_tests(runs_per_module=1)
        elif sys.argv[1] == "--stress":
            # Extended verification with multiple runs
            success = verifier.run_all_tests(runs_per_module=5)
        else:
            sys.exit(1)
    else:
        # Standard verification
        success = verifier.run_all_tests(runs_per_module=3)

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
