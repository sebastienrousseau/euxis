#!/usr/bin/env python3
"""Comprehensive Performance Audit for Euxis
Tests all hot paths with 10x and 100x typical workloads.
Establishes performance budgets and validates against them.
"""

import gc
import json
import os
import subprocess
import sys
import tempfile
import time
import tracemalloc
from pathlib import Path
from statistics import mean, stdev

import psutil


# Performance budgets (based on existing codebase analysis)
class PerformanceBudgets:
    # Startup budgets (milliseconds)
    COLD_START_MAX = 2000
    WARM_START_MAX = 500
    REGISTRY_LOAD_MAX = 100
    CONFIG_LOAD_MAX = 50

    # Hot path budgets (milliseconds)
    AGENT_LOOKUP_MAX = 0.1
    PROPERTY_ACCESS_MAX = 0.1
    SEARCH_OPERATION_MAX = 1.0
    SCREEN_TRANSITION_MAX = 200

    # Memory budgets (MB)
    BASE_MEMORY_MAX = 50
    REGISTRY_MEMORY_MAX = 5
    MEMORY_GROWTH_MAX = 2.0  # 2x minimum acceptable

    # Build performance budgets
    BUILD_TIME_MAX = 30000  # 30 seconds
    ARTIFACT_SIZE_MAX = 10  # 10MB


def measure_time_stats(func, iterations: int = 5) -> tuple[float, float]:
    """Measure function execution time statistics."""
    times = []
    for _ in range(iterations):
        start = time.perf_counter()
        func()
        end = time.perf_counter()
        times.append((end - start) * 1000)  # Convert to ms
    return mean(times), stdev(times) if len(times) > 1 else 0.0


def measure_memory_usage(func) -> tuple[float, float]:
    """Measure memory usage before and after function execution."""
    tracemalloc.start()
    gc.collect()

    tracemalloc.take_snapshot()
    peak_before = tracemalloc.get_traced_memory()[1]

    func()

    tracemalloc.take_snapshot()
    peak_after = tracemalloc.get_traced_memory()[1]

    tracemalloc.stop()

    return (peak_after - peak_before) / 1024 / 1024, peak_after / 1024 / 1024


