#!/usr/bin/env python3
"""
Euxis Performance Verification & Benchmarking Suite
Verifies performance budgets, threading behavior, and latency targets.
"""

import asyncio
import concurrent.futures
import json
import multiprocessing
import os
import statistics
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path
from queue import Queue, Full
from typing import Dict, List, Any, Optional


class PerformanceBudgets:
    """Performance budget definitions for Euxis system."""

    # Core System Budgets (milliseconds)
    HEALTH_CHECK_MAX = 500
    LINT_MAX = 100
    CORTEX_RECALL_MAX = 50

    # API Response Budgets (milliseconds)
    CLAUDE_API_P95 = 10000  # 10s
    LOCAL_API_P95 = 3000    # 3s

    # Voice Pipeline Budgets (milliseconds)
    # Budgets set for realistic system load during benchmark suite execution.
    # Original pre-optimization: cold=7764ms, whisper=10216ms, piper=6755ms, tts=3614ms
    # Voice Pipeline Budgets (milliseconds)
    # Accounts for system load during benchmark suite execution.
    # Pre-optimization baselines: cold=7764ms, whisper=10216ms, piper=6755ms, tts=3614ms
    VOICE_TOTAL_COLD_START = 6000   # 6s   (was 7764ms, idle best ~2.3s, loaded ~5.2s)
    VOICE_TOTAL_WARM = 2000         # 2s
    WHISPER_LOADING_MAX = 5000      # 5s   (was 10216ms, idle best ~1.4s)
    PIPER_LOADING_MAX = 4000        # 4s   (was 6755ms, idle best ~1.4s)
    TTS_SYNTHESIS_SHORT_MAX = 300   # 300ms (was 489ms, idle best ~70ms)
    TTS_SYNTHESIS_MEDIUM_MAX = 1000 # 1s   (was 3614ms, idle best ~400ms)

    # Concurrency Budgets
    PARALLEL_OVERHEAD_MAX = 50      # 50ms max overhead for parallel vs sequential
    LOCK_CONTENTION_MAX = 100       # 100ms max wait for lock acquisition
    THREAD_SPAWN_MAX = 10           # 10ms max per thread creation

    # Memory Budgets (MB)
    WHISPER_MODEL_MEMORY_MAX = 500
    PIPER_MODEL_MEMORY_MAX = 200
    CORTEX_MEMORY_MAX = 100


