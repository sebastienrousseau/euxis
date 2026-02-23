# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Comprehensive unit tests for AgentScreen.

Tests cover: init, compose, on_mount header setup, on_input_submitted,
_execute_agent (success, OSError, RuntimeError, ValueError), _update_elapsed,
actions go_back / clear_output.
"""

from __future__ import annotations

import asyncio
import unittest
from unittest.mock import Mock, PropertyMock, patch

from hypothesis import given
from hypothesis import strategies as st

from tui.core.registry import Agent
from tui.core.runner import AgentRun
from tui.screens.agent import AgentScreen


def _patch_screen_app(screen, mock_app):
    """Bypass Textual's read-only app property for testing."""
    patcher = patch.object(
        type(screen), "app", new_callable=PropertyMock, return_value=mock_app
    )
    patcher.start()
    return patcher


def _make_agent(**overrides):
    defaults = {
        "id": "architect",
        "tier": "core",
        "version": "v0.0.2",
        "tags": ("design", "plan", "review"),
        "activation": "default",
    }
    defaults.update(overrides)
    return Agent(**defaults)


def _make_mock_app():
    mock_app = Mock()
    mock_app.project_name = "test-project"
    mock_app.git_branch = "main"
    mock_app.fleet_registry = Mock()
    mock_app.fleet_registry.agents = [_make_agent()]
    mock_app.config = Mock()
    mock_app.config.default_provider = "claude"
    mock_app.config.save = Mock()
    mock_app.config.add_recent_agent = Mock()
    mock_app.config.add_recent_command = Mock()
    return mock_app


class TestAgentScreenInit(unittest.TestCase):
    """Tests for AgentScreen initialization."""

    def test_init_defaults(self):
        agent = _make_agent()
        screen = AgentScreen(agent)
        assert screen.agent is agent
        assert screen.provider == "claude"
        assert screen._run is None
        assert screen._timer_task is None

    def test_init_custom_provider(self):
        agent = _make_agent()
        screen = AgentScreen(agent, provider="gemini")
        assert screen.provider == "gemini"

    def test_bindings(self):
        screen = AgentScreen(_make_agent())
        binding_keys = [b[0] for b in screen.BINDINGS]
        assert "ctrl+k" in binding_keys
        assert "escape" in binding_keys
        assert "ctrl+l" in binding_keys

    def test_euxis_app_property(self):
        screen = AgentScreen(_make_agent())
        mock_app = _make_mock_app()
        patcher = _patch_screen_app(screen, mock_app)
        try:
            assert screen.euxis_app is mock_app
        finally:
            patcher.stop()


class TestAgentScreenCompose(unittest.TestCase):
    """Tests for AgentScreen compose method."""

    @patch("tui.screens.agent.OutputPanel")
    @patch("tui.screens.agent.Input")
    @patch("tui.screens.agent.Static")
    @patch("tui.screens.agent.Container")
    @patch("tui.screens.agent.Horizontal")
    @patch("tui.screens.agent.ETXHeader")
    def test_compose_yields_widgets(self, *mocks):
        screen = AgentScreen(_make_agent())
        result = list(screen.compose())
        assert len(result) > 0


class TestAgentScreenOnMount(unittest.TestCase):
    """Tests for AgentScreen on_mount."""

    def setUp(self):
        self.agent = _make_agent()
        self.screen = AgentScreen(self.agent, provider="gemini")
        self.mock_app = _make_mock_app()
        self._patcher = _patch_screen_app(self.screen, self.mock_app)

        self.mock_header = Mock()
        self.mock_name_display = Mock()
        self.mock_provider_display = Mock()
        self.mock_elapsed_display = Mock()
        self.mock_input = Mock()
        self.mock_output = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            from tui.widgets.header import ETXHeader
            from tui.widgets.output_panel import OutputPanel

            def _matches_selector(target, cls, name):
                return target is cls or (
                    hasattr(target, "__name__") and target.__name__ == name
                )

            if _matches_selector(selector, ETXHeader, "ETXHeader"):
                return self.mock_header
            if _matches_selector(selector, OutputPanel, "OutputPanel"):
                return self.mock_output
            mapping = {
                "#agent-name-display": self.mock_name_display,
                "#agent-provider-display": self.mock_provider_display,
                "#agent-elapsed-display": self.mock_elapsed_display,
                "#task-input": self.mock_input,
            }
            if selector in mapping:
                return mapping[selector]
            return Mock()

        self.screen.query_one = Mock(side_effect=query_one_side_effect)

    def tearDown(self):
        self._patcher.stop()

    def test_header_configured(self):
        self.screen.on_mount()
        assert self.mock_header.project == "test-project"
        assert self.mock_header.branch == "main"
        assert self.mock_header.provider == "gemini"

    def test_name_display_core_tier(self):
        self.screen.on_mount()
        call_args = self.mock_name_display.update.call_args[0][0]
        assert "[yellow]CORE[/]" in call_args
        assert "architect" in call_args

    def test_name_display_fleet_tier(self):
        self.screen.agent = _make_agent(tier="fleet")
        self.screen.on_mount()
        call_args = self.mock_name_display.update.call_args[0][0]
        assert "[cyan]FLEET[/]" in call_args

    def test_provider_display(self):
        self.screen.on_mount()
        call_args = self.mock_provider_display.update.call_args[0][0]
        assert "gemini" in call_args

    def test_elapsed_display_ready(self):
        self.screen.on_mount()
        call_args = self.mock_elapsed_display.update.call_args[0][0]
        assert "Ready" in call_args

    def test_input_focused(self):
        self.screen.on_mount()
        self.mock_input.focus.assert_called_once()

    def test_welcome_messages(self):
        self.screen.on_mount()
        calls = [c[0][0] for c in self.mock_output.write_status.call_args_list]
        assert any("architect" in c for c in calls)
        assert any("CORE" in c for c in calls)
        assert any("gemini" in c for c in calls)
        self.mock_output.write_separator.assert_called()


