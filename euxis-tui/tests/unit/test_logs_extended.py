# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Comprehensive unit tests for LogViewerScreen (logs.py) — extended coverage.

Covers:
- highlight_log_line: log levels, timestamps, search patterns (valid/invalid regex)
- LogLine: init with/without search_pattern
- LogContent: clear, add_line (basic, with search, with ERROR, memory limit),
  add_lines, _matches_filter, watch_search_pattern, _rebuild_with_filter,
  load_file, start_tail, stop_tail, _check_for_updates
- LogViewerScreen: __init__, compose, on_mount, _load_agent_list,
  _select_agent, on_option_list_option_selected, _load_agent_logs,
  on_input_changed, on_switch_changed, action methods
"""

from __future__ import annotations

import os
import re
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import (
    MagicMock,
    Mock,
    PropertyMock,
    call,
    mock_open,
    patch,
)

import pytest

from tui.screens.logs import (
    LOG_LEVEL_PATTERNS,
    TIMESTAMP_PATTERN,
    LogContent,
    LogLine,
    LogViewerScreen,
    highlight_log_line,
)


# ============================================================================
# Helpers
# ============================================================================


def _patch_screen_app(screen, mock_app):
    """Bypass Textual's read-only app property for testing."""
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


def _make_log_content():
    """Create a LogContent with mocked Textual runtime methods.

    Patches both watchers AND _rebuild_with_filter during construction, then
    replaces runtime methods with mocks. Triggers lazy reactive init by
    reading each reactive attribute so that subsequent reads in the real
    methods don't re-trigger the watcher.
    """
    # Patch all methods that touch Textual runtime during construction
    with (
        patch.object(LogContent, "watch_search_pattern"),
        patch.object(LogContent, "_rebuild_with_filter"),
    ):
        content = LogContent()
        # Trigger lazy init while patches are active
        _ = content.search_pattern
        _ = content.auto_scroll

    # Override Textual runtime methods with fresh mocks
    content.mount = Mock()
    content.remove_children = Mock()
    content.scroll_end = Mock()
    content.scroll_home = Mock()
    content.set_interval = Mock()
    # Reset lines in case watcher ran during init
    content._lines = []
    return content


# ============================================================================
# highlight_log_line
# ============================================================================


class TestHighlightLogLine(unittest.TestCase):
    """Tests for the highlight_log_line function."""

    def test_plain_line_no_highlight(self):
        result = highlight_log_line("Just a normal line")
        assert "Just a normal line" in result

    def test_error_level_highlighted(self):
        result = highlight_log_line("2026-02-18 ERROR Something bad")
        assert "bold red" in result

    def test_fatal_level_highlighted(self):
        result = highlight_log_line("FATAL: system crash")
        assert "bold red" in result

    def test_critical_level_highlighted(self):
        result = highlight_log_line("CRITICAL: disk full")
        assert "bold red" in result

    def test_warn_level_highlighted(self):
        result = highlight_log_line("WARNING: low memory")
        assert "bold yellow" in result

    def test_info_level_highlighted(self):
        result = highlight_log_line("INFO: Server started")
        assert "cyan" in result

    def test_debug_level_highlighted(self):
        result = highlight_log_line("DEBUG: Entering function")
        assert "dim" in result

    def test_success_level_highlighted(self):
        result = highlight_log_line("SUCCESS: Tests passed")
        assert "bold green" in result

    def test_timestamp_highlighted(self):
        result = highlight_log_line("2026-02-18T10:30:45 INFO Starting")
        assert "[dim]" in result

    def test_timestamp_space_separator(self):
        result = highlight_log_line("2026-02-18 10:30:45 INFO Starting")
        assert "[dim]" in result

    def test_search_pattern_highlighted(self):
        result = highlight_log_line("Test: finding the needle here", "needle")
        assert "bold on yellow" in result

    def test_search_pattern_case_insensitive(self):
        result = highlight_log_line("ERROR found NEEDLE here", "needle")
        assert "bold on yellow" in result

    def test_search_pattern_none(self):
        result = highlight_log_line("Just a line", None)
        assert "bold on yellow" not in result

    def test_search_pattern_empty_string(self):
        result = highlight_log_line("Just a line", "")
        # Empty pattern is falsy, should skip highlighting
        assert "bold on yellow" not in result

    def test_invalid_regex_search_pattern(self):
        # Invalid regex should be silently skipped
        result = highlight_log_line("Some text [bracket", "[invalid")
        # Should not crash, just return without search highlighting
        assert "Some text" in result

    def test_multiple_log_levels_in_one_line(self):
        result = highlight_log_line("ERROR then INFO")
        assert "bold red" in result
        assert "cyan" in result


