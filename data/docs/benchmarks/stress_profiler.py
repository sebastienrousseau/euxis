#!/usr/bin/env python3
"""Performance stress testing with large synthetic datasets."""

import json
import statistics
import sys
import tempfile
import time
import tracemalloc
from pathlib import Path

# Add project root to path
project_root = Path(__file__).parent
sys.path.insert(0, str(project_root))

from tui.core.registry import FleetRegistry


def create_synthetic_registry(agent_count: int) -> dict:
    """Create synthetic registry data with specified agent count."""
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

def profile_large_registry_loading(agent_counts: list[int], iterations=3):
    """Profile registry loading with different agent counts."""
    results = {}

    for count in agent_counts:
        times = []
        memory_usage = []

        for _ in range(iterations):
            # Create synthetic data
            data = create_synthetic_registry(count)

            with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as f:
                json.dump(data, f)
                temp_path = Path(f.name)

            try:
                tracemalloc.start()
                start_time = time.perf_counter()

                # Simulate registry loading
                registry = FleetRegistry()
                registry._parse_agents(data)

                # Force property evaluation
                _ = len(registry.agents)
                _ = len(registry.core_agents)
                _ = len(registry.default_agents)

                end_time = time.perf_counter()
                _current, peak = tracemalloc.get_traced_memory()
                tracemalloc.stop()

                times.append((end_time - start_time) * 1000)
                memory_usage.append(peak / 1024 / 1024)

            finally:
                temp_path.unlink(missing_ok=True)

        if times:
            results[count] = {
                "mean_ms": statistics.mean(times),
                "p95_ms": sorted(times)[int(0.95 * len(times))] if len(times) > 1 else times[0],
                "memory_mb": statistics.mean(memory_usage),
                "agents_per_ms": count / statistics.mean(times)
            }

    return results

def profile_property_access_scaling(agent_counts: list[int]):
    """Profile how property access performance scales with agent count."""
    results = {}

    for count in agent_counts:

        # Create registry
        data = create_synthetic_registry(count)
        registry = FleetRegistry()
        registry._parse_agents(data)

        # Test property access performance
        iterations = 100
        start_time = time.perf_counter()

        for _ in range(iterations):
            _ = len(registry.core_agents)
            _ = len(registry.default_agents)
            _ = len(registry.ondemand_agents)
            _ = len(registry.specialist_agents)

        end_time = time.perf_counter()
        total_time = (end_time - start_time) * 1000
        avg_time = total_time / iterations

        results[count] = {
            "avg_per_access_ms": avg_time,
            "total_ms": total_time,
            "iterations": iterations
        }

    return results

def profile_agent_search_scaling(agent_counts: list[int]):
    """Profile how agent search scales with agent count."""
    results = {}

    for count in agent_counts:

        # Create registry
        data = create_synthetic_registry(count)
        registry = FleetRegistry()
        registry._parse_agents(data)

        # Test search performance for different positions
        search_times = []
        test_positions = [0, count // 4, count // 2, count * 3 // 4, count - 1]

        for pos in test_positions:
            if pos < len(registry.agents):
                agent_id = registry.agents[pos].id

                start_time = time.perf_counter()
                registry.get_agent(agent_id)
                end_time = time.perf_counter()

                search_times.append((end_time - start_time) * 1000)

        if search_times:
            results[count] = {
                "mean_ms": statistics.mean(search_times),
                "max_ms": max(search_times),
                "worst_case_position": count - 1,  # Last position is worst case for linear search
                "searches_tested": len(search_times)
            }

    return results

def profile_memory_usage_scaling():
    """Profile memory usage as dataset grows."""
    agent_counts = [100, 500, 1000, 2000, 5000]
    memory_results = {}

    for count in agent_counts:
        tracemalloc.start()

        # Create and load registry
        data = create_synthetic_registry(count)
        registry = FleetRegistry()
        registry._parse_agents(data)

        # Force all objects to be created

        _current, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()

        memory_results[count] = {
            "memory_mb": peak / 1024 / 1024,
            "memory_per_agent_kb": (peak / 1024) / count,
            "agent_count": count
        }

    return memory_results

def detect_algorithmic_complexity() -> None:
    """Detect if operations scale linearly or worse."""
    # Test loading complexity
    load_counts = [100, 500, 1000, 2000]
    load_results = profile_large_registry_loading(load_counts, iterations=1)

    # Calculate if loading time grows linearly
    if len(load_results) >= 2:
        counts = list(load_results.keys())
        times = [load_results[c]["mean_ms"] for c in counts]

        # Check if time grows roughly linearly with agent count
        ratios = []
        for i in range(1, len(counts)):
            count_ratio = counts[i] / counts[i-1]
            time_ratio = times[i] / times[i-1]
            ratios.append(time_ratio / count_ratio)

        avg_ratio = statistics.mean(ratios) if ratios else 1.0

        if avg_ratio > 1.5:
            pass
        else:
            pass

    # Test search complexity
    search_counts = [100, 1000, 5000]
    search_results = profile_agent_search_scaling(search_counts)

    if len(search_results) >= 2:
        counts = list(search_results.keys())
        times = [search_results[c]["mean_ms"] for c in counts]

        # For O(n) search, time should grow linearly with count
        ratios = []
        for i in range(1, len(counts)):
            count_ratio = counts[i] / counts[i-1]
            time_ratio = times[i] / times[i-1] if times[i-1] > 0 else 1
            ratios.append(time_ratio / count_ratio)

        avg_ratio = statistics.mean(ratios) if ratios else 1.0

        if avg_ratio > 2.0:
            pass
        else:
            pass

def main() -> int:

    large_counts = [100, 500, 1000, 2000, 5000]
    load_results = profile_large_registry_loading(large_counts)

    for count, result in load_results.items():
        pass

    property_counts = [100, 1000, 5000]
    property_results = profile_property_access_scaling(property_counts)

    for count, result in property_results.items():
        pass

    search_counts = [100, 1000, 5000]
    search_results = profile_agent_search_scaling(search_counts)

    for count, result in search_results.items():
        pass

    memory_results = profile_memory_usage_scaling()

    for count, result in memory_results.items():
        pass

    # Complexity analysis
    detect_algorithmic_complexity()


    violations = []

    # Check if large datasets exceed budgets
    for count, result in load_results.items():
        if count >= 1000 and result["mean_ms"] > 500:  # 500ms budget for 1000+ agents
            violations.append(f"Loading {count} agents: {result['mean_ms']:.1f}ms exceeds 500ms budget")

    # Check property access efficiency
    worst_property = max(property_results.values(), key=lambda x: x["avg_per_access_ms"])
    if worst_property["avg_per_access_ms"] > 0.1:  # 0.1ms budget per property access
        violations.append(f"Property access: {worst_property['avg_per_access_ms']:.4f}ms exceeds 0.1ms budget")

    # Check search efficiency
    worst_search = max(search_results.values(), key=lambda x: x["max_ms"])
    if worst_search["max_ms"] > 1.0:  # 1ms budget for agent search
        violations.append(f"Agent search: {worst_search['max_ms']:.4f}ms exceeds 1ms budget")

    # Check memory efficiency
    worst_memory = max(memory_results.values(), key=lambda x: x["memory_per_agent_kb"])
    if worst_memory["memory_per_agent_kb"] > 10:  # 10KB per agent budget
        violations.append(f"Memory per agent: {worst_memory['memory_per_agent_kb']:.1f}KB exceeds 10KB budget")

    if violations:
        for _i, _violation in enumerate(violations, 1):
            pass


    else:
        pass

    return 0 if not violations else 1

if __name__ == "__main__":
    sys.exit(main())