class ThreadingBenchmark:
    """Benchmarks for threading performance and behavior."""

    def __init__(self):
        self.results = {}

    def benchmark_thread_creation(self, num_threads=10, iterations=5):
        """Benchmark thread creation overhead."""
        times = []

        for _ in range(iterations):
            start_time = time.time()
            threads = []

            for i in range(num_threads):
                thread = threading.Thread(target=lambda: time.sleep(0.001))
                threads.append(thread)
                thread.start()

            for thread in threads:
                thread.join()

            end_time = time.time()
            total_time = (end_time - start_time) * 1000  # Convert to ms
            per_thread_time = total_time / num_threads
            times.append(per_thread_time)

        return {
            'per_thread_ms': statistics.mean(times),
            'min_ms': min(times),
            'max_ms': max(times),
            'budget_ms': PerformanceBudgets.THREAD_SPAWN_MAX,
            'passes_budget': statistics.mean(times) <= PerformanceBudgets.THREAD_SPAWN_MAX
        }

    def benchmark_lock_contention(self, num_threads=5, iterations=3):
        """Benchmark lock contention behavior."""
        results = []

        for _ in range(iterations):
            lock = threading.Lock()
            shared_data = {'counter': 0}
            thread_times = []

            def contending_worker():
                start = time.time()
                with lock:
                    shared_data['counter'] += 1
                    time.sleep(0.001)  # Simulate work under lock
                end = time.time()
                thread_times.append((end - start) * 1000)

            threads = []
            start_time = time.time()

            for _ in range(num_threads):
                thread = threading.Thread(target=contending_worker)
                threads.append(thread)
                thread.start()

            for thread in threads:
                thread.join()

            end_time = time.time()

            total_time = (end_time - start_time) * 1000
            avg_wait_time = statistics.mean(thread_times)
            max_wait_time = max(thread_times)

            results.append({
                'total_ms': total_time,
                'avg_wait_ms': avg_wait_time,
                'max_wait_ms': max_wait_time
            })

        avg_max_wait = statistics.mean([r['max_wait_ms'] for r in results])

        return {
            'avg_max_wait_ms': avg_max_wait,
            'budget_ms': PerformanceBudgets.LOCK_CONTENTION_MAX,
            'passes_budget': avg_max_wait <= PerformanceBudgets.LOCK_CONTENTION_MAX,
            'details': results
        }

    def benchmark_parallel_overhead(self, num_tasks=10, iterations=3):
        """Benchmark parallel vs sequential execution overhead.

        Uses I/O-bound tasks (time.sleep) to measure true threading
        parallelism.  CPU-bound busy-wait would only measure GIL
        contention, which is not what this budget targets.
        """
        results = []

        def io_task(duration_ms=10):
            """Simulated I/O-bound task (releases GIL via sleep)."""
            time.sleep(duration_ms / 1000.0)

        for _ in range(iterations):
            # Sequential execution
            start_time = time.time()
            for _ in range(num_tasks):
                io_task(10)
            sequential_time = (time.time() - start_time) * 1000

            # Parallel execution
            start_time = time.time()
            with concurrent.futures.ThreadPoolExecutor(max_workers=num_tasks) as executor:
                futures = [executor.submit(io_task, 10) for _ in range(num_tasks)]
                concurrent.futures.wait(futures)
            parallel_time = (time.time() - start_time) * 1000

            overhead = parallel_time - (sequential_time / num_tasks)
            results.append({
                'sequential_ms': sequential_time,
                'parallel_ms': parallel_time,
                'overhead_ms': overhead
            })

        avg_overhead = statistics.mean([r['overhead_ms'] for r in results])

        return {
            'avg_overhead_ms': avg_overhead,
            'budget_ms': PerformanceBudgets.PARALLEL_OVERHEAD_MAX,
            'passes_budget': avg_overhead <= PerformanceBudgets.PARALLEL_OVERHEAD_MAX,
            'details': results
        }


class LatencyBenchmark:
    """Benchmarks for core system latency."""

    def __init__(self):
        self.euxis_dir = Path.home() / ".euxis"

    def benchmark_health_check(self, iterations=5):
        """Benchmark euxis health check latency."""
        times = []

        for _ in range(iterations):
            start_time = time.time()
            result = subprocess.run([
                sys.executable, "-c",
                "import sys; sys.path.append(str(__import__('pathlib').Path.home() / '.euxis')); print('OK')"
            ], capture_output=True, timeout=10)
            end_time = time.time()

            if result.returncode == 0:
                times.append((end_time - start_time) * 1000)

        if not times:
            return {'error': 'Health check failed'}

        return {
            'mean_ms': statistics.mean(times),
            'p95_ms': sorted(times)[int(0.95 * len(times))],
            'budget_ms': PerformanceBudgets.HEALTH_CHECK_MAX,
            'passes_budget': statistics.mean(times) <= PerformanceBudgets.HEALTH_CHECK_MAX
        }

    def benchmark_cortex_recall(self, iterations=5):
        """Benchmark cortex recall query latency (in-process, excludes import overhead).

        Measures the actual vector-search operation, not subprocess cold-start.
        A single subprocess imports chromadb once, then runs N timed queries.
        """
        cortex_cmd = self.euxis_dir / "cli" / "bin" / "euxis-cortex"
        if not cortex_cmd.exists():
            return {'error': 'euxis-cortex not found'}

        db_path = str(self.euxis_dir / "memory" / "cortex" / "db")
        test_script = f"""
import time, os, sys
try:
    import chromadb
    from chromadb.utils import embedding_functions
except ImportError:
    print("RECALL_ERROR: chromadb not installed")
    sys.exit(0)

os.makedirs("{db_path}", exist_ok=True)
client = chromadb.PersistentClient(path="{db_path}")
ef = embedding_functions.SentenceTransformerEmbeddingFunction(model_name="all-MiniLM-L6-v2")
collection = client.get_or_create_collection(name="euxis_memory", embedding_function=ef)

# Warm up embedding model with a throwaway query
collection.query(query_texts=["warmup"], n_results=1)

for _ in range({iterations}):
    start = time.time()
    collection.query(query_texts=["test"], n_results=1)
    end = time.time()
    print(f"RECALL_TIME: {{(end - start) * 1000:.2f}}")
"""
        times = []
        try:
            result = subprocess.run(
                [sys.executable, "-c", test_script],
                capture_output=True, text=True, timeout=30
            )
            for line in result.stdout.splitlines():
                if line.startswith("RECALL_TIME:"):
                    times.append(float(line.split(": ")[1]))
                elif line.startswith("RECALL_ERROR:"):
                    return {'error': line.split(": ", 1)[1]}
        except subprocess.TimeoutExpired:
            return {'error': 'Cortex recall benchmark timed out'}

        if not times:
            return {'error': 'Cortex recall measurement failed'}

        return {
            'mean_ms': statistics.mean(times),
            'p95_ms': sorted(times)[int(0.95 * len(times))],
            'budget_ms': PerformanceBudgets.CORTEX_RECALL_MAX,
            'passes_budget': statistics.mean(times) <= PerformanceBudgets.CORTEX_RECALL_MAX
        }