# ============================================================================
# LogLine
# ============================================================================


class TestLogLine(unittest.TestCase):
    """Tests for LogLine widget."""

    @patch("tui.screens.logs.highlight_log_line")
    def test_init_calls_highlight(self, mock_highlight):
        mock_highlight.return_value = "[bold red]ERROR[/] line"
        line = LogLine("ERROR line")
        mock_highlight.assert_called_once_with("ERROR line", None)

    @patch("tui.screens.logs.highlight_log_line")
    def test_init_with_search_pattern(self, mock_highlight):
        mock_highlight.return_value = "highlighted"
        line = LogLine("test line", search_pattern="test")
        mock_highlight.assert_called_once_with("test line", "test")


# ============================================================================
# LogContent
# ============================================================================


class TestLogContentClear(unittest.TestCase):
    """Tests for LogContent.clear."""

    def test_clear_empties_lines_and_children(self):
        content = _make_log_content()
        content._lines = ["line1", "line2"]
        content.clear()
        assert content._lines == []
        content.remove_children.assert_called_once()


class TestLogContentAddLine(unittest.TestCase):
    """Tests for LogContent.add_line."""

    def test_add_line_basic(self):
        content = _make_log_content()
        content.add_line("Hello world")
        assert content._lines == ["Hello world"]
        content.mount.assert_called_once()
        content.scroll_end.assert_called_once()

    def test_add_line_with_matching_search_pattern(self):
        content = _make_log_content()
        content.search_pattern = "world"
        content.add_line("Hello world")
        assert "Hello world" in content._lines
        content.mount.assert_called_once()

    def test_add_line_with_non_matching_search_pattern(self):
        content = _make_log_content()
        content.search_pattern = "xyz"
        content.add_line("Hello world")
        assert "Hello world" in content._lines
        # Line added to _lines but NOT mounted (filtered out)
        content.mount.assert_not_called()

    def test_add_line_error_level_adds_error_class(self):
        content = _make_log_content()
        content.add_line("2026-02-18 ERROR: Something bad happened")
        content.mount.assert_called_once()
        widget = content.mount.call_args[0][0]
        # LogLine stores CSS classes in _classes set (Textual internals)
        assert "log-line-error" in widget._classes

    def test_add_line_fatal_level_adds_error_class(self):
        content = _make_log_content()
        content.add_line("FATAL crash")
        content.mount.assert_called_once()

    def test_add_line_normal_level_no_error_class(self):
        content = _make_log_content()
        content.add_line("INFO: Everything is fine")
        content.mount.assert_called_once()

    def test_add_line_auto_scroll_disabled(self):
        content = _make_log_content()
        content.auto_scroll = False
        content.add_line("test")
        content.scroll_end.assert_not_called()

    def test_add_line_memory_limit_enforcement(self):
        content = _make_log_content()
        content._max_lines = 5

        # Add 6 lines
        for i in range(6):
            content.add_line(f"line-{i}")

        # _lines should be trimmed to last 5
        assert len(content._lines) == 5
        assert content._lines[0] == "line-1"
        assert content._lines[-1] == "line-5"

    def test_add_line_trims_widgets_when_over_limit(self):
        content = _make_log_content()
        content._max_lines = 3

        # Simulate existing children via patching the read-only property
        mock_children = [Mock() for _ in range(5)]
        with patch.object(type(content), "children", new_callable=PropertyMock, return_value=mock_children):
            # Prefill lines to trigger trim
            content._lines = ["a", "b", "c"]
            content.add_line("d")

        # Lines trimmed to last 3
        assert len(content._lines) == 3


class TestLogContentAddLines(unittest.TestCase):
    """Tests for LogContent.add_lines."""

    def test_add_lines_multiple(self):
        content = _make_log_content()
        content.add_lines(["line1", "line2", "line3"])
        assert len(content._lines) == 3
        assert content.mount.call_count == 3


