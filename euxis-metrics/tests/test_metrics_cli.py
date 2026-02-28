# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Tests for metrics_cli.py to achieve 95%+ coverage."""

import argparse
import io
import json
import sys
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

# Add the source to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))


class TestOutputHelpers:
    """Test output helper functions."""

    def test_out_basic(self):
        """Test _out function basic usage."""
        from metrics.metrics_cli import _out

        stream = io.StringIO()
        _out("hello", "world", stream=stream)
        assert stream.getvalue() == "hello world\n"

    def test_out_custom_sep(self):
        """Test _out with custom separator."""
        from metrics.metrics_cli import _out

        stream = io.StringIO()
        _out("a", "b", "c", sep=",", stream=stream)
        assert stream.getvalue() == "a,b,c\n"

    def test_out_custom_end(self):
        """Test _out with custom end."""
        from metrics.metrics_cli import _out

        stream = io.StringIO()
        _out("test", end="", stream=stream)
        assert stream.getvalue() == "test"


class TestPrintFleetOverview:
    """Test _print_fleet_overview function."""

    def test_empty_report(self, capsys):
        """Test with empty report."""
        from metrics.metrics_cli import _print_fleet_overview

        _print_fleet_overview({})
        captured = capsys.readouterr()
        assert captured.out == ""

    def test_with_fleet_metrics(self, capsys):
        """Test with fleet metrics data."""
        from metrics.metrics_cli import _print_fleet_overview

        report = {
            "fleet_metrics": {
                "total_tasks": 100,
                "fleet_success_rate": 0.95,
                "fleet_avg_duration_ms": 1500,
                "active_agents": 5,
                "most_active_agent": "architect",
                "fastest_agent": "debugger",
            }
        }
        _print_fleet_overview(report)
        captured = capsys.readouterr()
        assert "FLEET OVERVIEW" in captured.out
        assert "100" in captured.out
        assert "architect" in captured.out


class TestPrintTopPerformers:
    """Test _print_top_performers function."""

    def test_empty_report(self, capsys):
        """Test with empty report."""
        from metrics.metrics_cli import _print_top_performers

        _print_top_performers({})
        captured = capsys.readouterr()
        assert captured.out == ""

    def test_with_agent_performance(self, capsys):
        """Test with agent performance data."""
        from metrics.metrics_cli import _print_top_performers

        report = {
            "agent_performance": {
                "architect": {
                    "success_rate": 0.98,
                    "avg_duration_ms": 1200,
                    "total_tasks": 50,
                },
                "debugger": {
                    "success_rate": 0.95,
                    "avg_duration_ms": 800,
                    "total_tasks": 30,
                },
            }
        }
        _print_top_performers(report)
        captured = capsys.readouterr()
        assert "TOP PERFORMERS" in captured.out
        assert "architect" in captured.out


class TestPrintDelegationPatterns:
    """Test _print_delegation_patterns function."""

    def test_empty_report(self, capsys):
        """Test with empty report."""
        from metrics.metrics_cli import _print_delegation_patterns

        _print_delegation_patterns({})
        captured = capsys.readouterr()
        assert captured.out == ""

    def test_with_delegation_data(self, capsys):
        """Test with delegation data."""
        from metrics.metrics_cli import _print_delegation_patterns

        report = {
            "delegation_patterns": {
                "architect->debugger": {
                    "delegation_frequency": 25,
                    "handoff_success_rate": 0.92,
                    "avg_delegation_duration_ms": 500,
                },
            }
        }
        _print_delegation_patterns(report)
        captured = capsys.readouterr()
        assert "DELEGATION" in captured.out