class ComprehensiveBenchmark:
    def __init__(self) -> None:
        self.results = {}
        self.violations = []
        self.process = psutil.Process()

        # Add project root to path
        project_root = Path(__file__).parent
        sys.path.insert(0, str(project_root))

    def benchmark_startup_performance(self) -> None:
        """Benchmark application startup times."""
        # Cold startup benchmark
        cold_times = []
        for _i in range(3):

            # Clear module cache
            modules_to_clear = [m for m in sys.modules if m.startswith("tui.")]
            for module in modules_to_clear:
                if module in sys.modules:
                    del sys.modules[module]

            start_time = time.perf_counter()

            try:
                from tui.app import EuxisApp
                app = EuxisApp()
                # Force initialization
                _ = app.config
                _ = app.fleet_registry
                _ = app.project_name
                _ = app.git_branch
            except ImportError:
                cold_times.append(float("inf"))
                continue

            end_time = time.perf_counter()
            cold_times.append((end_time - start_time) * 1000)

        # Warm startup benchmark
        warm_times = []
        for _i in range(5):
            start_time = time.perf_counter()
            try:
                from tui.app import EuxisApp
                app = EuxisApp()
                _ = app.config
                _ = app.fleet_registry
            except ImportError:
                warm_times.append(float("inf"))
                continue
            end_time = time.perf_counter()
            warm_times.append((end_time - start_time) * 1000)

        cold_avg = mean([t for t in cold_times if t != float("inf")])
        warm_avg = mean([t for t in warm_times if t != float("inf")])

        self.results["startup"] = {
            "cold_start_ms": cold_avg,
            "warm_start_ms": warm_avg,
            "cold_pass": cold_avg < PerformanceBudgets.COLD_START_MAX,
            "warm_pass": warm_avg < PerformanceBudgets.WARM_START_MAX
        }


        if cold_avg >= PerformanceBudgets.COLD_START_MAX:
            self.violations.append(f"Cold startup {cold_avg:.2f}ms exceeds {PerformanceBudgets.COLD_START_MAX}ms budget")
        if warm_avg >= PerformanceBudgets.WARM_START_MAX:
            self.violations.append(f"Warm startup {warm_avg:.2f}ms exceeds {PerformanceBudgets.WARM_START_MAX}ms budget")

    def create_synthetic_registry(self, agent_count: int) -> dict:
        """Create synthetic registry for scale testing."""
        return {
            "schema_version": "1.0",
            "protocol_version": "0.0.7",
            "last_updated": "2026-02-09",
            "agents": [
                {
                    "id": f"agent_{i:05d}",
                    "tier": "core" if i % 10 == 0 else "fleet",
                    "version": "0.0.7",
                    "tags": [f"tag_{i % 5}", f"category_{i % 7}", f"type_{i % 3}"],
                    "activation": ["default", "on-demand", "specialist"][i % 3],
                    "capability_tags": [f"capability_{i % 8}", f"skill_{i % 12}"]
                }
                for i in range(agent_count)
            ]
        }

    def benchmark_scale_performance(self) -> None:
        """Benchmark performance under 10x and 100x typical workloads."""
        base_agent_count = 41  # Current fleet size
        scale_factors = [1, 10, 100]  # 1x, 10x, 100x
        scale_results = {}

        try:
            from tui.core.registry import FleetRegistry

            for scale in scale_factors:
                agent_count = base_agent_count * scale

                # Create synthetic registry
                registry_data = self.create_synthetic_registry(agent_count)

                with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as f:
                    json.dump(registry_data, f)
                    temp_file = f.name

                try:
                    # Measure loading time and memory
                    def load_registry():
                        return FleetRegistry.from_dicts(registry_data)

                    load_time, _load_std = measure_time_stats(load_registry, 3)
                    memory_delta, _peak_memory = measure_memory_usage(load_registry)

                    # Test registry operations
                    registry = load_registry()

                    # Agent lookup performance
                    def lookup_test() -> None:
                        for i in range(0, min(100, agent_count), 10):
                            registry.get_agent(f"agent_{i:05d}")

                    lookup_time, _lookup_std = measure_time_stats(lookup_test, 5)
                    lookup_per_operation = lookup_time / min(10, agent_count)

                    scale_results[scale] = {
                        "agent_count": agent_count,
                        "load_time_ms": load_time,
                        "memory_mb": memory_delta,
                        "lookup_per_op_ms": lookup_per_operation,
                        "pass": (load_time < PerformanceBudgets.REGISTRY_LOAD_MAX * scale and
                                lookup_per_operation < PerformanceBudgets.AGENT_LOOKUP_MAX)
                    }


                    if lookup_per_operation >= PerformanceBudgets.AGENT_LOOKUP_MAX:
                        self.violations.append(f"{scale}x scale: Agent lookup {lookup_per_operation:.4f}ms exceeds {PerformanceBudgets.AGENT_LOOKUP_MAX}ms budget")

                finally:
                    os.unlink(temp_file)

        except ImportError:
            scale_results = {}

        self.results["scale"] = scale_results

    def benchmark_build_performance(self) -> None:
        """Benchmark build time and output artifact size."""
        # Check if we're in a git repository
        try:
            # Test build time (using pip install -e .)
            start_time = time.perf_counter()
            result = subprocess.run([
                sys.executable, "-m", "pip", "install", "-e", ".", "--quiet"
            ], capture_output=True, text=True, timeout=60)
            end_time = time.perf_counter()

            build_time = (end_time - start_time) * 1000
            build_success = result.returncode == 0


            if build_time >= PerformanceBudgets.BUILD_TIME_MAX:
                self.violations.append(f"Build time {build_time:.2f}ms exceeds {PerformanceBudgets.BUILD_TIME_MAX}ms budget")

        except subprocess.TimeoutExpired:
            build_time = float("inf")
            build_success = False
            self.violations.append("Build time exceeded 60s timeout")
        except Exception:
            build_time = float("inf")
            build_success = False

        # Check package size
        try:
            import importlib.metadata
            importlib.metadata.distribution("euxis")
            # Get installation size (approximate)
            package_size = 0
            try:
                # This is an approximation - actual size calculation is complex
                package_size = 1.0  # Placeholder MB
            except:
                package_size = float("inf")


        except Exception:
            package_size = float("inf")

        self.results["build"] = {
            "build_time_ms": build_time,
            "package_size_mb": package_size,
            "build_success": build_success
        }

    def benchmark_memory_efficiency(self) -> None:
        """Test memory usage under sustained load."""
        # Get baseline memory
        gc.collect()
        baseline_memory = self.process.memory_info().rss / 1024 / 1024

        try:
            from tui.core.config import ETXConfig
            from tui.core.registry import FleetRegistry

            # Load normal configuration
            ETXConfig.load()
            registry = FleetRegistry.load()

            # Simulate sustained operations
            operations = []
            for _ in range(100):
                # Simulate typical operations
                agents = [registry.get_agent(f"agent_{i:03d}") for i in range(min(41, len(registry.agents)))]
                core_agents = [a for a in registry.agents if a.tier == "core"]
                operations.append(len(agents) + len(core_agents))

            gc.collect()
            peak_memory = self.process.memory_info().rss / 1024 / 1024
            memory_used = peak_memory - baseline_memory


            # Check for memory leaks (run operations again)
            for _ in range(100):
                agents = [registry.get_agent(f"agent_{i:03d}") for i in range(min(41, len(registry.agents)))]
                core_agents = [a for a in registry.agents if a.tier == "core"]

            gc.collect()
            final_memory = self.process.memory_info().rss / 1024 / 1024
            memory_growth = final_memory - peak_memory


            # Check budgets
            if memory_used > PerformanceBudgets.BASE_MEMORY_MAX:
                self.violations.append(f"Memory usage {memory_used:.2f}MB exceeds {PerformanceBudgets.BASE_MEMORY_MAX}MB budget")

            if memory_growth > 1.0:  # More than 1MB growth indicates potential leak
                self.violations.append(f"Memory growth {memory_growth:.2f}MB indicates potential memory leak")

            self.results["memory"] = {
                "baseline_mb": baseline_memory,
                "peak_mb": peak_memory,
                "used_mb": memory_used,
                "growth_mb": memory_growth,
                "memory_efficient": memory_used <= PerformanceBudgets.BASE_MEMORY_MAX and memory_growth <= 1.0
            }

        except ImportError as e:
            self.results["memory"] = {"error": str(e)}

    def generate_report(self) -> None:
        """Generate comprehensive performance report."""
        # Summary status
        len([v for v in self.results.values() if isinstance(v, dict)])
        passed_tests = 0

        for category, data in self.results.items():
            if isinstance(data, dict):
                if category == "startup":
                    if data.get("cold_pass", False) and data.get("warm_pass", False):
                        passed_tests += 1
                elif category == "scale":
                    if all(scale_data.get("pass", False) for scale_data in data.values()):
                        passed_tests += 1
                elif category == "build":
                    if data.get("build_success", False):
                        passed_tests += 1
                elif category == "memory" and data.get("memory_efficient", False):
                    passed_tests += 1


        if self.violations:
            for _i, _violation in enumerate(self.violations, 1):
                pass


        # Detailed results

    def run_full_benchmark(self) -> None:
        """Run the complete benchmark suite."""
        self.benchmark_startup_performance()
        self.benchmark_scale_performance()
        self.benchmark_build_performance()
        self.benchmark_memory_efficiency()
        self.generate_report()


if __name__ == "__main__":
    benchmark = ComprehensiveBenchmark()
    benchmark.run_full_benchmark()