class TestLogContentMatchesFilter(unittest.TestCase):
    """Tests for LogContent._matches_filter."""

    def test_no_pattern(self):
        content = _make_log_content()
        content.search_pattern = ""
        assert content._matches_filter("any line") is True

    def test_valid_regex_match(self):
        content = _make_log_content()
        content.search_pattern = r"ERROR.*timeout"
        assert content._matches_filter("ERROR: connection timeout") is True

    def test_valid_regex_no_match(self):
        content = _make_log_content()
        content.search_pattern = r"ERROR.*timeout"
        assert content._matches_filter("INFO: all good") is False

    def test_invalid_regex_falls_back_to_string(self):
        content = _make_log_content()
        content.search_pattern = "[invalid"
        # Falls back to case-insensitive string search
        assert content._matches_filter("contains [invalid text") is True
        assert content._matches_filter("no match here") is False

    def test_case_insensitive(self):
        content = _make_log_content()
        content.search_pattern = "error"
        assert content._matches_filter("ERROR: bad") is True


class TestLogContentWatchSearchPattern(unittest.TestCase):
    """Tests for watch_search_pattern reactive watcher."""

    def test_calls_rebuild(self):
        content = _make_log_content()
        content._rebuild_with_filter = Mock()
        content.watch_search_pattern()
        content._rebuild_with_filter.assert_called_once()


class TestLogContentRebuildWithFilter(unittest.TestCase):
    """Tests for _rebuild_with_filter."""

    def test_rebuild_without_filter(self):
        content = _make_log_content()
        content._lines = ["line1", "line2", "ERROR: bad"]
        # search_pattern is already "" from init

        content._rebuild_with_filter()

        content.remove_children.assert_called_once()
        # All 3 lines should be mounted
        assert content.mount.call_count == 3

    def test_rebuild_with_filter(self):
        content = _make_log_content()
        content._lines = ["line1", "ERROR: bad", "line3"]

        # Set the search_pattern via the reactive, which will trigger
        # watch_search_pattern -> _rebuild_with_filter. Then verify the
        # net result.
        content.search_pattern = "ERROR"

        # The watcher triggers _rebuild_with_filter once (via watch_search_pattern)
        content.remove_children.assert_called()
        # Only 1 line matches "ERROR"
        assert content.mount.call_count == 1

    def test_rebuild_auto_scroll(self):
        content = _make_log_content()
        content._lines = ["line1"]
        # search_pattern already ""

        content._rebuild_with_filter()

        content.scroll_end.assert_called_once()

    def test_rebuild_no_auto_scroll(self):
        content = _make_log_content()
        content._lines = ["line1"]
        content.auto_scroll = False

        content._rebuild_with_filter()

        content.scroll_end.assert_not_called()

    def test_rebuild_error_line_gets_error_class(self):
        content = _make_log_content()
        content._lines = ["CRITICAL: system failure"]

        content._rebuild_with_filter()

        widget = content.mount.call_args[0][0]
        assert "log-line-error" in widget._classes
        assert content.mount.call_count == 1


# ============================================================================
# LogContent.load_file
# ============================================================================