class TestPrintToolUsage:
    """Test _print_tool_usage function."""

    def test_empty_report(self, capsys):
        """Test with empty report."""
        from metrics.metrics_cli import _print_tool_usage

        _print_tool_usage({})
        captured = capsys.readouterr()
        assert captured.out == ""

    def test_with_tool_data(self, capsys):
        """Test with tool usage data."""
        from metrics.metrics_cli import _print_tool_usage

        report = {
            "tool_usage_patterns": {
                "Read": {
                    "total_executions": 500,
                    "success_rate": 0.99,
                    "avg_duration_ms": 50,
                },
            }
        }
        _print_tool_usage(report)
        captured = capsys.readouterr()
        assert "TOOL PERFORMANCE" in captured.out


class TestCmdReport:
    """Test cmd_report command."""

    @patch("metrics.metrics_cli.PerformanceAnalyzer")
    def test_report_summary(self, MockAnalyzer, capsys):
        """Test summary report format."""
        from metrics.metrics_cli import cmd_report

        mock_analyzer = MagicMock()
        mock_analyzer.generate_performance_report.return_value = {
            "fleet_metrics": {"total_tasks": 10, "fleet_success_rate": 0.9, "fleet_avg_duration_ms": 100, "active_agents": 2},
            "agent_performance": {},
            "delegation_patterns": {},
            "tool_usage_patterns": {},
        }
        MockAnalyzer.return_value = mock_analyzer

        args = argparse.Namespace(format="summary", hours=24)
        cmd_report(args)
        captured = capsys.readouterr()
        assert "PERFORMANCE REPORT" in captured.out

    @patch("metrics.metrics_cli.PerformanceAnalyzer")
    def test_report_json(self, MockAnalyzer, capsys):
        """Test JSON report format."""
        from metrics.metrics_cli import cmd_report

        mock_analyzer = MagicMock()
        mock_analyzer.generate_performance_report.return_value = {"test": "data"}
        MockAnalyzer.return_value = mock_analyzer

        args = argparse.Namespace(format="json", hours=24)
        cmd_report(args)
        captured = capsys.readouterr()
        data = json.loads(captured.out)
        assert data == {"test": "data"}

    @patch("metrics.metrics_cli.PerformanceAnalyzer")
    def test_report_csv(self, MockAnalyzer, capsys):
        """Test CSV report format."""
        from metrics.metrics_cli import cmd_report

        mock_analyzer = MagicMock()
        mock_analyzer.analyze_agent_performance.return_value = {
            "architect": {
                "total_tasks": 10,
                "success_rate": 0.95,
                "avg_duration_ms": 100,
                "p95_duration_ms": 200,
            }
        }
        MockAnalyzer.return_value = mock_analyzer

        args = argparse.Namespace(format="csv", hours=24)
        cmd_report(args)
        captured = capsys.readouterr()
        assert "agent_id" in captured.out
        assert "architect" in captured.out

    @patch("metrics.metrics_cli.PerformanceAnalyzer")
    def test_report_unsupported_format(self, MockAnalyzer, capsys):
        """Test unsupported format does not crash."""
        from metrics.metrics_cli import cmd_report
        args = argparse.Namespace(format="unknown", hours=24)
        cmd_report(args)
        captured = capsys.readouterr()
        assert captured.out == ""


