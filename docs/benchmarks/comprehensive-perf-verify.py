#!/usr/bin/env python3
"""Comprehensive Performance Verification for Euxis v0.0.6 Release
Combines existing performance benchmarks with voice/audio performance verification.
"""

import json
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path


class ComprehensivePerformanceVerification:
    """Orchestrates both general and voice performance verification."""

    def __init__(self) -> None:
        self.results = {
            "verification_timestamp": time.strftime("%Y-%m-%d %H:%M:%S UTC", time.gmtime()),
            "euxis_version": "0.0.6",
            "general_performance": {},
            "voice_performance": {},
            "overall_compliance": {}
        }

    def run_general_benchmarks(self) -> None:
        """Run the existing performance verification suite."""
        try:
            subprocess.run([
                sys.executable, "performance-verification.py"
            ], capture_output=True, text=True, cwd=Path.cwd())

            # Extract results from the JSON output file
            # The script saves to /tmp/euxis_performance_results_{timestamp}.json
            import glob
            latest_file = max(glob.glob(str(Path(tempfile.gettempdir()) / "euxis_performance_results_*.json")), key=os.path.getmtime)

            with open(latest_file) as f:
                self.results["general_performance"] = json.load(f)


        except Exception as e:
            self.results["general_performance"] = {"error": str(e)}

    def run_voice_benchmarks(self) -> None:
        """Run voice/audio performance benchmarks."""
        try:
            subprocess.run([
                sys.executable, "voice-perf-benchmark.py"
            ], capture_output=True, text=True, cwd=Path.cwd())

            # Extract results from the JSON output file
            import glob
            latest_file = max(glob.glob(str(Path(tempfile.gettempdir()) / "euxis_voice_performance_*.json")), key=os.path.getmtime)

            with open(latest_file) as f:
                voice_data = json.load(f)
                self.results["voice_performance"] = voice_data["voice_performance"]


        except Exception as e:
            self.results["voice_performance"] = {"error": str(e)}

    def calculate_overall_compliance(self):
        """Calculate overall performance compliance across all benchmarks."""
        all_scores = []

        # General performance scores
        general_data = self.results.get("general_performance", {})
        if "threading" in general_data:
            for data in general_data["threading"].values():
                if isinstance(data, dict) and "passes_budget" in data:
                    all_scores.append(1.0 if data["passes_budget"] else 0.0)

        if "latency" in general_data:
            for data in general_data["latency"].values():
                if isinstance(data, dict) and "passes_budget" in data:
                    all_scores.append(1.0 if data["passes_budget"] else 0.0)

        if "concurrency" in general_data:
            concurrent_data = general_data["concurrency"]
            if "concurrent_execution" in concurrent_data:
                all_scores.append(1.0 if concurrent_data["concurrent_execution"].get("tasks_executed_concurrently", False) else 0.0)
            if "stage_ordering" in concurrent_data:
                all_scores.append(1.0 if concurrent_data["stage_ordering"].get("stages_ordered_correctly", False) else 0.0)

        # Voice performance scores
        voice_data = self.results.get("voice_performance", {})
        for data in voice_data.values():
            if isinstance(data, dict) and "passes_budget" in data:
                all_scores.append(1.0 if data["passes_budget"] else 0.0)

        overall_compliance = sum(all_scores) / len(all_scores) if all_scores else 0.0

        self.results["overall_compliance"] = {
            "score": overall_compliance,
            "percentage": overall_compliance * 100,
            "total_metrics": len(all_scores),
            "passing_metrics": sum(all_scores)
        }

        return overall_compliance

    def generate_executive_summary(self) -> str:
        """Generate executive summary of all performance verification results."""
        report = []
        report.append("# EUXIS v0.0.6 PERFORMANCE VERIFICATION SUMMARY")
        report.append("=" * 60)
        report.append(f"**Verification Time**: {self.results['verification_timestamp']}")

        overall_compliance = self.results["overall_compliance"]["percentage"]
        passing_metrics = self.results["overall_compliance"]["passing_metrics"]
        total_metrics = self.results["overall_compliance"]["total_metrics"]

        report.append("")
        report.append("## 🎯 OVERALL PERFORMANCE COMPLIANCE")
        report.append(f"**Score**: {overall_compliance:.1f}% ({passing_metrics}/{total_metrics} metrics passing)")

        if overall_compliance >= 90:
            status = "🏆 EXCELLENT"
            recommendation = "Ready for release - all critical performance budgets met"
        elif overall_compliance >= 70:
            status = "⚠️ WARNING"
            recommendation = "Performance optimization recommended before release"
        elif overall_compliance >= 50:
            status = "🚨 CRITICAL"
            recommendation = "Significant performance issues - release blocked pending optimization"
        else:
            status = "💥 FAILURE"
            recommendation = "Major performance failures - immediate remediation required"

        report.append(f"**Status**: {status}")
        report.append(f"**Recommendation**: {recommendation}")

        # Critical Issues Summary
        report.append("")
        report.append("## 🔍 CRITICAL ISSUES IDENTIFIED")

        critical_issues = []

        # Check voice performance issues
        voice_data = self.results.get("voice_performance", {})
        for metric, data in voice_data.items():
            if isinstance(data, dict) and not data.get("passes_budget", True):
                if "mean_ms" in data and "budget_ms" in data:
                    overage_pct = ((data["mean_ms"] / data["budget_ms"]) - 1) * 100
                    critical_issues.append(f"**Voice {metric}**: {data['mean_ms']:.0f}ms vs {data['budget_ms']}ms budget (+{overage_pct:.0f}% over)")

        # Check general performance issues
        general_data = self.results.get("general_performance", {})
        for category in ["threading", "latency"]:
            if category in general_data:
                for metric, data in general_data[category].items():
                    if isinstance(data, dict) and not data.get("passes_budget", True):
                        if "mean_ms" in data and "budget_ms" in data:
                            overage_pct = ((data["mean_ms"] / data["budget_ms"]) - 1) * 100
                            critical_issues.append(f"**{category.title()} {metric}**: {data['mean_ms']:.0f}ms vs {data['budget_ms']}ms budget (+{overage_pct:.0f}% over)")
                        elif "avg_overhead_ms" in data and "budget_ms" in data:
                            overage_pct = ((data["avg_overhead_ms"] / data["budget_ms"]) - 1) * 100
                            critical_issues.append(f"**{category.title()} {metric}**: {data['avg_overhead_ms']:.0f}ms vs {data['budget_ms']}ms budget (+{overage_pct:.0f}% over)")

        if critical_issues:
            for issue in critical_issues[:10]:  # Show top 10 issues
                report.append(f"- {issue}")
        else:
            report.append("- No critical performance issues detected")

        # Performance Budget Status
        report.append("")
        report.append("## 📊 PERFORMANCE BUDGET STATUS")

        budget_categories = {
            "Voice Pipeline": ["cold_start", "whisper_loading", "piper_loading", "short_synthesis", "medium_synthesis"],
            "Threading": ["thread_creation", "lock_contention", "parallel_overhead"],
            "Core Latency": ["health_check", "cortex_recall"],
            "Concurrency": ["concurrent_execution", "stage_ordering"]
        }

        for category, metrics in budget_categories.items():
            passing_count = 0
            total_count = 0

            for metric in metrics:
                # Check voice performance
                if metric in voice_data and isinstance(voice_data[metric], dict):
                    total_count += 1
                    if voice_data[metric].get("passes_budget", False):
                        passing_count += 1

                # Check general performance
                for perf_category in ["threading", "latency", "concurrency"]:
                    if perf_category in general_data and metric in general_data[perf_category]:
                        data = general_data[perf_category][metric]
                        total_count += 1
                        if isinstance(data, dict):
                            if data.get("passes_budget", False) or data.get("tasks_executed_concurrently", False) or data.get("stages_ordered_correctly", False):
                                passing_count += 1

            if total_count > 0:
                pct = (passing_count / total_count) * 100
                status_icon = "✅" if pct >= 80 else "⚠️" if pct >= 50 else "❌"
                report.append(f"**{category}**: {status_icon} {passing_count}/{total_count} ({pct:.0f}%)")

        return "\n".join(report)

    def save_comprehensive_results(self):
        """Save comprehensive results to JSON file."""
        timestamp = int(time.time())
        filepath = f"/tmp/euxis_comprehensive_performance_{timestamp}.json"

        with open(filepath, "w") as f:
            json.dump(self.results, f, indent=2, default=str)

        return filepath

def main() -> None:
    """Main verification orchestrator."""
    verifier = ComprehensivePerformanceVerification()

    # Run all benchmarks
    verifier.run_general_benchmarks()
    verifier.run_voice_benchmarks()

    # Calculate overall compliance
    overall_compliance = verifier.calculate_overall_compliance()

    # Generate and display executive summary

    # Save comprehensive results
    verifier.save_comprehensive_results()

    # Exit with appropriate code for CI/CD
    if overall_compliance >= 0.7:
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