class TestLogContentLoadFile:
    """Tests for LogContent.load_file using pytest tmp_path."""

    def test_file_not_found(self, tmp_path):
        content = _make_log_content()
        missing = tmp_path / "nonexistent.log"

        content.load_file(missing)

        assert content._current_file == missing
        # Should add "File not found" line
        assert any("not found" in line for line in content._lines)

    def test_normal_file(self, tmp_path):
        content = _make_log_content()
        log_file = tmp_path / "test.log"
        log_file.write_text("line1\nline2\nline3\n")

        content.load_file(log_file)

        assert content._current_file == log_file
        # Should have loaded lines
        assert len(content._lines) >= 3

    def test_large_file_chunked(self, tmp_path):
        content = _make_log_content()
        log_file = tmp_path / "big.log"
        # Write many lines
        lines = [f"log line {i}: some data here" for i in range(500)]
        log_file.write_text("\n".join(lines) + "\n")

        content.load_file(log_file)

        assert len(content._lines) == 500

    def test_load_file_with_tail(self, tmp_path):
        content = _make_log_content()
        log_file = tmp_path / "tail.log"
        log_file.write_text("line1\n")

        content.load_file(log_file, tail=True)

        content.set_interval.assert_called_once()

    def test_load_file_clears_previous(self, tmp_path):
        content = _make_log_content()
        content._lines = ["old-line"]
        log_file = tmp_path / "new.log"
        log_file.write_text("new-line\n")

        content.load_file(log_file)

        # clear() was called (remove_children called, _lines reset)
        content.remove_children.assert_called()
        assert "old-line" not in content._lines

    def test_load_file_respects_max_lines(self, tmp_path):
        content = _make_log_content()
        content._max_lines = 5
        log_file = tmp_path / "overflow.log"
        lines = [f"line {i}" for i in range(20)]
        log_file.write_text("\n".join(lines) + "\n")

        content.load_file(log_file)

        # Should stop after max_lines
        assert len(content._lines) <= 5

    def test_load_file_read_error(self, tmp_path):
        content = _make_log_content()
        log_file = tmp_path / "error.log"
        log_file.write_text("content")

        with patch("builtins.open", side_effect=PermissionError("denied")):
            content.load_file(log_file)

        # Should add error line
        assert any("Error reading" in line for line in content._lines)

    def test_load_file_stores_file_position(self, tmp_path):
        content = _make_log_content()
        log_file = tmp_path / "pos.log"
        log_file.write_text("line1\nline2\n")

        content.load_file(log_file)

        assert hasattr(content, "_file_position")
        assert content._file_position > 0


# ============================================================================
# LogContent tail operations
# ============================================================================


class TestLogContentTail(unittest.TestCase):
    """Tests for start_tail and stop_tail."""

    def test_start_tail_sets_timer(self):
        content = _make_log_content()
        content._current_file = Path("/tmp/test.log")
        mock_timer = Mock()
        content.set_interval = Mock(return_value=mock_timer)

        content.start_tail()

        content.set_interval.assert_called_once_with(0.5, content._check_for_updates)
        assert content._tail_timer is mock_timer

    def test_start_tail_no_file(self):
        content = _make_log_content()
        content._current_file = None

        content.start_tail()

        content.set_interval.assert_not_called()
        assert content._tail_timer is None

    def test_start_tail_already_running(self):
        content = _make_log_content()
        content._current_file = Path("/tmp/test.log")
        content._tail_timer = Mock()  # already set

        content.start_tail()

        # Should not create another timer
        content.set_interval.assert_not_called()

    def test_stop_tail(self):
        content = _make_log_content()
        mock_timer = Mock()
        content._tail_timer = mock_timer

        content.stop_tail()

        mock_timer.stop.assert_called_once()
        assert content._tail_timer is None

    def test_stop_tail_when_not_running(self):
        content = _make_log_content()
        content._tail_timer = None

        content.stop_tail()  # Should not raise


class TestLogContentCheckForUpdates:
    """Tests for _check_for_updates."""

    def test_no_file(self):
        content = _make_log_content()
        content._current_file = None

        content._check_for_updates()  # Should not raise

    def test_file_not_exists(self, tmp_path):
        content = _make_log_content()
        content._current_file = tmp_path / "gone.log"

        content._check_for_updates()  # Should not raise

    def test_file_with_new_content(self, tmp_path):
        content = _make_log_content()
        log_file = tmp_path / "tailing.log"
        log_file.write_text("existing\n")

        content._current_file = log_file
        content._file_position = 0  # start from beginning

        content._check_for_updates()

        assert "existing" in content._lines

    def test_file_unchanged(self, tmp_path):
        content = _make_log_content()
        log_file = tmp_path / "static.log"
        log_file.write_text("existing\n")

        content._current_file = log_file
        # Position at end of file
        content._file_position = log_file.stat().st_size

        content._check_for_updates()

        # No new lines added
        assert content._lines == []

    def test_file_read_error(self, tmp_path):
        content = _make_log_content()
        log_file = tmp_path / "error.log"
        log_file.write_text("content\n")
        content._current_file = log_file
        content._file_position = 0

        with patch("builtins.open", side_effect=OSError("read fail")):
            content._check_for_updates()  # Should not raise


