#!/usr/bin/env python3
"""Final comprehensive benchmark for the performance audit."""

import statistics
import sys
import time
import tracemalloc
from pathlib import Path

import psutil

# Add project root to path
project_root = Path(__file__).parent
sys.path.insert(0, str(project_root))

def measure_startup_time():
    """Measure cold and warm startup time."""
    # Cold startup (with import overhead)
    cold_times = []
    for _i in range(3):

        # Clear Python module cache to simulate cold start
        modules_to_clear = [m for m in sys.modules if m.startswith("tui.")]
        for module in modules_to_clear:
            if module in sys.modules:
                del sys.modules[module]

        start_time = time.perf_counter()

        # Import and create app (cold)
        from tui.app import EuxisApp
        app = EuxisApp()

        # Force initialization
        _ = app.config
        _ = app.fleet_registry
        _ = app.project_name
        _ = app.git_branch

        end_time = time.perf_counter()
        cold_times.append((end_time - start_time) * 1000)

    # Warm startup (imports already cached)
    warm_times = []
    for _i in range(5):
        start_time = time.perf_counter()

        # Create app (warm - imports cached)
        app = EuxisApp()
        _ = app.config
        _ = app.fleet_registry

        end_time = time.perf_counter()
        warm_times.append((end_time - start_time) * 1000)


    return {
        "cold_mean_ms": statistics.mean(cold_times),
        "warm_mean_ms": statistics.mean(warm_times),
        "cold_budget_ms": 2000,
        "warm_budget_ms": 500,
        "cold_passes": statistics.mean(cold_times) <= 2000,
        "warm_passes": statistics.mean(warm_times) <= 500
    }

def measure_memory_footprint():
    """Measure memory footprint under typical workload."""
    process = psutil.Process()
    initial_memory = process.memory_info().rss / 1024 / 1024  # MB

    # Start memory tracking
    tracemalloc.start()

    # Simulate typical workload
    from tui.app import EuxisApp
    app = EuxisApp()

    # Load all data
    registry = app.fleet_registry
    agents = registry.agents

    # Simulate UI operations

    # Simulate repeated lookups
    for _ in range(100):
        registry.get_agent("optimizer")
        registry.get_agent("architect")
        registry.get_agent("debugger")

    # Get memory usage
    _current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()

    final_memory = process.memory_info().rss / 1024 / 1024  # MB
    used_memory = final_memory - initial_memory
    peak_allocated = peak / 1024 / 1024  # MB

    # Estimate minimum required memory (just the data structures)
    agent_count = len(agents)
    estimated_minimum = (agent_count * 1.0) / 1024  # 1KB per agent in MB


    memory_ratio = used_memory / estimated_minimum if estimated_minimum > 0 else 0

    return {
        "used_memory_mb": used_memory,
        "peak_memory_mb": peak_allocated,
        "estimated_minimum_mb": estimated_minimum,
        "memory_ratio": memory_ratio,
        "passes_2x_budget": memory_ratio <= 2.0,
        "agent_count": agent_count
    }

def identify_hot_paths():
    """Profile to identify the slowest operations."""
    hot_paths = {}

    # Profile config loading
    start_time = time.perf_counter()
    from tui.core.config import ETXConfig
    ETXConfig.load()
    config_time = (time.perf_counter() - start_time) * 1000

    # Profile registry loading
    start_time = time.perf_counter()
    from tui.core.registry import FleetRegistry
    registry = FleetRegistry.load(euxis_home=project_root)
    registry_time = (time.perf_counter() - start_time) * 1000

    # Profile git operations
    start_time = time.perf_counter()
    from tui.core.runner import get_git_branch, get_project_name
    get_project_name()
    get_git_branch()
    git_time = (time.perf_counter() - start_time) * 1000

    # Profile textual import
    start_time = time.perf_counter()
    textual_time = (time.perf_counter() - start_time) * 1000

    # Profile property computations
    start_time = time.perf_counter()
    for _ in range(100):
        _ = registry.core_agents
        _ = registry.default_agents
    property_time = (time.perf_counter() - start_time) * 1000

    hot_paths = {
        "config_loading": config_time,
        "registry_loading": registry_time,
        "git_operations": git_time,
        "textual_import": textual_time,
        "property_computation_100x": property_time
    }

    for _path, _time_ms in sorted(hot_paths.items(), key=lambda x: x[1], reverse=True):
        pass

    return hot_paths

def measure_cpu_usage():
    """Measure CPU usage during operations."""
    process = psutil.Process()

    # Measure CPU during intensive operations
    process.cpu_percent()

    start_time = time.perf_counter()

    # Simulate CPU-intensive operations
    from tui.core.registry import FleetRegistry
    registry = FleetRegistry.load(euxis_home=project_root)

    # Repeated filtering (CPU intensive)
    for _ in range(1000):
        _ = registry.core_agents
        _ = registry.default_agents
        _ = registry.ondemand_agents
        _ = registry.specialist_agents

    duration = time.perf_counter() - start_time

    # Small delay to let CPU measurement settle
    time.sleep(0.1)
    cpu_after = process.cpu_percent()


    return {
        "duration_ms": duration * 1000,
        "cpu_percent": cpu_after,
        "operations_per_second": 4000 / duration
    }

def main() -> int:

    # Startup time measurement
    startup_results = measure_startup_time()

    # Memory footprint analysis
    memory_results = measure_memory_footprint()

    # Hot path identification
    hot_paths = identify_hot_paths()

    # CPU usage analysis
    measure_cpu_usage()


    # Startup time assessment

    # Memory assessment

    # Performance issues summary

    findings = []

    if not startup_results["cold_passes"]:
        findings.append(f"Cold startup ({startup_results['cold_mean_ms']:.2f}ms) exceeds budget")

    if not memory_results["passes_2x_budget"]:
        findings.append(f"Memory usage ({memory_results['memory_ratio']:.1f}x) exceeds 2x minimum")

    # Check hot paths
    if hot_paths.get("textual_import", 0) > 100:
        findings.append(f"Textual import is slow ({hot_paths['textual_import']:.1f}ms)")

    if hot_paths.get("property_computation_100x", 0) > 50:
        findings.append("Property computation inefficiency detected")

    if findings:
        for _i, _finding in enumerate(findings, 1):
            pass


    else:
        pass

    "✅ PASS" if startup_results["cold_passes"] else "❌ FAIL"
    "✅ PASS" if memory_results["passes_2x_budget"] else "❌ FAIL"

    overall_pass = startup_results["cold_passes"] and memory_results["passes_2x_budget"]

    return 0 if overall_pass else 1

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
    except Exception:
        import traceback
        traceback.print_exc()
        sys.exit(1)
