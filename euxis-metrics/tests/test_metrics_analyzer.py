# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Extended unit tests for euxis-metrics modules with low coverage.

Covers:
- metrics/__init__.py: get_performance_collector, get_fast_collector
- metrics/collectors/performance_collector.py: PerformanceMetricsCollector
- metrics/collectors/fast_collector.py: FastMetricsCollector, FastMetricsBuffer
- metrics/aggregators/performance_analyzer.py: PerformanceAnalyzer
- metrics/verification/validation_pipeline.py: ValidationPipeline
- metrics/verification/evidence_framework.py: EvidenceFramework
"""

from __future__ import annotations

import asyncio
import json
import os
import time
from datetime import UTC, datetime, timedelta
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

# ============================================================================
# metrics/__init__.py
# ============================================================================


class TestPerformanceAnalyzer:
    """Tests for PerformanceAnalyzer."""

    def _create_events_file(self, metrics_dir, events):
        """Write events to events.jsonl."""
        events_file = metrics_dir / "events.jsonl"
        with events_file.open("w") as f:
            for event in events:
                f.write(json.dumps(event) + "\n")

    def _create_sessions_file(self, metrics_dir, sessions):
        """Write sessions to sessions.jsonl."""
        sessions_file = metrics_dir / "sessions.jsonl"
        with sessions_file.open("w") as f:
            for session in sessions:
                f.write(json.dumps(session) + "\n")

    def test_init_creates_reports_dir(self, tmp_path):
        """__init__ should create reports directory."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        analyzer = PerformanceAnalyzer(str(metrics_dir))
        assert (metrics_dir / "reports").exists()

    def test_load_events_empty(self, tmp_path):
        """_load_events with no file should return empty list."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()
        analyzer = PerformanceAnalyzer(str(metrics_dir))
        assert analyzer._load_events() == []

    def test_load_events_with_data(self, tmp_path):
        """_load_events should load recent events."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()

        now = datetime.now(UTC)
        self._create_events_file(metrics_dir, [
            {"event_type": "test", "timestamp": now.isoformat(), "properties": {}},
            {"event_type": "old", "timestamp": (now - timedelta(hours=48)).isoformat(), "properties": {}},
            {"bad json line": True},  # malformed line won't have timestamp properly
        ])

        analyzer = PerformanceAnalyzer(str(metrics_dir))
        events = analyzer._load_events(hours_back=24)
        assert len(events) == 1
        assert events[0]["event_type"] == "test"

    def test_load_events_with_malformed_json(self, tmp_path):
        """_load_events should skip malformed JSON lines."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()

        events_file = metrics_dir / "events.jsonl"
        events_file.write_text("not json\n{}\n")

        analyzer = PerformanceAnalyzer(str(metrics_dir))
        events = analyzer._load_events()
        assert events == []  # both lines fail (no timestamp key or bad json)

    def test_load_sessions_empty(self, tmp_path):
        """_load_sessions with no file should return empty list."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()
        analyzer = PerformanceAnalyzer(str(metrics_dir))
        assert analyzer._load_sessions() == []

    def test_load_sessions_with_data(self, tmp_path):
        """_load_sessions should load recent sessions."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()

        now = datetime.now(UTC)
        self._create_sessions_file(metrics_dir, [
            {"session_id": "s1", "agent_id": "architect", "completed_at": now.isoformat(),
             "duration_ms": 1000, "status": "SUCCESS", "task_type": "user_request", "priority": "P2"},
        ])

        analyzer = PerformanceAnalyzer(str(metrics_dir))
        sessions = analyzer._load_sessions(hours_back=24)
        assert len(sessions) == 1

    def test_calculate_percentile_empty(self):
        """calculate_percentile on empty list should return 0.0."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        analyzer = PerformanceAnalyzer.__new__(PerformanceAnalyzer)
        assert analyzer.calculate_percentile([], 95) == 0.0

    def test_calculate_percentile_single(self):
        """calculate_percentile on single element should return that element."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        analyzer = PerformanceAnalyzer.__new__(PerformanceAnalyzer)
        assert analyzer.calculate_percentile([42.0], 50) == 42.0

    def test_calculate_percentile_interpolation(self):
        """calculate_percentile should interpolate between elements."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        analyzer = PerformanceAnalyzer.__new__(PerformanceAnalyzer)
        values = [10.0, 20.0, 30.0, 40.0, 50.0]
        p50 = analyzer.calculate_percentile(values, 50)
        assert p50 == pytest.approx(30.0)

    def test_analyze_agent_performance_empty(self, tmp_path):
        """analyze_agent_performance with no sessions should return empty dict."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()
        analyzer = PerformanceAnalyzer(str(metrics_dir))
        assert analyzer.analyze_agent_performance() == {}

    def test_analyze_agent_performance_with_data(self, tmp_path):
        """analyze_agent_performance should compute metrics for each agent."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()

        now = datetime.now(UTC)
        self._create_sessions_file(metrics_dir, [
            {"session_id": "s1", "agent_id": "architect", "completed_at": now.isoformat(),
             "duration_ms": 1000, "status": "SUCCESS", "task_type": "user_request", "priority": "P2"},
            {"session_id": "s2", "agent_id": "architect", "completed_at": now.isoformat(),
             "duration_ms": 2000, "status": "FAILURE", "task_type": "user_request", "priority": "P1"},
            {"session_id": "s3", "agent_id": "architect", "completed_at": now.isoformat(),
             "duration_ms": 1500, "status": "WARNING", "task_type": "delegation", "priority": "P2"},
        ])

        analyzer = PerformanceAnalyzer(str(metrics_dir))
        result = analyzer.analyze_agent_performance(hours_back=24)

        assert "architect" in result
        agent = result["architect"]
        assert agent["total_tasks"] == 3
        assert agent["success_rate"] == pytest.approx(1 / 3)
        assert agent["failure_rate"] == pytest.approx(1 / 3)
        assert agent["warning_rate"] == pytest.approx(1 / 3)
        assert "avg_duration_ms" in agent
        assert "p95_duration_ms" in agent

    def test_analyze_delegation_patterns_empty(self, tmp_path):
        """analyze_delegation_patterns with no events should return empty dict."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()
        analyzer = PerformanceAnalyzer(str(metrics_dir))
        assert analyzer.analyze_delegation_patterns() == {}

    def test_analyze_delegation_patterns_with_data(self, tmp_path):
        """analyze_delegation_patterns should analyze start/complete pairs."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()

        now = datetime.now(UTC)
        self._create_events_file(metrics_dir, [
            {
                "event_type": "Agent:DelegationStarted",
                "timestamp": now.isoformat(),
                "properties": {
                    "correlation_id": "d1",
                    "delegating_agent": "architect",
                    "target_agent": "reviewer",
                    "priority": "P1",
                },
            },
            {
                "event_type": "Agent:DelegationCompleted",
                "timestamp": now.isoformat(),
                "properties": {
                    "correlation_id": "d1",
                    "delegating_agent": "architect",
                    "target_agent": "reviewer",
                    "total_duration_ms": 2000,
                    "handoff_successful": True,
                },
            },
        ])

        analyzer = PerformanceAnalyzer(str(metrics_dir))
        result = analyzer.analyze_delegation_patterns(hours_back=24)
        assert "architect->reviewer" in result
        pair = result["architect->reviewer"]
        assert pair["delegation_frequency"] == 1
        assert pair["handoff_success_rate"] == 1.0

    def test_analyze_tool_usage_patterns_empty(self, tmp_path):
        """analyze_tool_usage_patterns with no events should return empty dict."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()
        analyzer = PerformanceAnalyzer(str(metrics_dir))
        assert analyzer.analyze_tool_usage_patterns() == {}

    def test_analyze_tool_usage_patterns_with_data(self, tmp_path):
        """analyze_tool_usage_patterns should compute tool metrics."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()

        now = datetime.now(UTC)
        self._create_events_file(metrics_dir, [
            {
                "event_type": "Agent:ToolExecution",
                "timestamp": now.isoformat(),
                "properties": {
                    "tool_name": "Read",
                    "agent_id": "architect",
                    "execution_duration_ms": 50,
                    "success": True,
                    "retries": 0,
                },
            },
            {
                "event_type": "Agent:ToolExecution",
                "timestamp": now.isoformat(),
                "properties": {
                    "tool_name": "Read",
                    "agent_id": "architect",
                    "execution_duration_ms": 100,
                    "success": False,
                    "retries": 2,
                },
            },
        ])

        analyzer = PerformanceAnalyzer(str(metrics_dir))
        result = analyzer.analyze_tool_usage_patterns(hours_back=24)
        assert "Read" in result
        assert result["Read"]["total_executions"] == 2
        assert result["Read"]["success_rate"] == 0.5
        assert result["Read"]["avg_retries"] == 2.0

    def test_generate_performance_report_with_data(self, tmp_path):
        """generate_performance_report should include fleet metrics."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()

        now = datetime.now(UTC)
        self._create_sessions_file(metrics_dir, [
            {"session_id": "s1", "agent_id": "architect", "completed_at": now.isoformat(),
             "duration_ms": 1000, "status": "SUCCESS", "task_type": "user_request", "priority": "P2"},
            {"session_id": "s2", "agent_id": "reviewer", "completed_at": now.isoformat(),
             "duration_ms": 500, "status": "SUCCESS", "task_type": "delegation", "priority": "P1"},
        ])

        analyzer = PerformanceAnalyzer(str(metrics_dir))
        report = analyzer.generate_performance_report(hours_back=24)

        assert "fleet_metrics" in report
        fleet = report["fleet_metrics"]
        assert fleet["total_tasks"] == 2
        assert fleet["fleet_success_rate"] == 1.0
        assert fleet["active_agents"] == 2

    def test_generate_performance_report_empty(self, tmp_path):
        """generate_performance_report with no data should not have fleet_metrics."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()
        analyzer = PerformanceAnalyzer(str(metrics_dir))
        report = analyzer.generate_performance_report()
        assert "fleet_metrics" not in report

    def test_save_report(self, tmp_path):
        """save_report should write JSON to reports directory."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()
        analyzer = PerformanceAnalyzer(str(metrics_dir))

        report = {"test": True}
        path = analyzer.save_report(report, "test_report.json")
        assert Path(path).exists()
        with open(path) as f:
            loaded = json.load(f)
        assert loaded["test"] is True

    def test_save_report_auto_filename(self, tmp_path):
        """save_report with no filename should generate timestamp-based name."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()
        analyzer = PerformanceAnalyzer(str(metrics_dir))
        path = analyzer.save_report({"test": True})
        assert "performance_report_" in path

    def test_performance_analyzer_old_and_bad_sessions(self, tmp_path):
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()
        
        # Test old session to hit 69->63 and bad json to hit 71-72
        now = datetime.now(UTC)
        old_time = now - timedelta(hours=48)
        self._create_sessions_file(metrics_dir, [
            {"session_id": "s1", "agent_id": "a", "completed_at": old_time.isoformat(),
             "duration_ms": 100, "status": "SUCCESS", "task_type": "t", "priority": "P2"},
            {"session_id": "s2", "agent_id": "a", "completed_at": now.isoformat(),
             "duration_ms": 100, "status": "UNKNOWN", "task_type": "t", "priority": "P2"}, # hit 121->108
        ])
        # Manually append corrupt JSON
        with open(metrics_dir / "sessions.jsonl", "a") as f:
            f.write("corruptjson\\n")
            
        analyzer = PerformanceAnalyzer(str(metrics_dir))
        # This will also trigger the total_tasks == 0 because I can access the dict with an unknown agent to create an empty struct
        agent_perf = analyzer.analyze_agent_performance(24)
        assert "a" in agent_perf
        assert agent_perf["a"]["total_tasks"] == 1

    def test_performance_analyzer_delegation_edges(self, tmp_path):
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        
        # Write an event file with unmatched completion and unsuccessful handoff
        with open(metrics_dir / "events.jsonl", "w") as f:
            f.write(json.dumps({
                "event_type": "Agent:DelegationCompleted",
                "timestamp": datetime.now(UTC).isoformat().replace("+00:00", "Z"),
                "properties": {
                    "correlation_id": "unmatched",
                    "delegating_agent": "a", "target_agent": "b",
                    "total_duration_ms": 10, "handoff_successful": True, "quality_score": 1
                }
            }) + "\n")
            f.write(json.dumps({
                "event_type": "Agent:DelegationStarted",
                "timestamp": datetime.now(UTC).isoformat().replace("+00:00", "Z"),
                "properties": {"correlation_id": "matched", "delegating_agent": "a", "target_agent": "b", "priority": "P2"}
            }) + "\n")
            f.write(json.dumps({
                "event_type": "Agent:DelegationCompleted",
                "timestamp": datetime.now(UTC).isoformat().replace("+00:00", "Z"),
                "properties": {
                    "correlation_id": "matched",
                    "delegating_agent": "a", "target_agent": "b",
                    "total_duration_ms": 10, "handoff_successful": False, "quality_score": 1 # hits 195->184
                }
            }) + "\n")
            
        analyzer = PerformanceAnalyzer(str(metrics_dir))
        del_pats = analyzer.analyze_delegation_patterns()
        # Log out what's going on for debugging
        # print("EVENTS:", events) # Removed: NameError: name 'events' is not defined
        print("DEL_PATS:", del_pats)
        assert del_pats.get("a->b", {}).get("handoff_success_rate") == 0.0

    def test_module_functions(self, tmp_path):
        from metrics.aggregators.performance_analyzer import generate_daily_report, get_top_performers
        with patch.dict("os.environ", {"EUXIS_HOME": str(tmp_path)}):
            (tmp_path / "euxis-runtime" / "metrics" / "reports").mkdir(parents=True)
            res = generate_daily_report()
            assert "performance_report" in res
            tops = get_top_performers()
            assert tops == []

    def test_get_agent_rankings_empty(self, tmp_path):
        """get_agent_rankings with no data should return empty list."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()
        analyzer = PerformanceAnalyzer(str(metrics_dir))
        assert analyzer.get_agent_rankings() == []

    def test_get_agent_rankings_with_data(self, tmp_path):
        """get_agent_rankings should return sorted tuples."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()

        now = datetime.now(UTC)
        self._create_sessions_file(metrics_dir, [
            {"session_id": "s1", "agent_id": "architect", "completed_at": now.isoformat(),
             "duration_ms": 1000, "status": "SUCCESS", "task_type": "user_request", "priority": "P2"},
            {"session_id": "s2", "agent_id": "reviewer", "completed_at": now.isoformat(),
             "duration_ms": 500, "status": "FAILURE", "task_type": "user_request", "priority": "P2"},
        ])

        analyzer = PerformanceAnalyzer(str(metrics_dir))
        rankings = analyzer.get_agent_rankings(hours_back=24, metric="success_rate")
        assert len(rankings) == 2
        assert rankings[0][0] == "architect"
        assert rankings[0][1] == 1.0

    def test_get_agent_rankings_unknown_metric(self, tmp_path):
        """get_agent_rankings with nonexistent metric should return empty."""
        from metrics.aggregators.performance_analyzer import PerformanceAnalyzer
        metrics_dir = tmp_path / "metrics"
        metrics_dir.mkdir()
        (metrics_dir / "reports").mkdir()

        now = datetime.now(UTC)
        self._create_sessions_file(metrics_dir, [
            {"session_id": "s1", "agent_id": "a", "completed_at": now.isoformat(),
             "duration_ms": 1000, "status": "SUCCESS", "task_type": "t", "priority": "P2"},
        ])

        analyzer = PerformanceAnalyzer(str(metrics_dir))
        rankings = analyzer.get_agent_rankings(hours_back=24, metric="nonexistent")
        assert rankings == []