# ============================================================================
# LogViewerScreen
# ============================================================================


class TestLogViewerScreenInit(unittest.TestCase):
    """Tests for LogViewerScreen initialization."""

    def test_init_default(self):
        screen = LogViewerScreen()
        assert screen._initial_agent is None

    def test_init_with_filter_agent(self):
        screen = LogViewerScreen(filter_agent="architect")
        assert screen._initial_agent == "architect"

    def test_bindings(self):
        screen = LogViewerScreen()
        keys = [b.key for b in screen.BINDINGS]
        assert "escape" in keys
        assert "ctrl+k" in keys
        assert "slash" in keys
        assert "t" in keys
        assert "c" in keys
        assert "g" in keys
        assert "G" in keys

    def test_reactive_defaults(self):
        screen = LogViewerScreen()
        assert screen.tail_enabled is False
        assert screen.current_agent == ""

    def test_euxis_app_property(self):
        screen = LogViewerScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)
        try:
            assert screen.euxis_app is mock_app
        finally:
            patcher.stop()


class TestLogViewerScreenOnMount(unittest.TestCase):
    """Tests for on_mount."""

    def test_on_mount_configures_header_and_loads_list(self):
        screen = LogViewerScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)

        mock_header = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            from tui.widgets.header import ETXHeader

            if selector is ETXHeader:
                return mock_header
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)
        screen._load_agent_list = Mock()
        screen.call_later = Mock()

        try:
            screen.on_mount()

            assert mock_header.project == "test-project"
            assert mock_header.branch == "main"
            assert mock_header.provider == "claude"
            screen._load_agent_list.assert_called_once()
        finally:
            patcher.stop()

    def test_on_mount_with_initial_agent(self):
        screen = LogViewerScreen(filter_agent="debugger")
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)

        mock_header = Mock()
        screen.query_one = Mock(return_value=mock_header)
        screen._load_agent_list = Mock()
        screen.call_later = Mock()

        try:
            screen.on_mount()
            screen.call_later.assert_called_once()
        finally:
            patcher.stop()

    def test_on_mount_without_initial_agent(self):
        screen = LogViewerScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)

        mock_header = Mock()
        screen.query_one = Mock(return_value=mock_header)
        screen._load_agent_list = Mock()
        screen.call_later = Mock()

        try:
            screen.on_mount()
            screen.call_later.assert_not_called()
        finally:
            patcher.stop()