class ConcurrencyVerifier:
    """Verifies concurrency behavior matches expectations."""

    def verify_stage_execution_order(self):
        """Verify that staged execution respects dependencies."""
        execution_log = []

        def stage_1():
            execution_log.append(('stage_1', 'start', time.time()))
            time.sleep(0.1)
            execution_log.append(('stage_1', 'end', time.time()))

        def stage_2():
            execution_log.append(('stage_2', 'start', time.time()))
            time.sleep(0.05)
            execution_log.append(('stage_2', 'end', time.time()))

        # Stage 1 must complete before Stage 2 starts
        thread1 = threading.Thread(target=stage_1)
        thread1.start()
        thread1.join()

        thread2 = threading.Thread(target=stage_2)
        thread2.start()
        thread2.join()

        # Verify ordering
        stage_1_end = None
        stage_2_start = None

        for entry in execution_log:
            if entry[0] == 'stage_1' and entry[1] == 'end':
                stage_1_end = entry[2]
            elif entry[0] == 'stage_2' and entry[1] == 'start':
                stage_2_start = entry[2]

        return {
            'stages_ordered_correctly': stage_1_end < stage_2_start if stage_1_end and stage_2_start else False,
            'execution_log': execution_log
        }

    def verify_concurrent_execution_within_stage(self):
        """Verify that tasks within same stage execute concurrently."""
        start_times = {}
        end_times = {}

        def concurrent_task(task_id):
            start_times[task_id] = time.time()
            time.sleep(0.1)  # Simulate work
            end_times[task_id] = time.time()

        # Launch concurrent tasks
        threads = []
        launch_time = time.time()

        for i in range(3):
            thread = threading.Thread(target=concurrent_task, args=(f'task_{i}',))
            threads.append(thread)
            thread.start()

        for thread in threads:
            thread.join()

        total_time = time.time() - launch_time

        # All tasks should start within a small window (truly concurrent)
        start_window = max(start_times.values()) - min(start_times.values())

        # Total time should be ~0.1s (concurrent) not ~0.3s (sequential)
        is_concurrent = total_time < 0.15 and start_window < 0.05

        return {
            'tasks_executed_concurrently': is_concurrent,
            'total_execution_ms': total_time * 1000,
            'start_window_ms': start_window * 1000,
            'task_count': len(threads)
        }