# ============================================================================
# ValidationPipeline
# ============================================================================


class TestValidationPipeline:
    """Tests for ValidationPipeline."""

    def test_init_default(self, tmp_path):
        """ValidationPipeline should initialize with default framework."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()
            assert pipeline.framework is not None

    def test_extract_quantitative_claims_percentages(self):
        """Should extract percentage claims via success rate pattern."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": "/tmp/test"}):
            pipeline = ValidationPipeline()
        # Use "success rate: 95.5" which matches the rate pattern
        text = "The success rate: 95.5 for all agents."
        claims = pipeline.extract_quantitative_claims(text)
        assert len(claims) >= 1

    def test_extract_quantitative_claims_time(self):
        """Should extract time-based claims."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": "/tmp/test"}):
            pipeline = ValidationPipeline()
        text = "Response time was 250 milliseconds on average."
        claims = pipeline.extract_quantitative_claims(text)
        assert len(claims) >= 1

    def test_extract_quantitative_claims_comparison(self):
        """Should extract comparison claims."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": "/tmp/test"}):
            pipeline = ValidationPipeline()
        text = "The new implementation is 3.5x faster than before."
        claims = pipeline.extract_quantitative_claims(text)
        assert len(claims) >= 1

    def test_extract_quantitative_claims_empty(self):
        """Should return empty list for text without claims."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": "/tmp/test"}):
            pipeline = ValidationPipeline()
        claims = pipeline.extract_quantitative_claims("No numbers here.")
        assert claims == []

    def test_check_citation_requirements_all_cited(self):
        """All cited claims should show full citation count."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": "/tmp/test"}):
            pipeline = ValidationPipeline()
        claims = [
            {"matched_text": "95%", "context": "[E1: measurement in test.py] shows 95% success rate"},
        ]
        result = pipeline.check_citation_requirements("text", claims)
        assert result["cited_claims"] == 1
        assert result["uncited_claims"] == []

    def test_check_citation_requirements_uncited(self):
        """Uncited claims should be reported."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": "/tmp/test"}):
            pipeline = ValidationPipeline()
        claims = [
            {"matched_text": "95%", "context": "The rate is 95% which is good"},
        ]
        result = pipeline.check_citation_requirements("text", claims)
        assert result["cited_claims"] == 0
        assert len(result["uncited_claims"]) == 1

    def test_check_forbidden_terms_none(self):
        """Clean text should have no violations."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": "/tmp/test"}):
            pipeline = ValidationPipeline()
        result = pipeline.check_forbidden_terms("The success rate is exactly 95.5%.")
        assert result["forbidden_terms_found"] == 0

    def test_check_forbidden_terms_found(self):
        """Text with forbidden terms should report violations."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": "/tmp/test"}):
            pipeline = ValidationPipeline()
        result = pipeline.check_forbidden_terms("The rate is approximately 95% and seems fine.")
        assert result["forbidden_terms_found"] >= 2

    def test_validate_evidence_commands_success(self, tmp_path):
        """Evidence with successful verification commands should validate."""
        from metrics.verification.evidence_framework import Evidence, EvidenceGrade
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        evidence = Evidence(
            source_file="test.py", source_line=1,
            evidence_type="measurement", grade=EvidenceGrade.MEASURED,
            content="95.5 percent", timestamp=datetime.now(UTC),
            verification_cmd="echo success", metadata={},
        )

        with patch("metrics.verification.validation_pipeline.subprocess.run") as mock_run:
            mock_run.return_value = MagicMock(returncode=0, stdout="ok", stderr="")
            result = pipeline.validate_evidence_commands([evidence])

        assert result["with_commands"] == 1
        assert result["command_test_results"][0]["success"] is True

    def test_validate_evidence_commands_timeout(self, tmp_path):
        """Evidence with timing out command should report failure."""
        import subprocess
        from metrics.verification.evidence_framework import Evidence, EvidenceGrade
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        evidence = Evidence(
            source_file="test.py", source_line=1,
            evidence_type="measurement", grade=EvidenceGrade.MEASURED,
            content="data", timestamp=datetime.now(UTC),
            verification_cmd="sleep 100", metadata={},
        )

        with patch("metrics.verification.validation_pipeline.subprocess.run") as mock_run:
            mock_run.side_effect = subprocess.TimeoutExpired("sleep", 10)
            result = pipeline.validate_evidence_commands([evidence])

        assert result["command_test_results"][0]["success"] is False
        assert "timeout" in result["command_test_results"][0]["error"].lower()

    def test_validate_evidence_commands_os_error(self, tmp_path):
        """Evidence with OSError command should report failure."""
        from metrics.verification.evidence_framework import Evidence, EvidenceGrade
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        evidence = Evidence(
            source_file="test.py", source_line=1,
            evidence_type="measurement", grade=EvidenceGrade.MEASURED,
            content="data", timestamp=datetime.now(UTC),
            verification_cmd="nonexistent_cmd", metadata={},
        )

        with patch("metrics.verification.validation_pipeline.subprocess.run") as mock_run:
            mock_run.side_effect = OSError("No such file")
            result = pipeline.validate_evidence_commands([evidence])

        assert result["command_test_results"][0]["success"] is False

    def test_validate_evidence_commands_missing_cmd(self, tmp_path):
        """Evidence without verification command should be listed as missing."""
        from metrics.verification.evidence_framework import Evidence, EvidenceGrade
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        evidence = Evidence(
            source_file="test.py", source_line=1,
            evidence_type="observation", grade=EvidenceGrade.OBSERVED,
            content="data", timestamp=datetime.now(UTC),
            verification_cmd=None, metadata={},
        )

        result = pipeline.validate_evidence_commands([evidence])
        assert len(result["missing_commands"]) == 1

    def test_process_analysis_file_missing(self, tmp_path):
        """process_analysis_file with missing file should raise FileNotFoundError."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        with pytest.raises(FileNotFoundError):
            pipeline.process_analysis_file(tmp_path / "nonexistent.txt")

    def test_process_analysis_file_text(self, tmp_path):
        """process_analysis_file with text file should run pipeline."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        text_file = tmp_path / "analysis.txt"
        text_file.write_text("The success rate is 95.5% [E1: measurement in test.py].")

        result = pipeline.process_analysis_file(text_file)
        assert "overall_score" in result
        assert "validation_steps" in result
        assert result["file_path"] == str(text_file)

    def test_process_analysis_file_json(self, tmp_path):
        """process_analysis_file with JSON file should extract text from fields."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        json_file = tmp_path / "analysis.json"
        json_file.write_text(json.dumps({
            "content": "Success rate is 95.5%",
            "analysis": "Performance improved by 2.5x faster than baseline",
        }))

        result = pipeline.process_analysis_file(json_file)
        assert result["validation_steps"]["claim_extraction"]["claims_found"] >= 1

    def test_process_analysis_file_json_no_text_fields(self, tmp_path):
        """process_analysis_file with JSON without known text fields."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        json_file = tmp_path / "data.json"
        json_file.write_text(json.dumps({"unknown_field": "95.5% success"}))

        result = pipeline.process_analysis_file(json_file)
        assert "overall_score" in result

    def test_process_analysis_file_bad_json(self, tmp_path):
        """process_analysis_file with invalid JSON should raise."""
        from metrics.verification.evidence_framework import EvidenceVerificationError
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        json_file = tmp_path / "bad.json"
        json_file.write_text("{bad json")

        with pytest.raises(EvidenceVerificationError):
            pipeline.process_analysis_file(json_file)

    def test_calculate_validation_score_perfect(self, tmp_path):
        """Perfect pipeline result should score 1.0."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        result = {
            "validation_steps": {
                "citation_check": {"total_claims": 2, "cited_claims": 2},
                "forbidden_terms": {"forbidden_terms_found": 0},
            },
        }
        score = pipeline._calculate_validation_score(result)
        assert score == pytest.approx(1.0)

    def test_calculate_validation_score_with_penalties(self, tmp_path):
        """Pipeline result with issues should have reduced score."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        result = {
            "validation_steps": {
                "citation_check": {"total_claims": 4, "cited_claims": 2},
                "forbidden_terms": {"forbidden_terms_found": 3},
            },
        }
        score = pipeline._calculate_validation_score(result)
        assert 0.0 < score < 1.0

    def test_calculate_validation_score_with_evidence(self, tmp_path):
        """Pipeline result with evidence validation should incorporate it."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        result = {
            "validation_steps": {
                "citation_check": {"total_claims": 1, "cited_claims": 1},
                "forbidden_terms": {"forbidden_terms_found": 0},
                "evidence_validation": {
                    "total_evidence": 2,
                    "with_commands": 2,
                    "command_test_results": [
                        {"success": True},
                        {"success": False},
                    ],
                },
            },
        }
        score = pipeline._calculate_validation_score(result)
        assert 0.0 < score < 1.0

    def test_calculate_validation_score_no_claims(self, tmp_path):
        """Pipeline result with no claims should score 1.0 (no penalties)."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        result = {
            "validation_steps": {
                "citation_check": {"total_claims": 0, "cited_claims": 0},
                "forbidden_terms": {"forbidden_terms_found": 0},
            },
        }
        score = pipeline._calculate_validation_score(result)
        assert score == pytest.approx(1.0)

    def test_extract_embedded_evidence(self, tmp_path):
        """_extract_embedded_evidence should parse embedded JSON evidence."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        import json as json_mod
        evidence_data = json_mod.dumps({
            "evidence": [
                {"source_file": "test.py", "evidence_type": "measurement",
                 "grade": "E2", "content": "50ms"}
            ]
        })
        text = f"some text before {evidence_data} some text after"
        evidence_list = pipeline._extract_embedded_evidence(text)

        assert len(evidence_list) == 1
        assert evidence_list[0].source_file == "test.py"

    def test_extract_embedded_evidence_invalid(self, tmp_path):
        """_extract_embedded_evidence should skip invalid JSON blocks."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        evidence_list = pipeline._extract_embedded_evidence('{"evidence": "not a list"} and {"evidence": ')
        assert evidence_list == []

    def test_generate_validation_report_all_passed(self, tmp_path):
        """Validation report with all passed files should have no recommendations."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        results = [
            {
                "passed": True,
                "overall_score": 0.95,
                "validation_steps": {
                    "claim_extraction": {"claims_found": 2},
                    "citation_check": {"cited_claims": 2},
                    "forbidden_terms": {"forbidden_terms_found": 0},
                },
            },
        ]
        report = pipeline.generate_validation_report(results)
        assert report["summary"]["files_passed"] == 1
        assert report["summary"]["files_failed"] == 0

    def test_generate_validation_report_with_failures(self, tmp_path):
        """Validation report with failures should include recommendations."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        results = [
            {
                "passed": False,
                "overall_score": 0.5,
                "validation_steps": {
                    "claim_extraction": {"claims_found": 5},
                    "citation_check": {"cited_claims": 2},
                    "forbidden_terms": {"forbidden_terms_found": 3},
                },
            },
        ]
        report = pipeline.generate_validation_report(results)
        assert report["summary"]["files_failed"] == 1
        assert len(report["recommendations"]) >= 1

    def test_generate_validation_report_empty(self, tmp_path):
        """Validation report with no results should work."""
        from metrics.verification.validation_pipeline import ValidationPipeline
        with patch.dict(os.environ, {"EUXIS_HOME": str(tmp_path)}):
            pipeline = ValidationPipeline()

        report = pipeline.generate_validation_report([])
        assert report["summary"]["average_score"] == 0


# ============================================================================
# EvidenceFramework
# ============================================================================


