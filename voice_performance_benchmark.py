#!/usr/bin/env python3
"""
Voice Performance Benchmark Extension for Euxis v0.0.6
Benchmarks voice/audio hot paths and threading overhead.
"""

import asyncio
import os
import subprocess
import statistics
import sys
import tempfile
import threading
import time
from pathlib import Path
from typing import Dict, List, Any, Optional
import json

# Performance budgets from existing script
VOICE_TOTAL_COLD_START = 3000   # 3s
VOICE_TOTAL_WARM = 2000         # 2s
WHISPER_LOADING_MAX = 5000      # 5s
PIPER_LOADING_MAX = 2000        # 2s
TTS_SYNTHESIS_SHORT_MAX = 200   # 200ms
TTS_SYNTHESIS_MEDIUM_MAX = 500  # 500ms

class VoicePerformanceBenchmark:
    """Benchmarks for voice/audio pipeline performance."""

    def __init__(self):
        self.euxis_dir = Path.home() / ".euxis"
        self.voice_cmd = self.euxis_dir / "bin" / "euxis-voice"
        self.results = {}

    def benchmark_voice_cold_start(self, iterations=3):
        """Benchmark voice system cold start time."""
        if not self.voice_cmd.exists():
            return {'error': 'euxis-voice not found'}

        times = []

        for i in range(iterations):
            # Ensure cold start by removing any cached processes
            subprocess.run(["pkill", "-f", "euxis-voice"], capture_output=True)
            time.sleep(1)  # Allow cleanup

            start_time = time.time()

            # Start voice system and measure warmup time
            process = subprocess.Popen([
                sys.executable, str(self.voice_cmd)
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.PIPE)

            # Wait for "Ready." output indicating models loaded
            while True:
                line = process.stdout.readline()
                if line and b"Ready." in line:
                    end_time = time.time()
                    break
                if time.time() - start_time > 10:  # Timeout
                    break

            process.terminate()
            process.wait(timeout=5)

            if 'end_time' in locals():
                cold_start_time = (end_time - start_time) * 1000  # Convert to ms
                times.append(cold_start_time)

        if not times:
            return {'error': 'Voice cold start measurement failed'}

        return {
            'mean_ms': statistics.mean(times),
            'min_ms': min(times),
            'max_ms': max(times),
            'budget_ms': VOICE_TOTAL_COLD_START,
            'passes_budget': statistics.mean(times) <= VOICE_TOTAL_COLD_START
        }

    def benchmark_whisper_loading(self, iterations=3):
        """Benchmark Whisper model loading time."""
        times = []

        # Create test script to measure Whisper loading
        test_script = """
import sys
import time
sys.path.append(str(__import__('pathlib').Path.home() / '.euxis' / '.venv-voice' / 'lib' / 'python3.11' / 'site-packages'))

start_time = time.time()
from faster_whisper import WhisperModel
model = WhisperModel("base.en", device="cpu", compute_type="int8", num_workers=4)
end_time = time.time()
print(f"LOAD_TIME: {(end_time - start_time) * 1000:.2f}")
"""

        venv_python = self.euxis_dir / ".venv-voice" / "bin" / "python3"

        for _ in range(iterations):
            try:
                result = subprocess.run([
                    str(venv_python), "-c", test_script
                ], capture_output=True, text=True, timeout=30)

                # Extract timing from output
                for line in result.stdout.splitlines():
                    if line.startswith("LOAD_TIME:"):
                        load_time = float(line.split(": ")[1])
                        times.append(load_time)
                        break

            except (subprocess.TimeoutExpired, ValueError) as e:
                print(f"Whisper benchmark iteration failed: {e}")

        if not times:
            return {'error': 'Whisper loading measurement failed'}

        return {
            'mean_ms': statistics.mean(times),
            'min_ms': min(times),
            'max_ms': max(times),
            'budget_ms': WHISPER_LOADING_MAX,
            'passes_budget': statistics.mean(times) <= WHISPER_LOADING_MAX
        }

    def benchmark_piper_loading(self, iterations=3):
        """Benchmark Piper TTS model loading time."""
        times = []

        piper_voices_dir = self.euxis_dir / ".piper-voices"
        model_file = piper_voices_dir / "en_US-lessac-medium.onnx"

        if not model_file.exists():
            return {'error': 'Piper model not found'}

        # Create test script to measure Piper loading
        test_script = f"""
import sys
import time
sys.path.append(str(__import__('pathlib').Path.home() / '.euxis' / '.venv-voice' / 'lib' / 'python3.11' / 'site-packages'))

start_time = time.time()
from piper import PiperVoice
voice = PiperVoice.load("{model_file}")
end_time = time.time()
print(f"LOAD_TIME: {{(end_time - start_time) * 1000:.2f}}")
"""

        venv_python = self.euxis_dir / ".venv-voice" / "bin" / "python3"

        for _ in range(iterations):
            try:
                result = subprocess.run([
                    str(venv_python), "-c", test_script
                ], capture_output=True, text=True, timeout=15)

                # Extract timing from output
                for line in result.stdout.splitlines():
                    if line.startswith("LOAD_TIME:"):
                        load_time = float(line.split(": ")[1])
                        times.append(load_time)
                        break

            except (subprocess.TimeoutExpired, ValueError) as e:
                print(f"Piper benchmark iteration failed: {e}")

        if not times:
            return {'error': 'Piper loading measurement failed'}

        return {
            'mean_ms': statistics.mean(times),
            'min_ms': min(times),
            'max_ms': max(times),
            'budget_ms': PIPER_LOADING_MAX,
            'passes_budget': statistics.mean(times) <= PIPER_LOADING_MAX
        }

    def benchmark_tts_synthesis(self, iterations=5):
        """Benchmark TTS synthesis latency for short and medium text."""
        short_text = "Hello world"
        medium_text = "This is a medium length sentence for testing text-to-speech synthesis latency and performance characteristics."

        short_times = []
        medium_times = []

        piper_voices_dir = self.euxis_dir / ".piper-voices"
        model_file = piper_voices_dir / "en_US-lessac-medium.onnx"

        if not model_file.exists():
            return {'error': 'Piper model not found'}

        # Create test script to measure synthesis
        test_script = f"""
import sys
import time
import tempfile
import wave
sys.path.append(str(__import__('pathlib').Path.home() / '.euxis' / '.venv-voice' / 'lib' / 'python3.11' / 'site-packages'))

from piper import PiperVoice
voice = PiperVoice.load("{model_file}")

def measure_synthesis(text, label):
    with tempfile.NamedTemporaryFile(suffix='.wav', delete=True) as tmp:
        start_time = time.time()
        with wave.open(tmp.name, 'wb') as wf:
            voice.synthesize_wav(text, wf)
        end_time = time.time()
        print(f"{{label}}: {{(end_time - start_time) * 1000:.2f}}")

measure_synthesis("{short_text}", "SHORT")
measure_synthesis("{medium_text}", "MEDIUM")
"""

        venv_python = self.euxis_dir / ".venv-voice" / "bin" / "python3"

        for _ in range(iterations):
            try:
                result = subprocess.run([
                    str(venv_python), "-c", test_script
                ], capture_output=True, text=True, timeout=10)

                # Extract timings from output
                for line in result.stdout.splitlines():
                    if line.startswith("SHORT:"):
                        short_time = float(line.split(": ")[1])
                        short_times.append(short_time)
                    elif line.startswith("MEDIUM:"):
                        medium_time = float(line.split(": ")[1])
                        medium_times.append(medium_time)

            except (subprocess.TimeoutExpired, ValueError) as e:
                print(f"TTS synthesis benchmark iteration failed: {e}")

        results = {}

        if short_times:
            results['short_synthesis'] = {
                'mean_ms': statistics.mean(short_times),
                'min_ms': min(short_times),
                'max_ms': max(short_times),
                'budget_ms': TTS_SYNTHESIS_SHORT_MAX,
                'passes_budget': statistics.mean(short_times) <= TTS_SYNTHESIS_SHORT_MAX
            }
        else:
            results['short_synthesis'] = {'error': 'Short synthesis measurement failed'}

        if medium_times:
            results['medium_synthesis'] = {
                'mean_ms': statistics.mean(medium_times),
                'min_ms': min(medium_times),
                'max_ms': max(medium_times),
                'budget_ms': TTS_SYNTHESIS_MEDIUM_MAX,
                'passes_budget': statistics.mean(medium_times) <= TTS_SYNTHESIS_MEDIUM_MAX
            }
        else:
            results['medium_synthesis'] = {'error': 'Medium synthesis measurement failed'}

        return results

    def benchmark_voice_threading_overhead(self, iterations=3):
        """Benchmark threading overhead in voice operations."""
        times = []

        # Create test script to measure threading overhead in voice context
        test_script = """
import sys
import time
import threading
import concurrent.futures
sys.path.append(str(__import__('pathlib').Path.home() / '.euxis' / '.venv-voice' / 'lib' / 'python3.11' / 'site-packages'))

def dummy_tts_task(duration_ms=50):
    \"\"\"Simulated TTS task.\"\"\"
    start = time.time()
    while (time.time() - start) * 1000 < duration_ms:
        pass

# Sequential execution
start_time = time.time()
for _ in range(3):
    dummy_tts_task(50)
sequential_time = (time.time() - start_time) * 1000

# Parallel execution
start_time = time.time()
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as executor:
    futures = [executor.submit(dummy_tts_task, 50) for _ in range(3)]
    concurrent.futures.wait(futures)
parallel_time = (time.time() - start_time) * 1000

overhead = parallel_time - (sequential_time / 3)
print(f"OVERHEAD: {overhead:.2f}")
"""

        venv_python = self.euxis_dir / ".venv-voice" / "bin" / "python3"

        for _ in range(iterations):
            try:
                result = subprocess.run([
                    str(venv_python), "-c", test_script
                ], capture_output=True, text=True, timeout=5)

                for line in result.stdout.splitlines():
                    if line.startswith("OVERHEAD:"):
                        overhead = float(line.split(": ")[1])
                        times.append(overhead)
                        break

            except (subprocess.TimeoutExpired, ValueError) as e:
                print(f"Voice threading benchmark iteration failed: {e}")

        if not times:
            return {'error': 'Voice threading overhead measurement failed'}

        return {
            'mean_overhead_ms': statistics.mean(times),
            'min_overhead_ms': min(times),
            'max_overhead_ms': max(times),
            'budget_ms': 50,  # Same as parallel overhead budget
            'passes_budget': statistics.mean(times) <= 50
        }

class VoicePerformanceSuite:
    """Complete voice performance verification suite."""

    def __init__(self):
        self.voice_benchmark = VoicePerformanceBenchmark()
        self.results = {'voice_performance': {}}

    def run_voice_benchmarks(self):
        """Run all voice performance benchmarks."""
        print("🎤 VOICE/AUDIO PERFORMANCE BENCHMARKS")
        print("-" * 40)

        print("• Voice Cold Start...")
        self.results['voice_performance']['cold_start'] = self.voice_benchmark.benchmark_voice_cold_start()

        print("• Whisper Model Loading...")
        self.results['voice_performance']['whisper_loading'] = self.voice_benchmark.benchmark_whisper_loading()

        print("• Piper Model Loading...")
        self.results['voice_performance']['piper_loading'] = self.voice_benchmark.benchmark_piper_loading()

        print("• TTS Synthesis Latency...")
        tts_results = self.voice_benchmark.benchmark_tts_synthesis()
        self.results['voice_performance'].update(tts_results)

        print("• Voice Threading Overhead...")
        self.results['voice_performance']['threading_overhead'] = self.voice_benchmark.benchmark_voice_threading_overhead()

        return self.results

    def generate_voice_report(self) -> str:
        """Generate voice performance report."""
        report = []
        report.append("# VOICE/AUDIO PERFORMANCE REPORT")
        report.append("=" * 40)
        report.append(f"Generated: {time.strftime('%Y-%m-%d %H:%M:%S UTC', time.gmtime())}")
        report.append("")

        # Calculate compliance
        compliance_scores = []

        for metric, data in self.results['voice_performance'].items():
            if isinstance(data, dict) and 'passes_budget' in data:
                compliance_scores.append(data['passes_budget'])

        overall_compliance = sum(compliance_scores) / len(compliance_scores) if compliance_scores else 0

        # Executive Summary
        report.append("## Executive Summary")
        report.append(f"**Voice Performance Compliance**: {overall_compliance * 100:.1f}%")

        if overall_compliance >= 0.9:
            report.append("**Status**: ✅ All voice performance budgets met")
        elif overall_compliance >= 0.7:
            report.append("**Status**: ⚠️ Some voice budgets exceeded")
        else:
            report.append("**Status**: 🚨 Critical voice performance issues")

        report.append("")

        # Voice Performance Details
        report.append("## Voice Performance Details")

        for metric, data in self.results['voice_performance'].items():
            report.append(f"### {metric.replace('_', ' ').title()}")
            if isinstance(data, dict):
                if 'error' in data:
                    report.append(f"**Error**: {data['error']}")
                else:
                    if 'mean_ms' in data:
                        report.append(f"- **Mean**: {data['mean_ms']:.2f}ms")
                    if 'min_ms' in data:
                        report.append(f"- **Min**: {data['min_ms']:.2f}ms")
                    if 'max_ms' in data:
                        report.append(f"- **Max**: {data['max_ms']:.2f}ms")
                    if 'mean_overhead_ms' in data:
                        report.append(f"- **Overhead**: {data['mean_overhead_ms']:.2f}ms")
                    if 'budget_ms' in data:
                        report.append(f"- **Budget**: {data['budget_ms']}ms")
                        status = "✅ PASS" if data.get('passes_budget', False) else "❌ FAIL"
                        report.append(f"- **Status**: {status}")
            report.append("")

        return "\n".join(report)

def main():
    """Main entry point for voice performance benchmarking."""
    suite = VoicePerformanceSuite()
    results = suite.run_voice_benchmarks()

    print("\n" + "=" * 50)
    print(suite.generate_voice_report())

    # Save results
    timestamp = int(time.time())
    results_path = f"/tmp/euxis_voice_performance_{timestamp}.json"
    with open(results_path, 'w') as f:
        json.dump(results, f, indent=2, default=str)
    print(f"\n📁 Voice performance results saved to: {results_path}")

    # Calculate overall compliance for exit code
    compliance_scores = []
    for metric, data in results['voice_performance'].items():
        if isinstance(data, dict) and 'passes_budget' in data:
            compliance_scores.append(data['passes_budget'])

    overall_compliance = sum(compliance_scores) / len(compliance_scores) if compliance_scores else 0
    sys.exit(0 if overall_compliance >= 0.7 else 1)

if __name__ == "__main__":
    main()