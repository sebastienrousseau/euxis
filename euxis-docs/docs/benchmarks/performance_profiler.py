#!/usr/bin/env python3
"""Euxis TUI Performance Profiler & Baseline Suite
Comprehensive benchmarking for hot paths, memory usage, and scalability.
"""

import json
import os
import statistics
import subprocess
import sys
import tempfile
import time
import tracemalloc
from pathlib import Path

# Add project root to path for imports
project_root = Path(__file__).parent
sys.path.insert(0, str(project_root))

try:
    from tui.app import EuxisApp
    from tui.core.config import ETXConfig
    from tui.core.registry import FleetRegistry
    from tui.core.runner import get_git_branch, get_project_name
except ImportError:
    sys.exit(1)


class PerformanceBudgets:
    """Extended performance budgets for TUI operations."""

    # Application Startup Budgets (milliseconds)
    APP_COLD_START_MAX = 2000      # 2s for cold start
    APP_WARM_START_MAX = 500       # 500ms for subsequent starts
    REGISTRY_LOAD_MAX = 100        # 100ms for fleet registry loading
    CONFIG_LOAD_MAX = 50          # 50ms for config loading
    SCREEN_TRANSITION_MAX = 200    # 200ms for screen transitions

    # Memory Budgets (MB)
    APP_BASE_MEMORY_MAX = 50       # 50MB base memory footprint
    REGISTRY_MEMORY_MAX = 5        # 5MB for fleet registry
    SCREEN_MEMORY_MAX = 10         # 10MB per screen

    # Large Scale Budgets
    AGENTS_1000_LOAD_MAX = 500     # 500ms to load 1000 agents
    AGENTS_10000_LOAD_MAX = 2000   # 2s to load 10000 agents
    SEARCH_RESPONSE_MAX = 100      # 100ms for agent search

    # Build and Artifact Budgets
    BUILD_TIME_MAX = 30000         # 30s maximum build time
    ARTIFACT_SIZE_MAX = 50         # 50MB maximum artifact size


