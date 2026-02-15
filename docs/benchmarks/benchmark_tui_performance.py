#!/usr/bin/env python3
"""TUI Performance Benchmark for FleetGrid operations."""

import time
import sys
from pathlib import Path

sys.path.append(str(Path(__file__).resolve().parents[1]))

from tui.core.registry import FleetRegistry

def benchmark_fleet_grid_operations():
    """Benchmark the expensive FleetGrid operations."""
    registry = FleetRegistry.load()

    print("=== TUI Performance Benchmark ===")
    print(f"Fleet Size: {len(registry.agents)} agents, {len(registry.squads)} squads, {len(registry.combos)} combos")

    # Simulate FleetGrid filtering operations
    test_queries = ["", "debug", "core", "architect", "performance", "zzz"]

    for query in test_queries:
        start = time.perf_counter()

        # Simulate _matches_filter for all agents (like in FleetGrid)
        matches = []
        query_lower = query.lower()
        for agent in registry.agents:
            if not query:
                matches.append(agent)
            elif (query_lower in agent.id.lower()
                  or any(query_lower in t.lower() for t in agent.tags)
                  or query_lower in agent.tier.lower()
                  or query_lower in agent.activation.lower()):
                matches.append(agent)

        filter_time = (time.perf_counter() - start) * 1000

        # Simulate property filter calls (like in _rebuild_grid)
        start = time.perf_counter()
        core_filtered = [a for a in matches if a.tier == "core"]
        default_filtered = [a for a in matches if a.activation == "default" and a.tier != "core"]
        ondemand_filtered = [a for a in matches if a.activation == "on-demand"]
        specialist_filtered = [a for a in matches if a.activation == "specialist"]
        property_time = (time.perf_counter() - start) * 1000

        total_time = filter_time + property_time
        print(f"Query: '{query:12}' | Total: {total_time:6.2f}ms | Filter: {filter_time:6.2f}ms | Group: {property_time:6.2f}ms | Results: {len(matches):2d}")

    # Simulate squad/combo linear searches
    print("\n=== Squad/Combo Search Performance ===")
    test_squad_ids = ["security", "creative", "nonexistent"]

    for squad_id in test_squad_ids:
        start = time.perf_counter()
        found_squad = None
        for squad in registry.squads:
            if squad.id == squad_id:
                found_squad = squad
                break
        search_time = (time.perf_counter() - start) * 1000
        print(f"Squad search '{squad_id:12}': {search_time:6.4f}ms | Found: {found_squad is not None}")

    test_combo_ids = ["envision", "research", "nonexistent"]
    for combo_id in test_combo_ids:
        start = time.perf_counter()
        found_combo = None
        for combo in registry.combos:
            if combo.id == combo_id:
                found_combo = combo
                break
        search_time = (time.perf_counter() - start) * 1000
        print(f"Combo search '{combo_id:12}': {search_time:6.4f}ms | Found: {found_combo is not None}")

    # Simulate multiple rapid filter changes (like typing in search)
    print("\n=== Rapid Filter Changes (Typing Simulation) ===")
    typing_sequence = ["a", "ar", "arc", "arch", "archi", "archit", "architect"]

    total_typing_time = 0
    for i, partial_query in enumerate(typing_sequence):
        start = time.perf_counter()
        matches = []
        query_lower = partial_query.lower()
        for agent in registry.agents:
            if (query_lower in agent.id.lower()
                or any(query_lower in t.lower() for t in agent.tags)):
                matches.append(agent)
        typing_time = (time.perf_counter() - start) * 1000
        total_typing_time += typing_time
        print(f"  Step {i+1}: '{partial_query:9}' -> {len(matches):2d} matches in {typing_time:6.2f}ms")

    print(f"  Total typing sequence: {total_typing_time:6.2f}ms")

    # Memory efficiency check (optional)
    print("\n=== Memory Efficiency Analysis ===")
    try:
        import psutil  # type: ignore
        import os
    except Exception:
        print("psutil not installed; skipping memory usage")
    else:
        process = psutil.Process(os.getpid())
        memory_mb = process.memory_info().rss / 1024 / 1024
        print(f"Current Memory Usage: {memory_mb:.1f} MB")

    # Test registry reload (refresh action)
    start = time.perf_counter()
    registry2 = FleetRegistry.load()
    reload_time = (time.perf_counter() - start) * 1000
    print(f"Registry Reload Time: {reload_time:.2f}ms")

if __name__ == "__main__":
    benchmark_fleet_grid_operations()