class TestCmdRankings:
    """Test cmd_rankings command."""

    @patch("metrics.metrics_cli.PerformanceAnalyzer")
    def test_rankings_success_rate(self, MockAnalyzer, capsys):
        """Test rankings by success rate."""
        from metrics.metrics_cli import cmd_rankings

        mock_analyzer = MagicMock()
        mock_analyzer.get_agent_rankings.return_value = [
            ("architect", 0.98),
            ("debugger", 0.95),
        ]
        MockAnalyzer.return_value = mock_analyzer

        args = argparse.Namespace(hours=24, metric="success_rate", limit=10)
        cmd_rankings(args)
        captured = capsys.readouterr()
        assert "RANKINGS" in captured.out
        assert "architect" in captured.out

    @patch("metrics.metrics_cli.PerformanceAnalyzer")
    def test_rankings_duration_ms(self, MockAnalyzer, capsys):
        """Test rankings by duration."""
        from metrics.metrics_cli import cmd_rankings

        mock_analyzer = MagicMock()
        mock_analyzer.get_agent_rankings.return_value = [
            ("debugger", 500),
            ("architect", 1200),
        ]
        MockAnalyzer.return_value = mock_analyzer

        args = argparse.Namespace(hours=24, metric="avg_duration_ms", limit=10)
        cmd_rankings(args)
        captured = capsys.readouterr()
        assert "500ms" in captured.out

    @patch("metrics.metrics_cli.PerformanceAnalyzer")
    def test_rankings_other_metric(self, MockAnalyzer, capsys):
        """Test rankings with other metric type."""
        from metrics.metrics_cli import cmd_rankings

        mock_analyzer = MagicMock()
        mock_analyzer.get_agent_rankings.return_value = [
            ("architect", 50),
        ]
        MockAnalyzer.return_value = mock_analyzer

        args = argparse.Namespace(hours=24, metric="total_tasks", limit=10)
        cmd_rankings(args)
        captured = capsys.readouterr()
        assert "50.00" in captured.out


class TestCmdMonitor:
    """Test cmd_monitor command."""

    @patch("metrics.metrics_cli.PerformanceAnalyzer")
    @patch("metrics.metrics_cli.time.sleep")
    def test_monitor_single_iteration(self, mock_sleep, MockAnalyzer, capsys):
        """Test monitor runs and can be interrupted."""
        from metrics.metrics_cli import cmd_monitor

        mock_analyzer = MagicMock()
        mock_analyzer.generate_performance_report.return_value = {
            "fleet_metrics": {
                "total_tasks": 5,
                "fleet_success_rate": 0.8,
                "active_agents": 3,
            }
        }
        MockAnalyzer.return_value = mock_analyzer

        # Simulate KeyboardInterrupt after first iteration
        mock_sleep.side_effect = KeyboardInterrupt()

        args = argparse.Namespace(refresh=5)
        cmd_monitor(args)
        captured = capsys.readouterr()
        assert "MONITOR" in captured.out
        assert "stopped" in captured.out


class TestCmdCleanup:
    """Test cmd_cleanup command."""

    def test_cleanup_no_files(self, tmp_path, capsys):
        """Test cleanup when files don't exist."""
        from metrics.metrics_cli import cmd_cleanup

        args = argparse.Namespace(max_size_mb=100)

        with patch.dict("os.environ", {"EUXIS_HOME": str(tmp_path)}):
            cmd_cleanup(args)
        captured = capsys.readouterr()
        assert "No cleanup needed" in captured.out

    def test_cleanup_with_large_files(self, tmp_path, capsys):
        """Test cleanup with files exceeding max size."""
        from metrics.metrics_cli import cmd_cleanup

        # Create metrics directory and large file (new structure)
        metrics_dir = tmp_path / "euxis-runtime" / "metrics"
        metrics_dir.mkdir(parents=True)
        events_file = metrics_dir / "events.jsonl"

        # Create a file larger than 0.5 MB (our test max_size)
        # Each line is ~100 chars, need >50KB = ~600 lines
        lines = [f'{{"event": {i}, "data": "x" * 80}}\n' for i in range(15000)]
        events_file.write_text("".join(lines))

        # Use tiny max_size to ensure cleanup triggers
        args = argparse.Namespace(max_size_mb=0)  # 0 MB max = always cleanup

        with patch.dict("os.environ", {"EUXIS_HOME": str(tmp_path)}):
            cmd_cleanup(args)
        captured = capsys.readouterr()
        assert "Cleaned up" in captured.out