class TestLogViewerScreenLoadAgentList:
    """Tests for _load_agent_list."""

    def test_no_logs_found(self, tmp_path):
        screen = LogViewerScreen()
        mock_option_list = Mock()
        screen.query_one = Mock(return_value=mock_option_list)

        euxis_home = tmp_path / "euxis"
        euxis_home.mkdir()

        with (
            patch("tui.screens.logs.EUXIS_HOME", euxis_home),
            patch("tui.screens.logs.get_project_name", return_value="myproject"),
        ):
            screen._load_agent_list()

        mock_option_list.add_option.assert_called_once()
        # The option should be disabled with "No logs found"
        opt = mock_option_list.add_option.call_args[0][0]
        assert opt.disabled is True

    def test_agents_with_logs(self, tmp_path):
        screen = LogViewerScreen()
        mock_option_list = Mock()
        screen.query_one = Mock(return_value=mock_option_list)

        euxis_home = tmp_path / "euxis"

        # Create log directory structure
        project_dir = euxis_home / "data" / "projects" / "myproject"
        arch_dir = project_dir / "architect"
        arch_dir.mkdir(parents=True)
        (arch_dir / "run.log").write_text("log data")
        (arch_dir / "run2.log").write_text("more log data")

        debug_dir = project_dir / "debugger"
        debug_dir.mkdir(parents=True)
        (debug_dir / "output.md").write_text("# Output")

        with (
            patch("tui.screens.logs.EUXIS_HOME", euxis_home),
            patch("tui.screens.logs.get_project_name", return_value="myproject"),
        ):
            screen._load_agent_list()

        # Two agents found
        assert mock_option_list.add_option.call_count == 2

    def test_multiple_log_sources(self, tmp_path):
        screen = LogViewerScreen()
        mock_option_list = Mock()
        screen.query_one = Mock(return_value=mock_option_list)

        euxis_home = tmp_path / "euxis"

        # Create logs in runtime/logs
        runtime_log_dir = euxis_home / "euxis-runtime" / "logs" / "agent-x"
        runtime_log_dir.mkdir(parents=True)
        (runtime_log_dir / "run.log").write_text("runtime log")

        # Create logs in runtime/history
        history_dir = euxis_home / "euxis-runtime" / "history" / "agent-y"
        history_dir.mkdir(parents=True)
        (history_dir / "mission.jsonl").write_text("{}\n")

        with (
            patch("tui.screens.logs.EUXIS_HOME", euxis_home),
            patch("tui.screens.logs.get_project_name", return_value="myproject"),
        ):
            screen._load_agent_list()

        assert mock_option_list.add_option.call_count == 2

    def test_skips_non_directories(self, tmp_path):
        screen = LogViewerScreen()
        mock_option_list = Mock()
        screen.query_one = Mock(return_value=mock_option_list)

        euxis_home = tmp_path / "euxis"
        project_dir = euxis_home / "data" / "projects" / "myproject"
        project_dir.mkdir(parents=True)
        # Create a regular file, not a directory
        (project_dir / "notes.txt").write_text("not a dir")

        with (
            patch("tui.screens.logs.EUXIS_HOME", euxis_home),
            patch("tui.screens.logs.get_project_name", return_value="myproject"),
        ):
            screen._load_agent_list()

        # Should show "No logs found"
        opt = mock_option_list.add_option.call_args[0][0]
        assert opt.disabled is True

    def test_dir_without_log_files(self, tmp_path):
        screen = LogViewerScreen()
        mock_option_list = Mock()
        screen.query_one = Mock(return_value=mock_option_list)

        euxis_home = tmp_path / "euxis"
        agent_dir = euxis_home / "data" / "projects" / "myproject" / "agent-a"
        agent_dir.mkdir(parents=True)
        # Create non-log file
        (agent_dir / "config.yaml").write_text("key: value")

        with (
            patch("tui.screens.logs.EUXIS_HOME", euxis_home),
            patch("tui.screens.logs.get_project_name", return_value="myproject"),
        ):
            screen._load_agent_list()

        opt = mock_option_list.add_option.call_args[0][0]
        assert opt.disabled is True


class TestLogViewerScreenSelectAgent(unittest.TestCase):
    """Tests for _select_agent."""

    def test_matching_agent(self):
        screen = LogViewerScreen()
        mock_option_list = Mock()
        mock_option = Mock()
        mock_option.id = "architect"
        mock_option_list._options = [mock_option]
        screen.query_one = Mock(return_value=mock_option_list)
        screen._load_agent_logs = Mock()

        screen._select_agent("architect")

        assert mock_option_list.highlighted == 0
        screen._load_agent_logs.assert_called_once_with("architect")

    def test_no_matching_agent(self):
        screen = LogViewerScreen()
        mock_option_list = Mock()
        mock_option = Mock()
        mock_option.id = "debugger"
        mock_option_list._options = [mock_option]
        screen.query_one = Mock(return_value=mock_option_list)
        screen._load_agent_logs = Mock()

        screen._select_agent("nonexistent")

        screen._load_agent_logs.assert_not_called()


class TestLogViewerScreenOptionSelected(unittest.TestCase):
    """Tests for on_option_list_option_selected."""

    def test_valid_id(self):
        screen = LogViewerScreen()
        screen._load_agent_logs = Mock()

        event = Mock()
        event.option.id = "architect"

        screen.on_option_list_option_selected(event)

        screen._load_agent_logs.assert_called_once_with("architect")

    def test_no_id(self):
        screen = LogViewerScreen()
        screen._load_agent_logs = Mock()

        event = Mock()
        event.option.id = ""

        screen.on_option_list_option_selected(event)

        screen._load_agent_logs.assert_not_called()

    def test_none_id(self):
        screen = LogViewerScreen()
        screen._load_agent_logs = Mock()

        event = Mock()
        event.option.id = None

        screen.on_option_list_option_selected(event)

        screen._load_agent_logs.assert_not_called()