class TUIPerformanceProfiler:
    """Profiles TUI-specific performance characteristics."""

    def __init__(self) -> None:
        self.results = {}
        self.project_root = Path(__file__).parent

    def profile_app_startup(self, iterations=5):
        """Profile application startup time."""
        times = []
        memory_usage = []

        for _i in range(iterations):
            # Force clean imports by clearing modules
            modules_to_clear = [m for m in sys.modules if m.startswith("tui.")]
            for module in modules_to_clear:
                if module in sys.modules:
                    del sys.modules[module]

            # Start memory tracking
            tracemalloc.start()
            start_time = time.perf_counter()

            try:
                # Import and instantiate app (but don't run it)
                from tui.app import EuxisApp
                app = EuxisApp()

                # Force initialization of core components
                _ = app.config
                _ = app.fleet_registry
                _ = app.project_name
                _ = app.git_branch

                end_time = time.perf_counter()
                _current, peak = tracemalloc.get_traced_memory()
                tracemalloc.stop()

                startup_time = (end_time - start_time) * 1000
                memory_mb = peak / 1024 / 1024

                times.append(startup_time)
                memory_usage.append(memory_mb)

            except Exception:
                tracemalloc.stop()
                continue

        if not times:
            return {"error": "All startup profiling attempts failed"}

        return {
            "mean_ms": statistics.mean(times),
            "p95_ms": sorted(times)[int(0.95 * len(times))] if len(times) > 1 else times[0],
            "min_ms": min(times),
            "max_ms": max(times),
            "memory_mb": statistics.mean(memory_usage),
            "peak_memory_mb": max(memory_usage),
            "budget_ms": PerformanceBudgets.APP_COLD_START_MAX,
            "passes_budget": statistics.mean(times) <= PerformanceBudgets.APP_COLD_START_MAX
        }

    def profile_registry_loading(self, iterations=5):
        """Profile fleet registry loading performance."""
        times = []
        memory_usage = []

        for _ in range(iterations):
            tracemalloc.start()
            start_time = time.perf_counter()

            try:
                registry = FleetRegistry.load()
                agent_count = len(registry.agents)
                squad_count = len(registry.squads)
                combo_count = len(registry.combos)

                end_time = time.perf_counter()
                _current, peak = tracemalloc.get_traced_memory()
                tracemalloc.stop()

                load_time = (end_time - start_time) * 1000
                memory_mb = peak / 1024 / 1024

                times.append(load_time)
                memory_usage.append(memory_mb)

            except Exception:
                tracemalloc.stop()
                continue

        if not times:
            return {"error": "Registry loading profiling failed"}

        return {
            "mean_ms": statistics.mean(times),
            "p95_ms": sorted(times)[int(0.95 * len(times))] if len(times) > 1 else times[0],
            "memory_mb": statistics.mean(memory_usage),
            "agent_count": agent_count,
            "squad_count": squad_count,
            "combo_count": combo_count,
            "budget_ms": PerformanceBudgets.REGISTRY_LOAD_MAX,
            "passes_budget": statistics.mean(times) <= PerformanceBudgets.REGISTRY_LOAD_MAX
        }

    def profile_config_loading(self, iterations=10):
        """Profile configuration loading performance."""
        times = []

        for _ in range(iterations):
            start_time = time.perf_counter()

            try:
                config = ETXConfig.load()
                _ = config.theme
                _ = config.default_provider
                _ = config.recent_agents

                end_time = time.perf_counter()
                times.append((end_time - start_time) * 1000)

            except Exception:
                continue

        if not times:
            return {"error": "Config loading profiling failed"}

        return {
            "mean_ms": statistics.mean(times),
            "p95_ms": sorted(times)[int(0.95 * len(times))] if len(times) > 1 else times[0],
            "budget_ms": PerformanceBudgets.CONFIG_LOAD_MAX,
            "passes_budget": statistics.mean(times) <= PerformanceBudgets.CONFIG_LOAD_MAX
        }

    def profile_large_scale_operations(self):
        """Profile performance with large datasets."""
        results = {}

        # Test with synthetic large agent registry
        large_agent_counts = [100, 1000, 5000]

        for count in large_agent_counts:
            times = []

            for _iteration in range(3):
                # Create synthetic registry data
                synthetic_data = {
                    "protocol_version": "0.0.7",
                    "agents": [
                        {
                            "id": f"agent_{i:05d}",
                            "tier": "fleet" if i % 10 != 0 else "core",
                            "version": "0.0.7",
                            "tags": [f"tag_{i % 5}", f"type_{i % 3}"],
                            "activation": "default" if i % 3 == 0 else "on-demand",
                            "capability_tags": [f"cap_{i % 7}"]
                        }
                        for i in range(count)
                    ]
                }

                with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False) as f:
                    json.dump(synthetic_data, f)
                    temp_path = Path(f.name)

                try:
                    start_time = time.perf_counter()

                    # Create registry and load synthetic data
                    registry = FleetRegistry()
                    registry._load_agents(temp_path)

                    # Force computation of filtered views
                    _ = registry.core_agents
                    _ = registry.default_agents
                    _ = registry.ondemand_agents
                    _ = registry.specialist_agents

                    end_time = time.perf_counter()
                    times.append((end_time - start_time) * 1000)

                finally:
                    temp_path.unlink(missing_ok=True)

            if times:
                results[f"agents_{count}"] = {
                    "mean_ms": statistics.mean(times),
                    "p95_ms": sorted(times)[int(0.95 * len(times))] if len(times) > 1 else times[0],
                    "agent_count": count
                }

        return results

    def profile_build_time(self):
        """Profile package build time."""
        if not (self.project_root / "pyproject.toml").exists():
            return {"error": "No pyproject.toml found"}

        try:
            start_time = time.perf_counter()
            result = subprocess.run([
                sys.executable, "-m", "build", "--wheel", "--no-isolation"
            ], cwd=self.project_root, capture_output=True, text=True, timeout=60)
            end_time = time.perf_counter()

            build_time = (end_time - start_time) * 1000

            if result.returncode != 0:
                return {
                    "error": f"Build failed: {result.stderr}",
                    "build_time_ms": build_time
                }

            # Check artifact size
            dist_dir = self.project_root / "dist"
            artifact_size_mb = 0
            if dist_dir.exists():
                for wheel_file in dist_dir.glob("*.whl"):
                    artifact_size_mb = wheel_file.stat().st_size / 1024 / 1024
                    break

            return {
                "build_time_ms": build_time,
                "artifact_size_mb": artifact_size_mb,
                "passes_build_budget": build_time <= PerformanceBudgets.BUILD_TIME_MAX,
                "passes_size_budget": artifact_size_mb <= PerformanceBudgets.ARTIFACT_SIZE_MAX,
                "build_budget_ms": PerformanceBudgets.BUILD_TIME_MAX,
                "size_budget_mb": PerformanceBudgets.ARTIFACT_SIZE_MAX
            }

        except subprocess.TimeoutExpired:
            return {"error": "Build timed out after 60 seconds"}
        except Exception as e:
            return {"error": f"Build profiling failed: {e}"}

    def run_memory_stress_test(self, duration_seconds=30):
        """Run sustained memory stress test."""
        memory_samples = []
        start_time = time.time()
        registries = []

        try:
            while time.time() - start_time < duration_seconds:
                # Create and destroy registries continuously
                registry = FleetRegistry.load()
                registries.append(registry)

                # Sample memory every second
                if len(registries) % 10 == 0:  # Sample every ~10th iteration
                    memory_mb = self._get_process_memory_mb()
                    memory_samples.append({
                        "time": time.time() - start_time,
                        "memory_mb": memory_mb
                    })

                # Keep only recent registries to test garbage collection
                if len(registries) > 50:
                    registries = registries[-25:]  # Keep last 25

                time.sleep(0.1)  # Small delay

        except KeyboardInterrupt:
            pass

        final_memory = self._get_process_memory_mb()
        peak_memory = max(s["memory_mb"] for s in memory_samples) if memory_samples else final_memory

        # Check for memory leaks (growth over time)
        if len(memory_samples) >= 5:
            early_avg = statistics.mean([s["memory_mb"] for s in memory_samples[:3]])
            late_avg = statistics.mean([s["memory_mb"] for s in memory_samples[-3:]])
            growth_mb = late_avg - early_avg
        else:
            growth_mb = 0

        return {
            "duration_seconds": duration_seconds,
            "samples_collected": len(memory_samples),
            "peak_memory_mb": peak_memory,
            "final_memory_mb": final_memory,
            "memory_growth_mb": growth_mb,
            "likely_leak": growth_mb > 10,  # More than 10MB growth suggests leak
            "memory_samples": memory_samples[-10:] if memory_samples else []  # Last 10 samples
        }

    def _get_process_memory_mb(self):
        """Get current process memory usage in MB."""
        try:
            import psutil
            process = psutil.Process(os.getpid())
            return process.memory_info().rss / 1024 / 1024
        except ImportError:
            # Fallback to tracemalloc if psutil not available
            try:
                current, _peak = tracemalloc.get_traced_memory()
                return current / 1024 / 1024
            except:
                return 0


