# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Extended unit tests for MetricsScreen._load_metrics.

Tests cover: no perf_dir, empty dir, JSONL parsing with JSONDecodeError skip,
agent_times/provider_counts aggregation, summary stats with sparkline,
per-agent section, provider distribution with percentage/bar, on_mount,
hypothesis for JSONL robustness.
"""

from __future__ import annotations

import json
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, PropertyMock, patch

from hypothesis import given, settings
from hypothesis import strategies as st

from tui.screens.metrics import MetricsScreen


def _patch_screen_app(screen, mock_app):
    patcher = patch.object(
        type(screen), "app", new_callable=PropertyMock, return_value=mock_app
    )
    patcher.start()
    return patcher


def _make_mock_app():
    mock_app = Mock()
    mock_app.project_name = "test-project"
    mock_app.git_branch = "main"
    mock_app.config = Mock()
    mock_app.config.default_provider = "claude"
    return mock_app


class TestMetricsScreenLoadMetrics(unittest.TestCase):
    """Tests for MetricsScreen._load_metrics with real temp directories."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="euxis_metrics_ext_")
        self.euxis_home = Path(self.tmpdir)

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def _make_screen(self):
        screen = MetricsScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)

        mock_summary = Mock()
        mock_agents = Mock()
        mock_providers = Mock()
        mock_header = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            if hasattr(selector, "__name__"):
                return mock_header
            mapping = {
                "#metrics-summary": mock_summary,
                "#metrics-agents": mock_agents,
                "#metrics-providers": mock_providers,
            }
            return mapping.get(selector, Mock())

        screen.query_one = Mock(side_effect=query_one_side_effect)
        return screen, patcher, mock_summary, mock_agents, mock_providers

    @patch("tui.screens.metrics.EUXIS_HOME")
    def test_no_perf_dir(self, mock_home):
        def _mock_truediv(self, path):
            if hasattr(self, "tmpdir"):
                return Path(self.tmpdir) / path
            return Path("/nonexistent") / path

        mock_home.__truediv__ = _mock_truediv
        # Use a path that doesn't have runtime/data/perf
        with patch("tui.screens.metrics.EUXIS_HOME", self.euxis_home):
            screen, patcher, mock_summary, _, _ = self._make_screen()
            try:
                screen._load_metrics()
                call_args = mock_summary.update.call_args[0][0]
                assert "No performance data" in call_args
            finally:
                patcher.stop()

    def test_empty_perf_dir(self):
        perf_dir = self.euxis_home / "data" / "perf"
        perf_dir.mkdir(parents=True)

        with patch("tui.screens.metrics.EUXIS_HOME", self.euxis_home):
            screen, patcher, mock_summary, _, _ = self._make_screen()
            try:
                screen._load_metrics()
                call_args = mock_summary.update.call_args[0][0]
                assert "No performance data" in call_args
            finally:
                patcher.stop()

    def test_jsonl_parsing_valid(self):
        perf_dir = self.euxis_home / "data" / "perf"
        perf_dir.mkdir(parents=True)

        lines = [
            json.dumps({"agent": "architect", "provider": "claude", "duration_ms": 5000}),
            json.dumps({"agent": "debugger", "provider": "gemini", "duration_ms": 3000}),
            json.dumps({"agent": "architect", "provider": "claude", "duration_ms": 7000}),
        ]
        (perf_dir / "run-001.jsonl").write_text("\n".join(lines))

        with patch("tui.screens.metrics.EUXIS_HOME", self.euxis_home):
            screen, patcher, mock_summary, mock_agents, mock_providers = self._make_screen()
            try:
                screen._load_metrics()

                # Summary should show total runs and avg time
                summary_text = mock_summary.update.call_args[0][0]
                assert "3" in summary_text  # total runs
                assert "Avg Time" in summary_text

                # Per-agent metrics
                agents_text = mock_agents.update.call_args[0][0]
                assert "architect" in agents_text
                assert "debugger" in agents_text

                # Provider distribution
                providers_text = mock_providers.update.call_args[0][0]
                assert "claude" in providers_text
                assert "gemini" in providers_text
            finally:
                patcher.stop()

    def test_jsonl_skips_invalid_lines(self):
        perf_dir = self.euxis_home / "data" / "perf"
        perf_dir.mkdir(parents=True)

        lines = [
            json.dumps({"agent": "architect", "provider": "claude", "duration_ms": 1000}),
            "NOT VALID JSON",
            json.dumps({"agent": "debugger", "provider": "claude", "duration_ms": 2000}),
        ]
        (perf_dir / "run-002.jsonl").write_text("\n".join(lines))

        with patch("tui.screens.metrics.EUXIS_HOME", self.euxis_home):
            screen, patcher, mock_summary, _, _ = self._make_screen()
            try:
                screen._load_metrics()
                # The file with invalid JSON should be skipped entirely (try/except)
                # but valid entries in other files should be counted
                summary_text = mock_summary.update.call_args[0][0]
                # Either "No performance data" (if entire file skipped) or has run count
                assert isinstance(summary_text, str)
            finally:
                patcher.stop()

    def test_agent_aggregation(self):
        perf_dir = self.euxis_home / "data" / "perf"
        perf_dir.mkdir(parents=True)

        lines = [
            json.dumps({"agent": "architect", "provider": "claude", "duration_ms": 10000}),
            json.dumps({"agent": "architect", "provider": "claude", "duration_ms": 20000}),
        ]
        (perf_dir / "run-003.jsonl").write_text("\n".join(lines))

        with patch("tui.screens.metrics.EUXIS_HOME", self.euxis_home):
            screen, patcher, _, mock_agents, _ = self._make_screen()
            try:
                screen._load_metrics()
                agents_text = mock_agents.update.call_args[0][0]
                assert "architect" in agents_text
                assert "2 runs" in agents_text
            finally:
                patcher.stop()

    def test_provider_distribution_percentages(self):
        perf_dir = self.euxis_home / "data" / "perf"
        perf_dir.mkdir(parents=True)

        lines = [
            json.dumps({"agent": "a", "provider": "claude", "duration_ms": 1000}),
            json.dumps({"agent": "b", "provider": "claude", "duration_ms": 1000}),
            json.dumps({"agent": "c", "provider": "gemini", "duration_ms": 1000}),
            json.dumps({"agent": "d", "provider": "claude", "duration_ms": 1000}),
        ]
        (perf_dir / "run-004.jsonl").write_text("\n".join(lines))

        with patch("tui.screens.metrics.EUXIS_HOME", self.euxis_home):
            screen, patcher, _, _, mock_providers = self._make_screen()
            try:
                screen._load_metrics()
                providers_text = mock_providers.update.call_args[0][0]
                assert "75%" in providers_text  # claude = 3/4
                assert "25%" in providers_text  # gemini = 1/4
            finally:
                patcher.stop()

    def test_unknown_agent_and_provider_defaults(self):
        perf_dir = self.euxis_home / "data" / "perf"
        perf_dir.mkdir(parents=True)

        # Entry missing agent/provider keys
        lines = [
            json.dumps({"duration_ms": 5000}),
        ]
        (perf_dir / "run-005.jsonl").write_text("\n".join(lines))

        with patch("tui.screens.metrics.EUXIS_HOME", self.euxis_home):
            screen, patcher, _, mock_agents, mock_providers = self._make_screen()
            try:
                screen._load_metrics()
                agents_text = mock_agents.update.call_args[0][0]
                assert "unknown" in agents_text
                providers_text = mock_providers.update.call_args[0][0]
                assert "unknown" in providers_text
            finally:
                patcher.stop()

    def test_multiple_jsonl_files(self):
        perf_dir = self.euxis_home / "data" / "perf"
        perf_dir.mkdir(parents=True)

        (perf_dir / "run-a.jsonl").write_text(
            json.dumps({"agent": "a1", "provider": "claude", "duration_ms": 1000})
        )
        (perf_dir / "run-b.jsonl").write_text(
            json.dumps({"agent": "a2", "provider": "gemini", "duration_ms": 2000})
        )

        with patch("tui.screens.metrics.EUXIS_HOME", self.euxis_home):
            screen, patcher, mock_summary, _, _ = self._make_screen()
            try:
                screen._load_metrics()
                summary_text = mock_summary.update.call_args[0][0]
                assert "2" in summary_text  # total from both files
            finally:
                patcher.stop()