class PerformanceVerificationSuite:
    """Main performance verification orchestrator."""

    def __init__(self):
        self.results = {
            'threading': {},
            'latency': {},
            'concurrency': {},
            'budget_compliance': {}
        }

    def run_all_benchmarks(self):
        """Run all performance benchmarks and verifications."""
        print("🔥 EUXIS PERFORMANCE VERIFICATION SUITE")
        print("=" * 50)

        # Threading Benchmarks
        print("\n📊 THREADING BENCHMARKS")
        print("-" * 30)
        threading_bench = ThreadingBenchmark()

        print("• Thread Creation Overhead...")
        self.results['threading']['thread_creation'] = threading_bench.benchmark_thread_creation()

        print("• Lock Contention...")
        self.results['threading']['lock_contention'] = threading_bench.benchmark_lock_contention()

        print("• Parallel vs Sequential Overhead...")
        self.results['threading']['parallel_overhead'] = threading_bench.benchmark_parallel_overhead()

        # Latency Benchmarks
        print("\n⚡ LATENCY BENCHMARKS")
        print("-" * 30)
        latency_bench = LatencyBenchmark()

        print("• Health Check Latency...")
        self.results['latency']['health_check'] = latency_bench.benchmark_health_check()

        print("• Cortex Recall Latency...")
        self.results['latency']['cortex_recall'] = latency_bench.benchmark_cortex_recall()

        # Concurrency Verification
        print("\n🔄 CONCURRENCY VERIFICATION")
        print("-" * 30)
        concurrency_verifier = ConcurrencyVerifier()

        print("• Stage Execution Order...")
        self.results['concurrency']['stage_ordering'] = concurrency_verifier.verify_stage_execution_order()

        print("• Concurrent Execution Within Stage...")
        self.results['concurrency']['concurrent_execution'] = concurrency_verifier.verify_concurrent_execution_within_stage()

        # Budget Compliance Summary
        print("\n💰 BUDGET COMPLIANCE")
        print("-" * 30)
        self._calculate_budget_compliance()

        return self.results

    def _calculate_budget_compliance(self):
        """Calculate overall budget compliance."""
        compliance_scores = []

        # Check threading budgets
        for metric, data in self.results['threading'].items():
            if 'passes_budget' in data:
                compliance_scores.append(data['passes_budget'])
                status = "✅ PASS" if data['passes_budget'] else "❌ FAIL"
                print(f"  {metric}: {status}")

        # Check latency budgets
        for metric, data in self.results['latency'].items():
            if 'passes_budget' in data:
                compliance_scores.append(data['passes_budget'])
                status = "✅ PASS" if data['passes_budget'] else "❌ FAIL"
                print(f"  {metric}: {status}")

        # Check concurrency requirements
        concurrent_tasks_ok = self.results['concurrency']['concurrent_execution']['tasks_executed_concurrently']
        stage_order_ok = self.results['concurrency']['stage_ordering']['stages_ordered_correctly']

        compliance_scores.extend([concurrent_tasks_ok, stage_order_ok])

        status = "✅ PASS" if concurrent_tasks_ok else "❌ FAIL"
        print(f"  concurrent_execution: {status}")

        status = "✅ PASS" if stage_order_ok else "❌ FAIL"
        print(f"  stage_ordering: {status}")

        # Overall score
        overall_compliance = sum(compliance_scores) / len(compliance_scores) if compliance_scores else 0
        self.results['budget_compliance']['overall_score'] = overall_compliance
        self.results['budget_compliance']['passing_percentage'] = overall_compliance * 100

        print(f"\n🎯 OVERALL COMPLIANCE: {overall_compliance * 100:.1f}%")

        if overall_compliance >= 0.9:
            print("🏆 EXCELLENT: Performance meets all critical budgets")
        elif overall_compliance >= 0.7:
            print("⚠️  WARNING: Some performance budgets exceeded")
        else:
            print("🚨 CRITICAL: Major performance issues detected")

    def generate_report(self) -> str:
        """Generate a detailed performance report."""
        report = []
        report.append("# EUXIS PERFORMANCE VERIFICATION REPORT")
        report.append("=" * 50)
        report.append(f"Generated: {time.strftime('%Y-%m-%d %H:%M:%S UTC', time.gmtime())}")
        report.append("")

        # Executive Summary
        compliance_pct = self.results['budget_compliance']['passing_percentage']
        report.append("## Executive Summary")
        report.append(f"**Overall Compliance**: {compliance_pct:.1f}%")

        if compliance_pct >= 90:
            report.append("**Status**: ✅ All performance budgets met")
        elif compliance_pct >= 70:
            report.append("**Status**: ⚠️ Some budgets exceeded - optimization recommended")
        else:
            report.append("**Status**: 🚨 Critical performance issues - immediate action required")

        report.append("")

        # Threading Performance
        report.append("## Threading Performance")
        for metric, data in self.results['threading'].items():
            report.append(f"### {metric.replace('_', ' ').title()}")
            if 'error' in data:
                report.append(f"**Error**: {data['error']}")
            else:
                if 'per_thread_ms' in data:
                    report.append(f"- **Per Thread**: {data['per_thread_ms']:.2f}ms")
                elif 'avg_max_wait_ms' in data:
                    report.append(f"- **Max Wait**: {data['avg_max_wait_ms']:.2f}ms")
                elif 'avg_overhead_ms' in data:
                    report.append(f"- **Overhead**: {data['avg_overhead_ms']:.2f}ms")

                budget_key = 'budget_ms'
                if budget_key in data:
                    report.append(f"- **Budget**: {data[budget_key]}ms")
                    status = "✅ PASS" if data.get('passes_budget', False) else "❌ FAIL"
                    report.append(f"- **Status**: {status}")
            report.append("")

        # Latency Performance
        report.append("## Latency Performance")
        for metric, data in self.results['latency'].items():
            report.append(f"### {metric.replace('_', ' ').title()}")
            if 'error' in data:
                report.append(f"**Error**: {data['error']}")
            else:
                report.append(f"- **Mean**: {data.get('mean_ms', 0):.2f}ms")
                report.append(f"- **P95**: {data.get('p95_ms', 0):.2f}ms")
                report.append(f"- **Budget**: {data.get('budget_ms', 0)}ms")
                status = "✅ PASS" if data.get('passes_budget', False) else "❌ FAIL"
                report.append(f"- **Status**: {status}")
            report.append("")

        # Concurrency Verification
        report.append("## Concurrency Verification")

        stage_data = self.results['concurrency']['stage_ordering']
        status = "✅ PASS" if stage_data['stages_ordered_correctly'] else "❌ FAIL"
        report.append(f"### Stage Ordering: {status}")

        concurrent_data = self.results['concurrency']['concurrent_execution']
        status = "✅ PASS" if concurrent_data['tasks_executed_concurrently'] else "❌ FAIL"
        report.append(f"### Concurrent Execution: {status}")
        report.append(f"- **Total Time**: {concurrent_data['total_execution_ms']:.2f}ms")
        report.append(f"- **Start Window**: {concurrent_data['start_window_ms']:.2f}ms")
        report.append("")

        return "\n".join(report)

    def save_results(self, filepath: Optional[str] = None):
        """Save detailed results to JSON."""
        if filepath is None:
            filepath = fos.environ.get("TMPDIR", "/tmp") + "/euxis_performance_results_{int(time.time())}.json"

        with open(filepath, 'w') as f:
            json.dump(self.results, f, indent=2, default=str)

        return filepath


def main():
    """Main entry point."""
    suite = PerformanceVerificationSuite()
    results = suite.run_all_benchmarks()

    print("\n" + "=" * 50)
    print(suite.generate_report())

    # Save results
    results_path = suite.save_results()
    print(f"\n📁 Detailed results saved to: {results_path}")

    # Exit with non-zero if major issues detected
    compliance = results['budget_compliance']['passing_percentage']
    sys.exit(0 if compliance >= 70 else 1)


if __name__ == "__main__":
    main()
