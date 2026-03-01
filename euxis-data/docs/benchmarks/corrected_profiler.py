#!/usr/bin/env python3
"""Corrected performance profiler using actual registry files."""

import statistics
import sys
import time
import tracemalloc
from pathlib import Path

# Add project root to path
project_root = Path(__file__).parent
sys.path.insert(0, str(project_root))

from tui.core.registry import FleetRegistry


def profile_actual_registry_loading(iterations=5):
    """Profile FleetRegistry.load() with actual project files."""
    times = []
    memory_usage = []

    # Use project root as euxis_home where the actual files are
    project_euxis_home = project_root

    for _ in range(iterations):
        tracemalloc.start()
        start_time = time.perf_counter()

        # Load from actual project files
        registry = FleetRegistry.load(euxis_home=project_euxis_home)

        # Force evaluation of all filtered views (these use list comprehensions)
        agent_count = len(registry.agents)
        core_agents = len(registry.core_agents)
        len(registry.default_agents)
        len(registry.ondemand_agents)
        len(registry.specialist_agents)

        # Test agent lookup (O(n) search)
        registry.get_agent("optimizer")

        end_time = time.perf_counter()
        _current, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()

        times.append((end_time - start_time) * 1000)  # Convert to ms
        memory_usage.append(peak / 1024 / 1024)  # Convert to MB

    return {
        "mean_ms": statistics.mean(times),
        "p95_ms": sorted(times)[int(0.95 * len(times))] if len(times) > 1 else times[0],
        "min_ms": min(times),
        "max_ms": max(times),
        "memory_mb": statistics.mean(memory_usage),
        "agent_count": agent_count,
        "core_agents": core_agents,
        "budget_ms": 100,
        "passes_budget": statistics.mean(times) <= 100
    }

def profile_full_app_initialization():
    """Profile full TUI app initialization including Textual."""
    tracemalloc.start()
    start_time = time.perf_counter()

    try:
        # Import textual components - this is where the real loading happens
        from tui.app import EuxisApp

        # Create app instance (this loads config, registry, and sets up Textual)
        app = EuxisApp()

        # Force initialization that happens in on_mount
        _ = app.config.theme
        _ = app.fleet_registry.agents
        _ = app.fleet_registry.core_agents

        end_time = time.perf_counter()
        _current, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()

        startup_time = (end_time - start_time) * 1000
        memory_mb = peak / 1024 / 1024

        return {
            "startup_ms": startup_time,
            "memory_mb": memory_mb,
            "budget_ms": 2000,
            "passes_budget": startup_time <= 2000
        }

    except Exception as e:
        tracemalloc.stop()
        return {"error": str(e)}

def profile_repeated_property_access():
    """Profile the O(n) list comprehensions that happen on every property access."""
    registry = FleetRegistry.load(euxis_home=project_root)

    iterations = 1000

    # Test repeated property access (simulates multiple UI updates)
    start_time = time.perf_counter()

    for _ in range(iterations):
        # These create new lists every time (no caching)
        _ = len(registry.core_agents)
        _ = len(registry.default_agents)
        _ = len(registry.ondemand_agents)
        _ = len(registry.specialist_agents)

    end_time = time.perf_counter()

    total_time = (end_time - start_time) * 1000
    avg_time_per_access = total_time / iterations

    return {
        "total_ms": total_time,
        "avg_per_access_ms": avg_time_per_access,
        "iterations": iterations,
        "agent_count": len(registry.agents)
    }

def profile_agent_search_performance():
    """Profile O(n) agent search performance."""
    registry = FleetRegistry.load(euxis_home=project_root)

    search_times = []
    agent_ids = [agent.id for agent in registry.agents]

    for agent_id in agent_ids:
        start_time = time.perf_counter()
        registry.get_agent(agent_id)
        end_time = time.perf_counter()
        search_times.append((end_time - start_time) * 1000)

    return {
        "mean_ms": statistics.mean(search_times),
        "p95_ms": sorted(search_times)[int(0.95 * len(search_times))],
        "max_ms": max(search_times),
        "searches_performed": len(search_times),
        "complexity": "O(n)"
    }

def main() -> int:

    registry_results = profile_actual_registry_loading()

    app_results = profile_full_app_initialization()
    if "error" in app_results:
        pass
    else:
        pass

    property_results = profile_repeated_property_access()

    search_results = profile_agent_search_performance()

    # Performance issues analysis

    bottlenecks = []

    # Check for property access inefficiency
    if property_results["avg_per_access_ms"] > 0.01:  # More than 0.01ms per access
        bottlenecks.append(f"Property access inefficiency: {property_results['avg_per_access_ms']:.4f}ms per access (no caching)")

    # Check for slow search
    if search_results["p95_ms"] > 0.1:  # More than 0.1ms for search
        bottlenecks.append(f"Agent search inefficiency: {search_results['p95_ms']:.4f}ms P95 with O(n) complexity")

    # Check registry loading
    if not registry_results["passes_budget"]:
        bottlenecks.append(f"Registry loading exceeds budget: {registry_results['mean_ms']:.2f}ms vs {registry_results['budget_ms']}ms")

    # Check app startup
    if "startup_ms" in app_results and not app_results.get("passes_budget", True):
        bottlenecks.append(f"App startup exceeds budget: {app_results['startup_ms']:.2f}ms vs {app_results['budget_ms']}ms")

    if bottlenecks:
        for _i, _bottleneck in enumerate(bottlenecks, 1):
            pass

    else:
        pass

    return 0 if not bottlenecks else 1

if __name__ == "__main__":
    sys.exit(main())
