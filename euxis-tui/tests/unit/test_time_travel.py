# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Comprehensive unit tests for TimeTravelScreen (time_travel.py).

Covers:
- MissionEvent / MissionInfo dataclasses
- get_history_dir with/without EUXIS_HOME
- list_missions: empty dir, normal files, completion events, bad JSON, IOError
- load_mission_events: file not found, valid events, bad JSON lines
- EventCard compose for every event type
- TimeTravelScreen init, actions, button/list handlers, go_back
"""

from __future__ import annotations

import json
import os
import unittest
from pathlib import Path
from unittest.mock import Mock, MagicMock, PropertyMock, patch, call

import pytest

from tui.screens.time_travel import (
    EventCard,
    MissionEvent,
    MissionInfo,
    TimeTravelScreen,
    get_history_dir,
    list_missions,
    load_mission_events,
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


def _make_event(**overrides):
    """Build a MissionEvent with sensible defaults."""
    defaults = {
        "type": "thought",
        "timestamp": "2026-02-18T10:30:00Z",
        "agent": "architect",
        "session": "sess-001",
        "data": {},
        "index": 0,
    }
    defaults.update(overrides)
    return MissionEvent(**defaults)


def _get_static_content(widget):
    """Extract text content from a Textual Static widget (name-mangled __content)."""
    return widget._Static__content


# ============================================================================
# Dataclass tests
# ============================================================================


class TestMissionEvent(unittest.TestCase):
    """Tests for MissionEvent dataclass."""

    def test_creation_with_defaults(self):
        event = MissionEvent(
            type="thought",
            timestamp="2026-02-18T10:00:00Z",
            agent="architect",
            session="sess-001",
            data={"content": "hello"},
        )
        assert event.type == "thought"
        assert event.timestamp == "2026-02-18T10:00:00Z"
        assert event.agent == "architect"
        assert event.session == "sess-001"
        assert event.data == {"content": "hello"}
        assert event.index == 0  # default

    def test_creation_with_index(self):
        event = MissionEvent(
            type="tool_call",
            timestamp="2026-02-18T10:00:01Z",
            agent="debugger",
            session="sess-002",
            data={"tool": "lint"},
            index=5,
        )
        assert event.index == 5

    def test_data_field_is_dict(self):
        event = _make_event(data={"key": "val", "nested": {"a": 1}})
        assert isinstance(event.data, dict)
        assert event.data["nested"]["a"] == 1


class TestMissionInfo(unittest.TestCase):
    """Tests for MissionInfo dataclass."""

    def test_creation_complete(self):
        info = MissionInfo(
            mission_id="m-001",
            description="Test mission",
            started="2026-02-18T09:00:00Z",
            completed="2026-02-18T09:05:00Z",
            status="success",
            event_count=42,
        )
        assert info.mission_id == "m-001"
        assert info.description == "Test mission"
        assert info.started == "2026-02-18T09:00:00Z"
        assert info.completed == "2026-02-18T09:05:00Z"
        assert info.status == "success"
        assert info.event_count == 42

    def test_completed_none(self):
        info = MissionInfo(
            mission_id="m-002",
            description="",
            started="2026-02-18T09:00:00Z",
            completed=None,
            status="in_progress",
            event_count=3,
        )
        assert info.completed is None
        assert info.status == "in_progress"


# ============================================================================
# get_history_dir
# ============================================================================


class TestGetHistoryDir(unittest.TestCase):
    """Tests for get_history_dir()."""

    def test_with_euxis_home_env(self):
        with patch.dict(os.environ, {"EUXIS_HOME": "/custom/euxis"}):
            result = get_history_dir()
        assert result == Path("/custom/euxis/euxis-runtime/history")

    def test_without_euxis_home_env(self):
        with patch.dict(os.environ, {}, clear=True):
            with patch("os.path.expanduser", return_value="/home/testuser/.euxis"):
                result = get_history_dir()
        assert result == Path("/home/testuser/.euxis/euxis-runtime/history")

    def test_returns_path_object(self):
        with patch.dict(os.environ, {"EUXIS_HOME": "/tmp/test"}):
            result = get_history_dir()
        assert isinstance(result, Path)


# ============================================================================
# list_missions (using tmp_path)
# ============================================================================


class TestListMissions:
    """Tests for list_missions() using pytest tmp_path fixture."""

    def test_empty_dir(self, tmp_path):
        """Empty history directory returns empty list."""
        history_dir = tmp_path / "euxis-runtime" / "history"
        history_dir.mkdir(parents=True)

        with patch("tui.screens.time_travel.get_history_dir", return_value=history_dir):
            result = list_missions()

        assert result == []

    def test_nonexistent_dir(self, tmp_path):
        """Non-existent history directory returns empty list."""
        history_dir = tmp_path / "nonexistent" / "history"

        with patch("tui.screens.time_travel.get_history_dir", return_value=history_dir):
            result = list_missions()

        assert result == []

    def test_normal_mission_without_completion(self, tmp_path):
        """Mission file with start event but no completion."""
        history_dir = tmp_path / "euxis-runtime" / "history"
        history_dir.mkdir(parents=True)

        start_event = {
            "mission_id": "mission-alpha",
            "type": "mission_start",
            "description": "Alpha test",
            "timestamp": "2026-02-18T09:00:00Z",
        }
        thought_event = {
            "type": "thought",
            "timestamp": "2026-02-18T09:00:05Z",
            "agent": "architect",
            "session": "s1",
            "data": {"content": "thinking"},
        }

        mission_file = history_dir / "mission-alpha.jsonl"
        mission_file.write_text(
            json.dumps(start_event) + "\n" + json.dumps(thought_event) + "\n"
        )

        with patch("tui.screens.time_travel.get_history_dir", return_value=history_dir):
            result = list_missions()

        assert len(result) == 1
        assert result[0].mission_id == "mission-alpha"
        assert result[0].description == "Alpha test"
        assert result[0].started == "2026-02-18T09:00:00Z"
        assert result[0].completed is None
        assert result[0].status == "in_progress"
        assert result[0].event_count == 2

    def test_mission_with_completion_event(self, tmp_path):
        """Mission file containing a mission_complete event."""
        history_dir = tmp_path / "euxis-runtime" / "history"
        history_dir.mkdir(parents=True)

        lines = [
            json.dumps({
                "mission_id": "mission-beta",
                "type": "mission_start",
                "description": "Beta test",
                "timestamp": "2026-02-18T10:00:00Z",
            }),
            json.dumps({
                "type": "thought",
                "timestamp": "2026-02-18T10:00:05Z",
            }),
            json.dumps({
                "type": "mission_complete",
                "timestamp": "2026-02-18T10:05:00Z",
                "status": "success",
            }),
        ]

        mission_file = history_dir / "mission-beta.jsonl"
        mission_file.write_text("\n".join(lines) + "\n")

        with patch("tui.screens.time_travel.get_history_dir", return_value=history_dir):
            result = list_missions()

        assert len(result) == 1
        assert result[0].completed == "2026-02-18T10:05:00Z"
        assert result[0].status == "success"

    def test_mission_with_error_status(self, tmp_path):
        """Mission that completed with error status."""
        history_dir = tmp_path / "euxis-runtime" / "history"
        history_dir.mkdir(parents=True)

        lines = [
            json.dumps({
                "mission_id": "mission-fail",
                "type": "mission_start",
                "description": "Failing test",
                "timestamp": "2026-02-18T11:00:00Z",
            }),
            json.dumps({
                "type": "mission_complete",
                "timestamp": "2026-02-18T11:01:00Z",
                "status": "error",
            }),
        ]

        mission_file = history_dir / "mission-fail.jsonl"
        mission_file.write_text("\n".join(lines) + "\n")

        with patch("tui.screens.time_travel.get_history_dir", return_value=history_dir):
            result = list_missions()

        assert len(result) == 1
        assert result[0].status == "error"

    def test_file_with_bad_json_first_line(self, tmp_path):
        """File whose first line is invalid JSON is skipped."""
        history_dir = tmp_path / "euxis-runtime" / "history"
        history_dir.mkdir(parents=True)

        mission_file = history_dir / "bad-start.jsonl"
        mission_file.write_text("NOT VALID JSON\n{\"type\":\"thought\"}\n")

        with patch("tui.screens.time_travel.get_history_dir", return_value=history_dir):
            result = list_missions()

        assert result == []

    def test_file_with_bad_json_in_middle(self, tmp_path):
        """Bad JSON lines in the middle are skipped but mission still loads."""
        history_dir = tmp_path / "euxis-runtime" / "history"
        history_dir.mkdir(parents=True)

        lines = [
            json.dumps({
                "mission_id": "mission-mixed",
                "type": "mission_start",
                "description": "Mixed quality",
                "timestamp": "2026-02-18T12:00:00Z",
            }),
            "NOT VALID JSON",
            json.dumps({
                "type": "mission_complete",
                "timestamp": "2026-02-18T12:05:00Z",
                "status": "success",
            }),
        ]

        mission_file = history_dir / "mission-mixed.jsonl"
        mission_file.write_text("\n".join(lines) + "\n")

        with patch("tui.screens.time_travel.get_history_dir", return_value=history_dir):
            result = list_missions()

        assert len(result) == 1
        # The bad JSON line in reversed search is skipped, completion found
        assert result[0].status == "success"
        assert result[0].event_count == 3  # all lines counted

    def test_empty_first_line_skipped(self, tmp_path):
        """File with empty first line is skipped via continue."""
        history_dir = tmp_path / "euxis-runtime" / "history"
        history_dir.mkdir(parents=True)

        mission_file = history_dir / "empty-start.jsonl"
        mission_file.write_text("")  # empty file

        with patch("tui.screens.time_travel.get_history_dir", return_value=history_dir):
            result = list_missions()

        assert result == []

    def test_ioerror_skipped(self, tmp_path):
        """IOError on file read is caught and mission is skipped."""
        history_dir = tmp_path / "euxis-runtime" / "history"
        history_dir.mkdir(parents=True)

        # Create a file that exists but will raise IOError on open
        mission_file = history_dir / "broken.jsonl"
        mission_file.write_text("placeholder")

        with patch("tui.screens.time_travel.get_history_dir", return_value=history_dir):
            with patch("builtins.open", side_effect=IOError("disk error")):
                result = list_missions()

        assert result == []

    def test_multiple_missions_sorted_reverse(self, tmp_path):
        """Multiple mission files are returned sorted by filename (reverse)."""
        history_dir = tmp_path / "euxis-runtime" / "history"
        history_dir.mkdir(parents=True)

        for name in ["aaa-mission", "zzz-mission"]:
            lines = [json.dumps({
                "mission_id": name,
                "type": "mission_start",
                "description": f"Mission {name}",
                "timestamp": "2026-02-18T09:00:00Z",
            })]
            (history_dir / f"{name}.jsonl").write_text("\n".join(lines) + "\n")

        with patch("tui.screens.time_travel.get_history_dir", return_value=history_dir):
            result = list_missions()

        assert len(result) == 2
        # Reverse sorted: zzz comes first
        assert result[0].mission_id == "zzz-mission"
        assert result[1].mission_id == "aaa-mission"

    def test_mission_id_fallback_to_stem(self, tmp_path):
        """If start event has no mission_id key, falls back to file stem."""
        history_dir = tmp_path / "euxis-runtime" / "history"
        history_dir.mkdir(parents=True)

        lines = [json.dumps({
            "type": "mission_start",
            "description": "No ID field",
            "timestamp": "2026-02-18T09:00:00Z",
        })]
        (history_dir / "fallback-id.jsonl").write_text("\n".join(lines) + "\n")

        with patch("tui.screens.time_travel.get_history_dir", return_value=history_dir):
            result = list_missions()

        assert len(result) == 1
        assert result[0].mission_id == "fallback-id"

    def test_non_jsonl_files_ignored(self, tmp_path):
        """Non-.jsonl files in the history directory are ignored."""
        history_dir = tmp_path / "euxis-runtime" / "history"
        history_dir.mkdir(parents=True)

        (history_dir / "notes.txt").write_text("not a mission")
        (history_dir / "data.json").write_text("{}")

        with patch("tui.screens.time_travel.get_history_dir", return_value=history_dir):
            result = list_missions()

        assert result == []


# ============================================================================
# load_mission_events (using tmp_path)
# ============================================================================


class TestLoadMissionEvents:
    """Tests for load_mission_events()."""

    def test_file_not_found(self, tmp_path):
        """Returns empty list when mission file does not exist."""
        history_dir = tmp_path / "euxis-runtime" / "history"
        history_dir.mkdir(parents=True)

        with patch("tui.screens.time_travel.get_history_dir", return_value=history_dir):
            result = load_mission_events("nonexistent-mission")

        assert result == []

    def test_valid_events(self, tmp_path):
        """Loads all valid events with correct indexing."""
        history_dir = tmp_path / "euxis-runtime" / "history"
        history_dir.mkdir(parents=True)

        events = [
            {"type": "mission_start", "timestamp": "2026-02-18T09:00:00Z",
             "agent": "architect", "session": "s1", "data": {}},
            {"type": "thought", "timestamp": "2026-02-18T09:00:05Z",
             "agent": "architect", "session": "s1", "data": {"content": "thinking"}},
            {"type": "tool_call", "timestamp": "2026-02-18T09:00:10Z",
             "agent": "architect", "session": "s1", "data": {"tool": "lint"}},
        ]

        mission_file = history_dir / "test-mission.jsonl"
        mission_file.write_text("\n".join(json.dumps(e) for e in events) + "\n")

        with patch("tui.screens.time_travel.get_history_dir", return_value=history_dir):
            result = load_mission_events("test-mission")

        assert len(result) == 3
        assert result[0].type == "mission_start"
        assert result[0].index == 0
        assert result[1].type == "thought"
        assert result[1].index == 1
        assert result[1].data == {"content": "thinking"}
        assert result[2].type == "tool_call"
        assert result[2].index == 2
        assert result[2].agent == "architect"
        assert result[2].session == "s1"

    def test_bad_json_lines_skipped(self, tmp_path):
        """Invalid JSON lines are skipped, valid ones loaded."""
        history_dir = tmp_path / "euxis-runtime" / "history"
        history_dir.mkdir(parents=True)

        lines = [
            json.dumps({"type": "thought", "timestamp": "T1", "agent": "a", "session": "s", "data": {}}),
            "INVALID JSON",
            json.dumps({"type": "cost", "timestamp": "T2", "agent": "b", "session": "s2", "data": {"cost_usd": 0.01}}),
        ]

        mission_file = history_dir / "mixed.jsonl"
        mission_file.write_text("\n".join(lines) + "\n")

        with patch("tui.screens.time_travel.get_history_dir", return_value=history_dir):
            result = load_mission_events("mixed")

        # Only the two valid lines
        assert len(result) == 2
        assert result[0].type == "thought"
        assert result[0].index == 0
        assert result[1].type == "cost"
        assert result[1].index == 2  # preserves enumerate index

    def test_missing_fields_use_defaults(self, tmp_path):
        """Events with missing fields use default values."""
        history_dir = tmp_path / "euxis-runtime" / "history"
        history_dir.mkdir(parents=True)

        lines = [json.dumps({"some_random_key": "value"})]
        mission_file = history_dir / "sparse.jsonl"
        mission_file.write_text("\n".join(lines) + "\n")

        with patch("tui.screens.time_travel.get_history_dir", return_value=history_dir):
            result = load_mission_events("sparse")

        assert len(result) == 1
        assert result[0].type == "unknown"
        assert result[0].timestamp == ""
        assert result[0].agent == ""
        assert result[0].session == ""
        assert result[0].data == {}


# ============================================================================
# EventCard
# ============================================================================


class TestEventCard(unittest.TestCase):
    """Tests for EventCard widget."""

    def test_init_stores_event(self):
        event = _make_event(type="thought")
        card = EventCard(event)
        assert card.event is event

    def test_compose_mission_start(self):
        event = _make_event(type="mission_start", timestamp="2026-02-18T10:30:00Z")
        card = EventCard(event)
        widgets = list(card.compose())
        # mission_start yields just the type header (no extra content)
        assert len(widgets) == 1
        # The header includes the type name and time
        content = _get_static_content(widgets[0])
        assert "mission_start" in content
        assert "10:30:00" in content

    def test_compose_mission_complete_success(self):
        event = _make_event(
            type="mission_complete",
            data={"status": "success", "summary": "All tasks done"},
        )
        card = EventCard(event)
        widgets = list(card.compose())
        # Header + status + summary = 3
        assert len(widgets) == 3

    def test_compose_mission_complete_failure(self):
        event = _make_event(
            type="mission_complete",
            data={"status": "error", "summary": ""},
        )
        card = EventCard(event)
        widgets = list(card.compose())
        # Header + status, no summary (empty string)
        assert len(widgets) == 2

    def test_compose_mission_complete_no_summary(self):
        event = _make_event(
            type="mission_complete",
            data={"status": "error"},
        )
        card = EventCard(event)
        widgets = list(card.compose())
        # Header + status, no summary key
        assert len(widgets) == 2

    def test_compose_thought(self):
        event = _make_event(
            type="thought",
            data={"content": "I should analyze this code"},
        )
        card = EventCard(event)
        widgets = list(card.compose())
        assert len(widgets) == 2
        assert "I should analyze this code" in _get_static_content(widgets[1])

    def test_compose_thought_truncated(self):
        event = _make_event(
            type="thought",
            data={"content": "x" * 200},
        )
        card = EventCard(event)
        widgets = list(card.compose())
        # Content is truncated to 100 chars
        assert len(widgets) == 2

    def test_compose_tool_call(self):
        event = _make_event(
            type="tool_call",
            data={"tool": "run_lint"},
        )
        card = EventCard(event)
        widgets = list(card.compose())
        assert len(widgets) == 2
        assert "run_lint" in _get_static_content(widgets[1])

    def test_compose_tool_result_success(self):
        event = _make_event(
            type="tool_result",
            data={"tool": "run_lint", "status": "success"},
        )
        card = EventCard(event)
        widgets = list(card.compose())
        assert len(widgets) == 2
        assert "success" in _get_static_content(widgets[1])

    def test_compose_tool_result_failure(self):
        event = _make_event(
            type="tool_result",
            data={"tool": "run_test", "status": "failure"},
        )
        card = EventCard(event)
        widgets = list(card.compose())
        assert len(widgets) == 2
        assert "failure" in _get_static_content(widgets[1])

    def test_compose_cost(self):
        event = _make_event(
            type="cost",
            data={"cost_usd": 0.0123, "model": "claude-sonnet-4"},
        )
        card = EventCard(event)
        widgets = list(card.compose())
        assert len(widgets) == 2
        content = _get_static_content(widgets[1])
        assert "claude-sonnet-4" in content
        assert "$0.0123" in content

    def test_compose_checkpoint(self):
        event = _make_event(
            type="checkpoint",
            data={"name": "after-lint"},
        )
        card = EventCard(event)
        widgets = list(card.compose())
        assert len(widgets) == 2
        assert "after-lint" in _get_static_content(widgets[1])

    def test_compose_unknown_type(self):
        event = _make_event(type="custom_event")
        card = EventCard(event)
        widgets = list(card.compose())
        # Only header for unknown type
        assert len(widgets) == 1

    def test_compose_error_type(self):
        event = _make_event(type="error")
        card = EventCard(event)
        widgets = list(card.compose())
        # Error type is listed in type_colors but has no special content rendering
        assert len(widgets) == 1

    def test_timestamp_with_t_separator(self):
        event = _make_event(type="thought", timestamp="2026-02-18T14:30:45Z")
        card = EventCard(event)
        widgets = list(card.compose())
        assert "14:30:45" in _get_static_content(widgets[0])

    def test_timestamp_without_t_separator(self):
        event = _make_event(type="thought", timestamp="14:30:45")
        card = EventCard(event)
        widgets = list(card.compose())
        # No "T" in timestamp, so full timestamp is used
        assert "14:30:45" in _get_static_content(widgets[0])

    def test_compose_mission_complete_success_color_green(self):
        event = _make_event(
            type="mission_complete",
            data={"status": "success"},
        )
        card = EventCard(event)
        widgets = list(card.compose())
        # The header should use green for success
        assert "green" in _get_static_content(widgets[0])


# ============================================================================
# TimeTravelScreen
# ============================================================================


class TestTimeTravelScreenInit(unittest.TestCase):
    """Tests for TimeTravelScreen initialization."""

    def test_init_without_mission_id(self):
        screen = TimeTravelScreen()
        assert screen.mission_id is None
        assert screen.events == []
        assert screen.missions == []

    def test_init_with_mission_id(self):
        screen = TimeTravelScreen(mission_id="m-001")
        assert screen.mission_id == "m-001"
        assert screen.events == []
        assert screen.missions == []

    def test_bindings(self):
        screen = TimeTravelScreen()
        keys = [b.key for b in screen.BINDINGS]
        assert "escape" in keys
        assert "h" in keys
        assert "l" in keys
        assert "left" in keys
        assert "right" in keys
        assert "r" in keys
        assert "enter" in keys

    def test_reactive_defaults(self):
        screen = TimeTravelScreen()
        # Mock internal methods to prevent Textual runtime queries
        screen.query_one = Mock(return_value=Mock())
        screen._render_events = Mock()
        screen._update_timeline = Mock()
        # Reactive defaults
        assert screen.current_index == 0
        assert screen.total_events == 0


class TestTimeTravelScreenActions(unittest.TestCase):
    """Tests for TimeTravelScreen action methods."""

    def _make_screen_with_events(self, n=10):
        """Create a screen with n fake events.

        Mocks _render_events and _update_timeline BEFORE setting reactive
        properties to prevent Textual runtime queries during watcher callbacks.
        """
        screen = TimeTravelScreen(mission_id="test")
        # Must mock watchers/helpers before setting reactive props
        screen.query_one = Mock(return_value=Mock())
        screen._render_events = Mock()
        screen._update_timeline = Mock()
        screen.notify = Mock()

        screen.events = [_make_event(index=i) for i in range(n)]
        screen.total_events = n
        screen.current_index = 0
        # Reset mock call counts after setup
        screen._render_events.reset_mock()
        screen._update_timeline.reset_mock()
        return screen

    def test_action_scrub_left_at_zero(self):
        screen = self._make_screen_with_events()
        screen.action_scrub_left()
        assert screen.current_index == 0  # stays at 0

    def test_action_scrub_left_above_zero(self):
        screen = self._make_screen_with_events()
        screen.current_index = 5
        screen._render_events.reset_mock()
        screen.action_scrub_left()
        assert screen.current_index == 4

    def test_action_scrub_right_at_max(self):
        screen = self._make_screen_with_events(n=10)
        screen.current_index = 9
        screen._render_events.reset_mock()
        screen.action_scrub_right()
        assert screen.current_index == 9  # stays at max

    def test_action_scrub_right_below_max(self):
        screen = self._make_screen_with_events(n=10)
        screen.current_index = 4
        screen._render_events.reset_mock()
        screen.action_scrub_right()
        assert screen.current_index == 5

    def test_action_select_mission_with_highlighted_child(self):
        screen = TimeTravelScreen()
        mock_list = Mock()
        mock_child = Mock()
        mock_child.id = "mission-alpha-1"
        mock_list.highlighted_child = mock_child

        screen.query_one = Mock(return_value=mock_list)
        screen._load_mission = Mock()

        screen.action_select_mission()

        screen._load_mission.assert_called_once_with("alpha-1")

    def test_action_select_mission_no_highlighted_child(self):
        screen = TimeTravelScreen()
        mock_list = Mock()
        mock_list.highlighted_child = None

        screen.query_one = Mock(return_value=mock_list)
        screen._load_mission = Mock()

        screen.action_select_mission()

        screen._load_mission.assert_not_called()

    def test_action_select_mission_non_matching_id(self):
        screen = TimeTravelScreen()
        mock_list = Mock()
        mock_child = Mock()
        mock_child.id = "something-else"
        mock_list.highlighted_child = mock_child

        screen.query_one = Mock(return_value=mock_list)
        screen._load_mission = Mock()

        screen.action_select_mission()

        screen._load_mission.assert_not_called()

    def test_action_select_mission_empty_id(self):
        screen = TimeTravelScreen()
        mock_list = Mock()
        mock_child = Mock()
        mock_child.id = ""
        mock_list.highlighted_child = mock_child

        screen.query_one = Mock(return_value=mock_list)
        screen._load_mission = Mock()

        screen.action_select_mission()

        screen._load_mission.assert_not_called()


class TestTimeTravelScreenRetryCheckpoint(unittest.TestCase):
    """Tests for action_retry_checkpoint."""

    def _make_screen(self):
        screen = TimeTravelScreen()
        # Mock to prevent reactive watcher failures
        screen.query_one = Mock(return_value=Mock())
        screen._render_events = Mock()
        screen._update_timeline = Mock()
        screen.notify = Mock()
        return screen

    def test_no_events(self):
        screen = self._make_screen()
        screen.events = []

        screen.action_retry_checkpoint()

        screen.notify.assert_called_once_with(
            "No mission loaded", severity="warning"
        )

    def test_events_with_checkpoint(self):
        screen = self._make_screen()
        screen.events = [
            _make_event(type="mission_start", index=0),
            _make_event(type="checkpoint", index=1, data={"name": "after-lint"}),
            _make_event(type="thought", index=2),
            _make_event(type="thought", index=3),
        ]
        screen.current_index = 3

        screen.action_retry_checkpoint()

        screen.notify.assert_called_once()
        assert "after-lint" in screen.notify.call_args[0][0]
        assert screen.notify.call_args[1].get("title") == "Checkpoint Recovery"

    def test_events_with_checkpoint_at_current_index(self):
        screen = self._make_screen()
        screen.events = [
            _make_event(type="mission_start", index=0),
            _make_event(type="checkpoint", index=1, data={"name": "cp-1"}),
        ]
        screen.current_index = 1

        screen.action_retry_checkpoint()

        assert "cp-1" in screen.notify.call_args[0][0]

    def test_events_without_checkpoint(self):
        screen = self._make_screen()
        screen.events = [
            _make_event(type="mission_start", index=0),
            _make_event(type="thought", index=1),
            _make_event(type="tool_call", index=2),
        ]
        screen.current_index = 2

        screen.action_retry_checkpoint()

        screen.notify.assert_called_once_with(
            "No checkpoint found before current position", severity="warning"
        )

    def test_checkpoint_only_after_current_position(self):
        """Checkpoint exists but only AFTER current position => not found."""
        screen = self._make_screen()
        screen.events = [
            _make_event(type="mission_start", index=0),
            _make_event(type="thought", index=1),
            _make_event(type="checkpoint", index=2, data={"name": "future-cp"}),
        ]
        screen.current_index = 1  # before the checkpoint

        screen.action_retry_checkpoint()

        screen.notify.assert_called_once_with(
            "No checkpoint found before current position", severity="warning"
        )


class TestTimeTravelScreenButtonPressed(unittest.TestCase):
    """Tests for on_button_pressed handler."""

    def test_btn_retry(self):
        screen = TimeTravelScreen()
        screen.action_retry_checkpoint = Mock()

        event = Mock()
        event.button.id = "btn-retry"

        screen.on_button_pressed(event)
        screen.action_retry_checkpoint.assert_called_once()

    def test_btn_export_with_mission_id(self):
        screen = TimeTravelScreen(mission_id="m-001")
        screen.notify = Mock()

        event = Mock()
        event.button.id = "btn-export"

        with patch("tui.screens.time_travel.get_history_dir") as mock_dir:
            mock_dir.return_value = Path("/tmp/history")
            screen.on_button_pressed(event)

        screen.notify.assert_called_once()
        assert "m-001" in screen.notify.call_args[0][0]
        assert screen.notify.call_args[1].get("title") == "Export Path"

    def test_btn_export_without_mission_id(self):
        screen = TimeTravelScreen()
        assert screen.mission_id is None
        screen.notify = Mock()

        event = Mock()
        event.button.id = "btn-export"

        screen.on_button_pressed(event)
        screen.notify.assert_not_called()

    def test_unknown_button_ignored(self):
        screen = TimeTravelScreen()
        screen.notify = Mock()
        screen.action_retry_checkpoint = Mock()

        event = Mock()
        event.button.id = "btn-something-else"

        screen.on_button_pressed(event)
        screen.notify.assert_not_called()
        screen.action_retry_checkpoint.assert_not_called()


class TestTimeTravelScreenListViewSelected(unittest.TestCase):
    """Tests for on_list_view_selected handler."""

    def test_valid_mission_item(self):
        screen = TimeTravelScreen()
        screen._load_mission = Mock()

        event = Mock()
        event.item.id = "mission-test-123"

        screen.on_list_view_selected(event)
        screen._load_mission.assert_called_once_with("test-123")

    def test_invalid_item_id(self):
        screen = TimeTravelScreen()
        screen._load_mission = Mock()

        event = Mock()
        event.item.id = "not-a-mission-prefix"

        screen.on_list_view_selected(event)
        screen._load_mission.assert_not_called()

    def test_empty_item_id(self):
        screen = TimeTravelScreen()
        screen._load_mission = Mock()

        event = Mock()
        event.item.id = ""

        screen.on_list_view_selected(event)
        screen._load_mission.assert_not_called()


class TestTimeTravelScreenGoBack(unittest.TestCase):
    """Tests for action_go_back."""

    def test_action_go_back(self):
        screen = TimeTravelScreen()
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)

        try:
            screen.action_go_back()
            mock_app.pop_screen.assert_called_once()
        finally:
            patcher.stop()


class TestTimeTravelScreenLoadMission(unittest.TestCase):
    """Tests for _load_mission."""

    def _make_screen(self):
        screen = TimeTravelScreen()
        screen.query_one = Mock(return_value=Mock())
        screen._render_events = Mock()
        screen._update_timeline = Mock()
        return screen

    def test_load_mission_sets_state(self):
        screen = self._make_screen()
        fake_events = [_make_event(index=i) for i in range(5)]

        with patch("tui.screens.time_travel.load_mission_events", return_value=fake_events):
            screen._load_mission("test-id")

        assert screen.mission_id == "test-id"
        assert screen.events == fake_events
        assert screen.total_events == 5
        assert screen.current_index == 0

    def test_load_mission_empty_events(self):
        screen = self._make_screen()

        with patch("tui.screens.time_travel.load_mission_events", return_value=[]):
            screen._load_mission("empty-mission")

        assert screen.total_events == 0


class TestTimeTravelScreenRenderEvents(unittest.TestCase):
    """Tests for _render_events."""

    def _make_screen(self):
        """Create screen with watchers mocked to avoid Textual runtime."""
        screen = TimeTravelScreen()
        screen.query_one = Mock(return_value=Mock())
        screen._update_timeline = Mock()
        return screen

    def test_render_events_empty(self):
        screen = self._make_screen()
        screen.events = []
        mock_scroll = Mock()
        screen.query_one = Mock(return_value=mock_scroll)

        # Call the real _render_events directly
        TimeTravelScreen._render_events(screen)

        mock_scroll.remove_children.assert_called_once()
        mock_scroll.mount.assert_called_once()

    def test_render_events_with_events(self):
        screen = self._make_screen()
        screen.events = [_make_event(index=i) for i in range(20)]
        screen.current_index = 10
        mock_scroll = Mock()
        screen.query_one = Mock(return_value=mock_scroll)

        with patch("tui.screens.time_travel.EventCard") as MockCard:
            mock_card_instance = Mock()
            MockCard.return_value = mock_card_instance
            TimeTravelScreen._render_events(screen)

        mock_scroll.remove_children.assert_called_once()
        # Context window: start=max(0,10-5)=5, end=min(20,10+15)=20 => 15 events
        assert mock_scroll.mount.call_count == 15


class TestTimeTravelScreenUpdateTimeline(unittest.TestCase):
    """Tests for _update_timeline."""

    def _make_screen(self):
        """Create screen with watchers mocked to avoid Textual runtime."""
        screen = TimeTravelScreen()
        screen.query_one = Mock(return_value=Mock())
        screen._render_events = Mock()
        return screen

    def test_update_with_events(self):
        screen = self._make_screen()
        screen.total_events = 10
        screen.current_index = 5

        mock_position = Mock()
        mock_bar = Mock()

        def query_one_side_effect(selector, cls=None):
            if selector == "#timeline-position":
                return mock_position
            if selector == "#timeline-bar":
                return mock_bar
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)

        TimeTravelScreen._update_timeline(screen)

        mock_position.update.assert_called_once()
        assert "6" in mock_position.update.call_args[0][0]  # 5+1 = 6
        assert "10" in mock_position.update.call_args[0][0]

    def test_update_no_events(self):
        screen = self._make_screen()
        # These are already 0 from init, but set explicitly for clarity
        mock_position = Mock()
        mock_bar = Mock()

        def query_one_side_effect(selector, cls=None):
            if selector == "#timeline-position":
                return mock_position
            if selector == "#timeline-bar":
                return mock_bar
            return Mock()

        screen.query_one = Mock(side_effect=query_one_side_effect)

        TimeTravelScreen._update_timeline(screen)

        mock_position.update.assert_called_once_with("0 / 0")
        assert mock_bar.progress == 0


class TestTimeTravelScreenOnMount(unittest.TestCase):
    """Tests for on_mount."""

    def test_on_mount_without_mission_id(self):
        screen = TimeTravelScreen()
        mock_header = Mock()
        screen.query_one = Mock(return_value=mock_header)
        screen._load_missions = Mock()
        screen._load_mission = Mock()

        screen.on_mount()

        screen._load_missions.assert_called_once()
        screen._load_mission.assert_not_called()

    def test_on_mount_with_mission_id(self):
        screen = TimeTravelScreen(mission_id="m-001")
        mock_header = Mock()
        screen.query_one = Mock(return_value=mock_header)
        screen._load_missions = Mock()
        screen._load_mission = Mock()

        screen.on_mount()

        screen._load_missions.assert_called_once()
        screen._load_mission.assert_called_once_with("m-001")


class TestTimeTravelScreenLoadMissions(unittest.TestCase):
    """Tests for _load_missions."""

    def test_load_missions_with_missions(self):
        screen = TimeTravelScreen()
        mock_list = Mock()
        mock_header = Mock()

        def query_one_side_effect(selector, cls=None):
            if selector == "#mission-list":
                return mock_list
            return mock_header

        screen.query_one = Mock(side_effect=query_one_side_effect)

        missions = [
            MissionInfo("m-1", "First mission", "T1", "T2", "success", 10),
            MissionInfo("m-2", "Second", "T1", None, "error", 5),
            MissionInfo("m-3", "", "T1", None, "in_progress", 3),
        ]

        with patch("tui.screens.time_travel.list_missions", return_value=missions):
            screen._load_missions()

        mock_list.clear.assert_called_once()
        assert mock_list.append.call_count == 3

    def test_load_missions_empty(self):
        screen = TimeTravelScreen()
        mock_list = Mock()

        screen.query_one = Mock(return_value=mock_list)

        with patch("tui.screens.time_travel.list_missions", return_value=[]):
            screen._load_missions()

        mock_list.clear.assert_called_once()
        # One "No recorded missions" item
        assert mock_list.append.call_count == 1


class TestTimeTravelScreenWatchCurrentIndex(unittest.TestCase):
    """Tests for watch_current_index reactive watcher."""

    def test_watch_calls_render_and_update(self):
        screen = TimeTravelScreen()
        screen._render_events = Mock()
        screen._update_timeline = Mock()

        screen.watch_current_index(5)

        screen._render_events.assert_called_once()
        screen._update_timeline.assert_called_once()


if __name__ == "__main__":
    unittest.main()