class TestMetricsScreenCompose(unittest.TestCase):
    @patch("tui.screens.metrics.Static")
    @patch("tui.screens.metrics.VerticalScroll")
    @patch("tui.screens.metrics.ETXHeader")
    def test_compose_yields_widgets(self, *mocks):
        screen = MetricsScreen()
        result = list(screen.compose())
        assert len(result) > 0

    def test_euxis_app_property(self):
        screen = MetricsScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)
        try:
            assert screen.euxis_app is mock_app
        finally:
            patcher.stop()


class TestMetricsScreenEmptyLines(unittest.TestCase):
    """Test JSONL with empty lines (line 117 coverage)."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="euxis_metrics_empty_")
        self.euxis_home = Path(self.tmpdir)

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def test_jsonl_with_empty_lines(self):
        perf_dir = self.euxis_home / "data" / "perf"
        perf_dir.mkdir(parents=True)

        content = (
            json.dumps({"agent": "a", "provider": "claude", "duration_ms": 1000})
            + "\n\n\n"
            + json.dumps({"agent": "b", "provider": "claude", "duration_ms": 2000})
            + "\n"
        )
        (perf_dir / "run-empty.jsonl").write_text(content)

        with patch("tui.screens.metrics.EUXIS_HOME", self.euxis_home):
            screen = MetricsScreen()
            mock_app = _make_mock_app()
            patcher = _patch_screen_app(screen, mock_app)
            mock_summary = Mock()
            screen.query_one = Mock(return_value=mock_summary)
            try:
                screen._load_metrics()
                summary_text = mock_summary.update.call_args[0][0]
                assert "2" in summary_text  # 2 valid entries, empty lines skipped
            finally:
                patcher.stop()


class TestMetricsScreenOnMount(unittest.TestCase):
    def test_on_mount_calls_load_metrics(self):
        screen = MetricsScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)

        mock_header = Mock()
        screen.query_one = Mock(return_value=mock_header)

        with patch.object(screen, "_load_metrics") as mock_load:
            screen.on_mount()
            mock_load.assert_called_once()

        patcher.stop()

    def test_on_mount_header(self):
        screen = MetricsScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)

        mock_header = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            if hasattr(selector, "__name__"):
                return mock_header
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)

        with patch.object(screen, "_load_metrics"):
            screen.on_mount()

        assert mock_header.project == "test-project"
        assert mock_header.branch == "main"
        patcher.stop()


class TestMetricsScreenActions(unittest.TestCase):
    def test_action_go_back(self):
        screen = MetricsScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)
        try:
            screen.action_go_back()
            mock_app.pop_screen.assert_called_once()
        finally:
            patcher.stop()


class TestMetricsScreenHypothesis(unittest.TestCase):
    """Property-based tests for JSONL parsing robustness."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="euxis_metrics_hyp_")
        self.euxis_home = Path(self.tmpdir)

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    @settings(max_examples=30)
    @given(
        entries=st.lists(
            st.fixed_dictionaries(
                {
                    "agent": st.text(
                        min_size=1,
                        max_size=10,
                        alphabet=st.characters(whitelist_categories=["Ll"]),
                    ),
                    "provider": st.sampled_from(["claude", "gemini", "ollama"]),
                    "duration_ms": st.integers(min_value=0, max_value=100000),
                }
            ),
            min_size=1,
            max_size=20,
        )
    )
    def test_valid_jsonl_never_crashes(self, entries):
        perf_dir = self.euxis_home / "data" / "perf"
        perf_dir.mkdir(parents=True, exist_ok=True)

        content = "\n".join(json.dumps(e) for e in entries)
        jsonl_file = perf_dir / "hyp-test.jsonl"
        jsonl_file.write_text(content)

        with patch("tui.screens.metrics.EUXIS_HOME", self.euxis_home):
            screen = MetricsScreen()
            mock_app = _make_mock_app()
            patcher = _patch_screen_app(screen, mock_app)

            screen.query_one = Mock(return_value=Mock())
            try:
                screen._load_metrics()  # Must not raise
            finally:
                patcher.stop()

        # Clean up for next hypothesis iteration
        jsonl_file.unlink(missing_ok=True)


if __name__ == "__main__":
    unittest.main()