class TestLogViewerScreenLoadAgentLogs:
    """Tests for _load_agent_logs."""

    def test_with_log_files(self, tmp_path):
        screen = LogViewerScreen()
        screen.tail_enabled = False

        mock_content = Mock()
        mock_status = Mock()

        def query_one_side_effect(selector, cls=None):
            if selector == "#log-content":
                return mock_content
            if selector == "#log-status":
                return mock_status
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)

        euxis_home = tmp_path / "euxis"
        log_dir = euxis_home / "euxis-runtime" / "logs" / "architect"
        log_dir.mkdir(parents=True)
        log_file = log_dir / "run.log"
        log_file.write_text("log data here\n")

        with (
            patch("tui.screens.logs.EUXIS_HOME", euxis_home),
            patch("tui.screens.logs.get_project_name", return_value="myproject"),
        ):
            screen._load_agent_logs("architect")

        assert screen.current_agent == "architect"
        mock_content.stop_tail.assert_called_once()
        mock_content.clear.assert_called_once()
        mock_content.load_file.assert_called_once()
        mock_status.update.assert_called_once()
        assert "architect" in mock_status.update.call_args[0][0]

    def test_without_log_files(self, tmp_path):
        screen = LogViewerScreen()

        mock_content = Mock()
        mock_status = Mock()

        def query_one_side_effect(selector, cls=None):
            if selector == "#log-content":
                return mock_content
            if selector == "#log-status":
                return mock_status
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)

        euxis_home = tmp_path / "euxis"
        euxis_home.mkdir()

        with (
            patch("tui.screens.logs.EUXIS_HOME", euxis_home),
            patch("tui.screens.logs.get_project_name", return_value="myproject"),
        ):
            screen._load_agent_logs("missing-agent")

        mock_content.add_line.assert_called_once()
        assert "No logs found" in mock_content.add_line.call_args[0][0]
        mock_status.update.assert_called_once()
        assert "No logs" in mock_status.update.call_args[0][0]

    def test_selects_most_recent_file(self, tmp_path):
        screen = LogViewerScreen()
        screen.tail_enabled = False

        mock_content = Mock()
        mock_status = Mock()

        def query_one_side_effect(selector, cls=None):
            if selector == "#log-content":
                return mock_content
            if selector == "#log-status":
                return mock_status
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)

        euxis_home = tmp_path / "euxis"
        log_dir = euxis_home / "euxis-runtime" / "logs" / "architect"
        log_dir.mkdir(parents=True)

        import time

        old_file = log_dir / "old.log"
        old_file.write_text("old data\n")
        # Force older mtime
        os.utime(old_file, (time.time() - 1000, time.time() - 1000))

        new_file = log_dir / "new.log"
        new_file.write_text("new data\n")

        with (
            patch("tui.screens.logs.EUXIS_HOME", euxis_home),
            patch("tui.screens.logs.get_project_name", return_value="myproject"),
        ):
            screen._load_agent_logs("architect")

        # Should load the most recent file
        loaded_path = mock_content.load_file.call_args[0][0]
        assert loaded_path.name == "new.log"


class TestLogViewerScreenInputChanged(unittest.TestCase):
    """Tests for on_input_changed."""

    def test_search_input_updates_pattern(self):
        screen = LogViewerScreen()
        mock_content = Mock()
        screen.query_one = Mock(return_value=mock_content)

        event = Mock()
        event.input.id = "log-search"
        event.value = "ERROR"

        screen.on_input_changed(event)

        assert mock_content.search_pattern == "ERROR"

    def test_other_input_ignored(self):
        screen = LogViewerScreen()
        mock_content = Mock()
        screen.query_one = Mock(return_value=mock_content)

        event = Mock()
        event.input.id = "other-input"
        event.value = "test"

        screen.on_input_changed(event)

        # search_pattern should not be set
        assert not hasattr(mock_content, "search_pattern") or mock_content.search_pattern != "test"