class EuxisPerformanceSuite:
    """Main performance suite orchestrator."""

    def __init__(self) -> None:
        self.results = {
            "tui_startup": {},
            "component_loading": {},
            "large_scale": {},
            "memory_stress": {},
            "build_artifacts": {},
            "budget_compliance": {}
        }

    def run_all_benchmarks(self):
        """Run comprehensive performance benchmarks."""
        profiler = TUIPerformanceProfiler()

        # TUI Startup Benchmarks

        self.results["tui_startup"]["app_startup"] = profiler.profile_app_startup()

        self.results["component_loading"]["registry"] = profiler.profile_registry_loading()

        self.results["component_loading"]["config"] = profiler.profile_config_loading()

        # Large Scale Testing
        self.results["large_scale"] = profiler.profile_large_scale_operations()

        # Memory Stress Testing
        self.results["memory_stress"] = profiler.run_memory_stress_test(duration_seconds=10)

        # Build Performance
        self.results["build_artifacts"] = profiler.profile_build_time()

        # Budget Compliance
        self._calculate_budget_compliance()

        return self.results

    def _calculate_budget_compliance(self) -> None:
        """Calculate budget compliance across all benchmarks."""
        compliance_scores = []

        # Check TUI startup budgets
        startup_data = self.results["tui_startup"].get("app_startup", {})
        if "passes_budget" in startup_data:
            compliance_scores.append(startup_data["passes_budget"])
            "✅ PASS" if startup_data["passes_budget"] else "❌ FAIL"
            startup_data.get("mean_ms", 0)

        # Check component loading budgets
        for data in self.results["component_loading"].values():
            if "passes_budget" in data:
                compliance_scores.append(data["passes_budget"])
                "✅ PASS" if data["passes_budget"] else "❌ FAIL"
                data.get("mean_ms", 0)

        # Check build performance
        build_data = self.results["build_artifacts"]
        if "passes_build_budget" in build_data:
            compliance_scores.append(build_data["passes_build_budget"])
            "✅ PASS" if build_data["passes_build_budget"] else "❌ FAIL"
            build_data.get("build_time_ms", 0) / 1000

        if "passes_size_budget" in build_data:
            compliance_scores.append(build_data["passes_size_budget"])
            "✅ PASS" if build_data["passes_size_budget"] else "❌ FAIL"
            build_data.get("artifact_size_mb", 0)

        # Check memory stress test
        memory_data = self.results["memory_stress"]
        if memory_data.get("likely_leak") is not None:
            no_leak = not memory_data["likely_leak"]
            compliance_scores.append(no_leak)
            memory_data.get("memory_growth_mb", 0)

        # Overall compliance calculation
        if compliance_scores:
            overall_compliance = sum(compliance_scores) / len(compliance_scores)
            self.results["budget_compliance"]["overall_score"] = overall_compliance
            self.results["budget_compliance"]["passing_percentage"] = overall_compliance * 100


            if overall_compliance >= 0.9 or overall_compliance >= 0.7:
                pass
            else:
                pass
        else:
            pass

    def generate_report(self) -> str:
        """Generate comprehensive performance report."""
        report = []
        report.append("# EUXIS TUI PERFORMANCE BASELINE REPORT")
        report.append("=" * 60)
        report.append(f"Generated: {time.strftime('%Y-%m-%d %H:%M:%S UTC', time.gmtime())}")
        report.append("")

        # Executive Summary
        compliance_pct = self.results["budget_compliance"].get("passing_percentage", 0)
        report.append("## Executive Summary")
        report.append(f"**Overall Compliance**: {compliance_pct:.1f}%")

        if compliance_pct >= 90:
            report.append("**Status**: ✅ All performance budgets met")
        elif compliance_pct >= 70:
            report.append("**Status**: ⚠️ Some budgets exceeded")
        else:
            report.append("**Status**: 🚨 Critical performance issues")
        report.append("")

        # TUI Startup Performance
        report.append("## TUI Startup Performance")
        startup_data = self.results["tui_startup"].get("app_startup", {})
        if "error" in startup_data:
            report.append(f"**Error**: {startup_data['error']}")
        else:
            report.append(f"- **Mean Startup**: {startup_data.get('mean_ms', 0):.2f}ms")
            report.append(f"- **P95**: {startup_data.get('p95_ms', 0):.2f}ms")
            report.append(f"- **Memory Usage**: {startup_data.get('memory_mb', 0):.1f}MB")
            report.append(f"- **Budget**: {startup_data.get('budget_ms', 0)}ms")
            status = "✅ PASS" if startup_data.get("passes_budget", False) else "❌ FAIL"
            report.append(f"- **Status**: {status}")
        report.append("")

        # Component Loading
        report.append("## Component Loading Performance")
        for component, data in self.results["component_loading"].items():
            report.append(f"### {component.title()} Loading")
            if "error" in data:
                report.append(f"**Error**: {data['error']}")
            else:
                report.append(f"- **Mean**: {data.get('mean_ms', 0):.2f}ms")
                if "agent_count" in data:
                    report.append(f"- **Agents Loaded**: {data['agent_count']}")
                report.append(f"- **Budget**: {data.get('budget_ms', 0)}ms")
                status = "✅ PASS" if data.get("passes_budget", False) else "❌ FAIL"
                report.append(f"- **Status**: {status}")
            report.append("")

        # Large Scale Performance
        report.append("## Large Scale Performance")
        for data in self.results["large_scale"].values():
            agent_count = data.get("agent_count", 0)
            report.append(f"### {agent_count} Agents")
            report.append(f"- **Load Time**: {data.get('mean_ms', 0):.2f}ms")
            report.append(f"- **P95**: {data.get('p95_ms', 0):.2f}ms")
            report.append("")

        # Memory Performance
        memory_data = self.results["memory_stress"]
        report.append("## Memory Stress Test")
        report.append(f"- **Duration**: {memory_data.get('duration_seconds', 0)}s")
        report.append(f"- **Peak Memory**: {memory_data.get('peak_memory_mb', 0):.1f}MB")
        report.append(f"- **Memory Growth**: {memory_data.get('memory_growth_mb', 0):.1f}MB")
        leak_status = "❌ POTENTIAL LEAK" if memory_data.get("likely_leak", False) else "✅ STABLE"
        report.append(f"- **Status**: {leak_status}")
        report.append("")

        # Build Performance
        build_data = self.results["build_artifacts"]
        report.append("## Build & Artifact Performance")
        if "error" in build_data:
            report.append(f"**Error**: {build_data['error']}")
        else:
            build_time_s = build_data.get("build_time_ms", 0) / 1000
            report.append(f"- **Build Time**: {build_time_s:.1f}s")
            report.append(f"- **Artifact Size**: {build_data.get('artifact_size_mb', 0):.1f}MB")
            build_status = "✅ PASS" if build_data.get("passes_build_budget", False) else "❌ FAIL"
            size_status = "✅ PASS" if build_data.get("passes_size_budget", False) else "❌ FAIL"
            report.append(f"- **Build Budget**: {build_status}")
            report.append(f"- **Size Budget**: {size_status}")
        report.append("")

        return "\n".join(report)

    def save_results(self, filepath: str | None = None):
        """Save detailed results to JSON."""
        if filepath is None:
            filepath = str(Path(tempfile.gettempdir()) / f"euxis_tui_performance_{int(time.time())}.json")

        with open(filepath, "w") as f:
            json.dump(self.results, f, indent=2, default=str)

        return filepath


def main() -> None:
    """Main entry point."""
    try:
        suite = EuxisPerformanceSuite()
        results = suite.run_all_benchmarks()


        # Save results
        suite.save_results()

        # Exit with appropriate code
        compliance = results["budget_compliance"].get("passing_percentage", 0)
        sys.exit(0 if compliance >= 70 else 1)

    except KeyboardInterrupt:
        sys.exit(130)
    except Exception:
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