class TestAgentScreenInputSubmitted(unittest.TestCase):
    """Tests for on_input_submitted."""

    def setUp(self):
        self.agent = _make_agent()
        self.screen = AgentScreen(self.agent)
        self.mock_app = _make_mock_app()
        self._patcher = _patch_screen_app(self.screen, self.mock_app)

        self.mock_input = Mock()
        self.mock_input.disabled = False
        self.mock_output = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            if selector == "#task-input" or (args and args[0].__name__ == "Input"):
                return self.mock_input
            return self.mock_output

        self.screen.query_one = Mock(side_effect=query_one_side_effect)
        self.screen.run_worker = Mock()

    def tearDown(self):
        self._patcher.stop()

    def test_ignores_other_inputs(self):
        event = Mock()
        event.input.id = "other-input"
        loop = asyncio.new_event_loop()
        loop.run_until_complete(self.screen.on_input_submitted(event))
        loop.close()
        self.screen.run_worker.assert_not_called()

    def test_ignores_empty_input(self):
        event = Mock()
        event.input.id = "task-input"
        event.value = "   "
        loop = asyncio.new_event_loop()
        loop.run_until_complete(self.screen.on_input_submitted(event))
        loop.close()
        self.screen.run_worker.assert_not_called()

    def test_disables_input_and_tracks_config(self):
        event = Mock()
        event.input.id = "task-input"
        event.value = "do something"
        loop = asyncio.new_event_loop()
        loop.run_until_complete(self.screen.on_input_submitted(event))
        loop.close()

        assert self.mock_input.disabled is True
        self.mock_app.config.add_recent_agent.assert_called_once_with("architect")
        self.mock_app.config.add_recent_command.assert_called_once()
        self.mock_app.config.save.assert_called_once()

    def test_creates_agent_run(self):
        event = Mock()
        event.input.id = "task-input"
        event.value = "build feature"
        loop = asyncio.new_event_loop()
        loop.run_until_complete(self.screen.on_input_submitted(event))
        loop.close()

        assert self.screen._run is not None
        assert self.screen._run.agent_id == "architect"
        assert self.screen._run.task == "build feature"

    def test_starts_worker(self):
        event = Mock()
        event.input.id = "task-input"
        event.value = "build feature"
        loop = asyncio.new_event_loop()
        loop.run_until_complete(self.screen.on_input_submitted(event))
        loop.close()

        self.screen.run_worker.assert_called_once()