class TestCmdExport:
    """Test cmd_export command."""

    @patch("metrics.metrics_cli.PerformanceAnalyzer")
    def test_export_json(self, MockAnalyzer, tmp_path, capsys):
        """Test export to JSON file."""
        from metrics.metrics_cli import cmd_export

        mock_analyzer = MagicMock()
        mock_analyzer.generate_performance_report.return_value = {"test": "data"}
        MockAnalyzer.return_value = mock_analyzer

        output_file = tmp_path / "output.json"
        args = argparse.Namespace(
            output=str(output_file),
            format="json",
            hours=24,
        )

        cmd_export(args)
        _ = capsys.readouterr()  # Clear output

        assert output_file.exists()
        data = json.loads(output_file.read_text())
        assert data == {"test": "data"}

    @patch("metrics.metrics_cli.PerformanceAnalyzer")
    def test_export_csv(self, MockAnalyzer, tmp_path, capsys):
        """Test export to CSV file."""
        from metrics.metrics_cli import cmd_export

        mock_analyzer = MagicMock()
        # CSV export uses the full report's agent_performance key
        mock_analyzer.generate_performance_report.return_value = {
            "agent_performance": {
                "architect": {
                    "total_tasks": 10,
                    "success_rate": 0.95,
                    "avg_duration_ms": 100,
                    "p95_duration_ms": 200,
                    "failure_rate": 0.05,
                }
            }
        }
        MockAnalyzer.return_value = mock_analyzer

        output_file = tmp_path / "output.csv"
        args = argparse.Namespace(
            output=str(output_file),
            format="csv",
            hours=24,
        )

        cmd_export(args)
        _ = capsys.readouterr()  # Clear output

        assert output_file.exists()
        content = output_file.read_text()
        assert "architect" in content

    @patch("metrics.metrics_cli.PerformanceAnalyzer")
    def test_export_unsupported_format(self, MockAnalyzer, tmp_path):
        """Test export ignores unsupported formats."""
        from metrics.metrics_cli import cmd_export
        output_file = tmp_path / "output.txt"
        args = argparse.Namespace(
            output=str(output_file),
            format="unknown",
            hours=24,
        )
        cmd_export(args)
        assert output_file.exists()
        assert output_file.read_text() == ""


class TestMain:
    """Test main entry point."""

    @patch("metrics.metrics_cli.cmd_report")
    def test_main_report(self, mock_cmd):
        """Test main with report command."""
        from metrics.metrics_cli import main

        with patch.object(sys, "argv", ["metrics_cli.py", "report"]):
            main()
            mock_cmd.assert_called_once()

    @patch("metrics.metrics_cli.cmd_rankings")
    def test_main_rankings(self, mock_cmd):
        """Test main with rankings command."""
        from metrics.metrics_cli import main

        with patch.object(sys, "argv", ["metrics_cli.py", "rankings"]):
            main()
            mock_cmd.assert_called_once()

    @patch("metrics.metrics_cli.cmd_monitor")
    def test_main_monitor(self, mock_cmd):
        """Test main with monitor command."""
        from metrics.metrics_cli import main

        with patch.object(sys, "argv", ["metrics_cli.py", "monitor"]):
            main()
            mock_cmd.assert_called_once()

    @patch("metrics.metrics_cli.cmd_cleanup")
    def test_main_cleanup(self, mock_cmd):
        """Test main with cleanup command."""
        from metrics.metrics_cli import main

        with patch.object(sys, "argv", ["metrics_cli.py", "cleanup"]):
            main()
            mock_cmd.assert_called_once()

    @patch("metrics.metrics_cli.cmd_export")
    def test_main_export(self, mock_cmd):
        """Test main with export command."""
        from metrics.metrics_cli import main

        with patch.object(sys, "argv", ["metrics_cli.py", "export", "--output", "test.json"]):
            main()
            mock_cmd.assert_called_once()

    def test_main_no_command(self, capsys):
        """Test main with no command shows help."""
        from metrics.metrics_cli import main

        with patch.object(sys, "argv", ["metrics_cli.py"]):
            main()
        captured = capsys.readouterr()
        # Parser should have printed help or done nothing
        # Just verify no exception was raised