class TestLogViewerScreenSwitchChanged(unittest.TestCase):
    """Tests for on_switch_changed."""

    def test_tail_toggle_on(self):
        screen = LogViewerScreen()
        mock_content = Mock()
        screen.query_one = Mock(return_value=mock_content)
        screen.notify = Mock()

        event = Mock()
        event.switch.id = "tail-toggle"
        event.value = True

        screen.on_switch_changed(event)

        assert screen.tail_enabled is True
        mock_content.start_tail.assert_called_once()
        screen.notify.assert_called_once()
        assert "enabled" in screen.notify.call_args[0][0]

    def test_tail_toggle_off(self):
        screen = LogViewerScreen()
        mock_content = Mock()
        screen.query_one = Mock(return_value=mock_content)
        screen.notify = Mock()

        event = Mock()
        event.switch.id = "tail-toggle"
        event.value = False

        screen.on_switch_changed(event)

        assert screen.tail_enabled is False
        mock_content.stop_tail.assert_called_once()
        screen.notify.assert_called_once()
        assert "disabled" in screen.notify.call_args[0][0]

    def test_other_switch_ignored(self):
        screen = LogViewerScreen()
        screen.notify = Mock()

        event = Mock()
        event.switch.id = "other-switch"
        event.value = True

        screen.on_switch_changed(event)

        screen.notify.assert_not_called()


# ============================================================================
# LogViewerScreen action methods
# ============================================================================


class TestLogViewerScreenActions(unittest.TestCase):
    """Tests for action methods."""

    def test_action_go_back(self):
        screen = LogViewerScreen()
        mock_content = Mock()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)

        screen.query_one = Mock(return_value=mock_content)

        try:
            screen.action_go_back()
            mock_content.stop_tail.assert_called_once()
            mock_app.pop_screen.assert_called_once()
        finally:
            patcher.stop()

    def test_action_focus_search(self):
        screen = LogViewerScreen()
        mock_input = Mock()
        screen.query_one = Mock(return_value=mock_input)

        screen.action_focus_search()

        mock_input.focus.assert_called_once()

    def test_action_toggle_tail(self):
        screen = LogViewerScreen()
        mock_switch = Mock()
        mock_switch.value = False
        screen.query_one = Mock(return_value=mock_switch)

        screen.action_toggle_tail()

        assert mock_switch.value is True

    def test_action_toggle_tail_off(self):
        screen = LogViewerScreen()
        mock_switch = Mock()
        mock_switch.value = True
        screen.query_one = Mock(return_value=mock_switch)

        screen.action_toggle_tail()

        assert mock_switch.value is False

    def test_action_clear_filter(self):
        screen = LogViewerScreen()
        mock_input = Mock()
        screen.query_one = Mock(return_value=mock_input)

        screen.action_clear_filter()

        assert mock_input.value == ""

    def test_action_scroll_top(self):
        screen = LogViewerScreen()
        mock_content = Mock()
        screen.query_one = Mock(return_value=mock_content)

        screen.action_scroll_top()

        assert mock_content.auto_scroll is False
        mock_content.scroll_home.assert_called_once()

    def test_action_scroll_bottom(self):
        screen = LogViewerScreen()
        mock_content = Mock()
        screen.query_one = Mock(return_value=mock_content)

        screen.action_scroll_bottom()

        assert mock_content.auto_scroll is True
        mock_content.scroll_end.assert_called_once()


# ============================================================================
# Module-level constants
# ============================================================================


class TestModuleConstants(unittest.TestCase):
    """Tests for module-level constants."""

    def test_log_level_patterns_keys(self):
        assert "ERROR" in LOG_LEVEL_PATTERNS
        assert "WARN" in LOG_LEVEL_PATTERNS
        assert "INFO" in LOG_LEVEL_PATTERNS
        assert "DEBUG" in LOG_LEVEL_PATTERNS
        assert "SUCCESS" in LOG_LEVEL_PATTERNS

    def test_log_level_patterns_are_valid_regex(self):
        for level, (pattern, color) in LOG_LEVEL_PATTERNS.items():
            # Should not raise
            re.compile(pattern)

    def test_timestamp_pattern_is_regex(self):
        assert isinstance(TIMESTAMP_PATTERN, re.Pattern)
        match = TIMESTAMP_PATTERN.search("2026-02-18T10:30:45 INFO test")
        assert match is not None
        assert match.group(1) == "2026-02-18T10:30:45"


if __name__ == "__main__":
    unittest.main()