class TestAgentScreenExecuteAgent(unittest.TestCase):
    """Tests for _execute_agent async method."""

    def setUp(self):
        self.agent = _make_agent()
        self.screen = AgentScreen(self.agent)
        self.mock_app = _make_mock_app()
        self._patcher = _patch_screen_app(self.screen, self.mock_app)

        self.mock_input = Mock()
        self.mock_output = Mock()

        def query_one_side_effect(selector, *args, **kwargs):
            if selector == "#task-input":
                return self.mock_input
            return self.mock_output

        self.screen.query_one = Mock(side_effect=query_one_side_effect)
        self.screen.notify = Mock()
        self.screen._run = AgentRun(
            agent_id="architect", task="test", provider="claude"
        )

    def tearDown(self):
        self._patcher.stop()

    def _run_execute(self, task="test"):
        loop = asyncio.new_event_loop()
        try:
            loop.run_until_complete(self.screen._execute_agent(task))
        finally:
            loop.close()

    @patch("tui.screens.agent.run_agent")
    def test_success_streams_output(self, mock_run):
        async def fake_stream(*a, **kw):
            for line in ["line1", "line2"]:
                yield line

        mock_run.return_value = fake_stream()
        self._run_execute()

        assert self.mock_output.write_line.call_count == 2
        assert self.screen._run.return_code == 0
        self.screen.notify.assert_called_once()
        assert "complete" in self.screen.notify.call_args[0][0].lower()

    @patch("tui.screens.agent.run_agent")
    def test_success_appends_to_run_output(self, mock_run):
        async def fake_stream(*a, **kw):
            yield "hello"

        mock_run.return_value = fake_stream()
        self._run_execute()

        assert "hello" in self.screen._run.output_lines

    @patch("tui.screens.agent.run_agent")
    def test_oserror_handling(self, mock_run):
        async def error_stream(*a, **kw):
            msg = "binary not found"
            raise OSError(msg)
            yield

        mock_run.return_value = error_stream()
        self._run_execute()

        assert self.screen._run.return_code == 1
        error_calls = [
            c[0][0] for c in self.mock_output.write_status.call_args_list
        ]
        assert any("binary not found" in c for c in error_calls)

    @patch("tui.screens.agent.run_agent")
    def test_runtime_error_handling(self, mock_run):
        async def error_stream(*a, **kw):
            msg = "agent crashed"
            raise RuntimeError(msg)
            yield

        mock_run.return_value = error_stream()
        self._run_execute()

        assert self.screen._run.return_code == 1

    @patch("tui.screens.agent.run_agent")
    def test_value_error_handling(self, mock_run):
        async def error_stream(*a, **kw):
            msg = "bad input"
            raise ValueError(msg)
            yield

        mock_run.return_value = error_stream()
        self._run_execute()

        assert self.screen._run.return_code == 1

    @patch("tui.screens.agent.run_agent")
    def test_finally_re_enables_input(self, mock_run):
        async def fake_stream(*a, **kw):
            yield "ok"

        mock_run.return_value = fake_stream()
        self.screen._timer_task = Mock()
        self._run_execute()

        assert self.mock_input.disabled is False
        assert self.mock_input.value == ""
        self.mock_input.focus.assert_called()
        self.screen._timer_task.cancel.assert_called_once()

    @patch("tui.screens.agent.run_agent")
    def test_finally_on_error(self, mock_run):
        async def error_stream(*a, **kw):
            msg = "fail"
            raise OSError(msg)
            yield

        mock_run.return_value = error_stream()
        self._run_execute()

        # Input should still be re-enabled even on error
        assert self.mock_input.disabled is False


class TestAgentScreenUpdateElapsed(unittest.TestCase):
    """Tests for _update_elapsed timer."""

    def test_update_elapsed_stops_when_not_running(self):
        agent = _make_agent()
        screen = AgentScreen(agent)
        mock_display = Mock()
        screen.query_one = Mock(return_value=mock_display)

        run = AgentRun(agent_id="test", task="t", provider="claude")
        run.return_code = 0  # not running
        screen._run = run

        loop = asyncio.new_event_loop()
        loop.run_until_complete(screen._update_elapsed())
        loop.close()

        # Should update with final green display
        call_args = mock_display.update.call_args[0][0]
        assert "[green]" in call_args


class TestAgentScreenActions(unittest.TestCase):
    """Tests for action methods."""

    def setUp(self):
        self.screen = AgentScreen(_make_agent())
        self.mock_app = _make_mock_app()
        self._patcher = _patch_screen_app(self.screen, self.mock_app)

    def tearDown(self):
        self._patcher.stop()

    def test_action_go_back(self):
        self.screen.action_go_back()
        self.mock_app.pop_screen.assert_called_once()

    def test_action_clear_output(self):
        mock_output = Mock()
        self.screen.query_one = Mock(return_value=mock_output)
        self.screen.action_clear_output()
        mock_output.clear.assert_called_once()


class TestAgentScreenPropertyBased(unittest.TestCase):
    """Property-based tests for AgentScreen robustness."""

    @given(
        agent_id=st.text(
            min_size=1,
            max_size=30,
            alphabet=st.characters(whitelist_categories=["Lu", "Ll", "Nd"]),
        ),
        provider=st.sampled_from(["claude", "gemini", "ollama", "codex"]),
    )
    def test_init_robustness(self, agent_id, provider):
        agent = _make_agent(id=agent_id)
        screen = AgentScreen(agent, provider=provider)
        assert screen.agent.id == agent_id
        assert screen.provider == provider


if __name__ == "__main__":
    unittest.main()
