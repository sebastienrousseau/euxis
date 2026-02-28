# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

#!/usr/bin/env python3
"""Comprehensive unit tests for tui.core.runner.

Tests cover:
- _SAFE_ID regex pattern matching
- _validate_id and _validate_provider validators
- AgentRun dataclass properties
- get_project_name and get_git_branch helper functions
- list_agent_outputs file listing
- run_agent, run_squad, run_combo async generators
"""

from __future__ import annotations

import asyncio
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import AsyncMock, patch

import pytest

from tui.core.runner import (
    _SAFE_ID,
    PROVIDERS,
    AgentRun,
    _validate_id,
    _validate_provider,
    get_git_branch,
    get_project_name,
    list_agent_outputs,
    run_agent,
    run_combo,
    run_squad,
)

# ============================================================================
# _SAFE_ID regex
# ============================================================================


class TestSafeIdRegex(unittest.TestCase):
    """Tests for the _SAFE_ID pattern used to validate identifiers."""

    def test_valid_simple_ids(self):
        """Valid lowercase alpha IDs should match."""
        for ident in ("architect", "debugger", "tester", "reviewer"):
            assert _SAFE_ID.match(ident), f"{ident!r} should match"

    def test_valid_ids_with_hyphens(self):
        """IDs containing hyphens should match."""
        for ident in ("code-reviewer", "a-b-c", "x-1"):
            assert _SAFE_ID.match(ident), f"{ident!r} should match"

    def test_valid_ids_with_underscores(self):
        """IDs containing underscores should match."""
        for ident in ("code_reviewer", "a_b_c", "x_1"):
            assert _SAFE_ID.match(ident), f"{ident!r} should match"

    def test_valid_ids_with_digits(self):
        """IDs containing digits (not at start) should match."""
        for ident in ("agent1", "v2", "test123"):
            assert _SAFE_ID.match(ident), f"{ident!r} should match"

    def test_valid_mixed_case(self):
        """IDs with mixed case should match."""
        for ident in ("Agent", "TestRunner", "myID"):
            assert _SAFE_ID.match(ident), f"{ident!r} should match"

    def test_valid_single_char(self):
        """Single-letter IDs should match."""
        assert _SAFE_ID.match("a")
        assert _SAFE_ID.match("Z")

    def test_invalid_starts_with_digit(self):
        """IDs starting with a digit should NOT match."""
        assert _SAFE_ID.match("1agent") is None
        assert _SAFE_ID.match("0test") is None

    def test_invalid_starts_with_hyphen(self):
        """IDs starting with hyphen should NOT match."""
        assert _SAFE_ID.match("-agent") is None

    def test_invalid_starts_with_underscore(self):
        """IDs starting with underscore should NOT match."""
        assert _SAFE_ID.match("_agent") is None

    def test_invalid_empty_string(self):
        """Empty string should NOT match."""
        assert _SAFE_ID.match("") is None

    def test_invalid_special_chars(self):
        """IDs with special characters should NOT match."""
        for ident in ("agent!", "test@1", "a.b", "foo/bar", "x y", "a+b"):
            assert _SAFE_ID.match(ident) is None, f"{ident!r} should NOT match"

    def test_invalid_too_long(self):
        """IDs exceeding 64 characters should NOT match fully."""
        long_id = "a" * 65
        # The regex allows 1 alpha + 0-63 more = 64 total
        assert _SAFE_ID.match(long_id) is None or _SAFE_ID.match(long_id).group() != long_id

    def test_valid_max_length(self):
        """IDs exactly 64 characters long should match."""
        max_id = "a" * 64
        m = _SAFE_ID.match(max_id)
        assert m is not None
        assert m.group() == max_id


# ============================================================================
# _validate_id / _validate_provider
# ============================================================================


class TestValidateId(unittest.TestCase):
    """Tests for _validate_id function."""

    def test_valid_id_passes(self):
        """Valid IDs should not raise."""
        _validate_id("architect", "agent_id")
        _validate_id("code-reviewer", "agent_id")
        _validate_id("Test123", "squad_id")

    def test_invalid_id_raises_valueerror(self):
        """Invalid IDs should raise ValueError with descriptive message."""
        with pytest.raises(ValueError, match="Invalid agent_id"):
            _validate_id("123bad", "agent_id")

    def test_empty_id_raises(self):
        """Empty string should raise ValueError."""
        with pytest.raises(ValueError, match="Invalid"):
            _validate_id("", "test_label")

    def test_special_char_id_raises(self):
        """IDs with special characters should raise ValueError."""
        with pytest.raises(ValueError, match="Invalid"):
            _validate_id("agent!bad", "my_label")

    def test_error_message_contains_label(self):
        """The error message should include the label."""
        with pytest.raises(ValueError, match="squad_id"):
            _validate_id("", "squad_id")


class TestValidateProvider(unittest.TestCase):
    """Tests for _validate_provider function."""

    def test_all_known_providers_pass(self):
        """Every entry in PROVIDERS should be accepted."""
        for provider in PROVIDERS:
            _validate_provider(provider)

    def test_unknown_provider_raises(self):
        """Unknown providers should raise ValueError."""
        with pytest.raises(ValueError, match="Unknown provider"):
            _validate_provider("gpt4")

    def test_empty_string_raises(self):
        """Empty string is not a valid provider."""
        with pytest.raises(ValueError, match="Unknown provider"):
            _validate_provider("")

    def test_case_sensitive(self):
        """Provider lookup is case-sensitive."""
        with pytest.raises(ValueError, match="Unknown provider"):
            _validate_provider("Claude")  # should be "claude"


# ============================================================================
# AgentRun dataclass
# ============================================================================


class TestAgentRun(unittest.TestCase):
    """Tests for the AgentRun dataclass."""

    def test_default_initialization(self):
        """Defaults should set up a running agent with no output."""
        run = AgentRun(agent_id="architect", task="review code", provider="claude")
        assert run.agent_id == "architect"
        assert run.task == "review code"
        assert run.provider == "claude"
        assert run.return_code is None
        assert run.output_path is None
        assert run.output_lines == []
        assert isinstance(run.started_at, float)

    def test_is_running_when_no_return_code(self):
        """is_running should be True when return_code is None."""
        run = AgentRun(agent_id="a", task="t", provider="claude")
        assert run.is_running is True

    def test_is_running_when_return_code_set(self):
        """is_running should be False when return_code is set."""
        run = AgentRun(agent_id="a", task="t", provider="claude", return_code=0)
        assert run.is_running is False

    def test_is_running_with_nonzero_return(self):
        """is_running should be False even with nonzero return code."""
        run = AgentRun(agent_id="a", task="t", provider="claude", return_code=1)
        assert run.is_running is False

    def test_elapsed_increases(self):
        """Elapsed should reflect passage of time."""
        run = AgentRun(
            agent_id="a", task="t", provider="claude",
            started_at=time.time() - 5.0,
        )
        assert run.elapsed >= 4.5  # allow small float variance

    def test_elapsed_display_seconds(self):
        """elapsed_display should show seconds for short durations."""
        run = AgentRun(
            agent_id="a", task="t", provider="claude",
            started_at=time.time() - 30.0,
        )
        display = run.elapsed_display
        assert display.endswith("s")
        assert "m" not in display

    def test_elapsed_display_minutes(self):
        """elapsed_display should show minutes and seconds for longer durations."""
        run = AgentRun(
            agent_id="a", task="t", provider="claude",
            started_at=time.time() - 125.0,  # 2m 5s
        )
        display = run.elapsed_display
        assert "m" in display
        assert "s" in display

    def test_elapsed_display_exact_minute(self):
        """elapsed_display at exactly 60s should show minutes format."""
        run = AgentRun(
            agent_id="a", task="t", provider="claude",
            started_at=time.time() - 60.0,
        )
        display = run.elapsed_display
        assert "m" in display

    def test_output_lines_accumulation(self):
        """output_lines should be a mutable list for appending."""
        run = AgentRun(agent_id="a", task="t", provider="claude")
        run.output_lines.append("line 1")
        run.output_lines.append("line 2")
        assert len(run.output_lines) == 2


# ============================================================================
# get_project_name / get_git_branch
# ============================================================================


class TestGetProjectName(unittest.TestCase):
    """Tests for get_project_name helper."""

    def test_returns_directory_name(self):
        """Should return the basename of the working directory."""
        with tempfile.TemporaryDirectory(prefix="my-project-") as td:
            name = get_project_name(working_dir=td)
            assert name == Path(td).name

    def test_defaults_to_cwd(self):
        """Without args it should use Path.cwd()."""
        name = get_project_name()
        assert name == Path.cwd().name

    def test_explicit_working_dir(self):
        """Explicit working_dir should be respected."""
        name = get_project_name(working_dir="/tmp")
        assert name == "tmp"


class TestGetGitBranch(unittest.TestCase):
    """Tests for get_git_branch helper."""

    def test_returns_branch_from_head_file(self):
        """Should parse a standard ref: refs/heads/ HEAD file."""
        with tempfile.TemporaryDirectory() as td:
            git_dir = Path(td) / ".git"
            git_dir.mkdir()
            head_file = git_dir / "HEAD"
            head_file.write_text("ref: refs/heads/main\n")
            assert get_git_branch(working_dir=td) == "main"

    def test_returns_feature_branch(self):
        """Should handle feature branch names."""
        with tempfile.TemporaryDirectory() as td:
            git_dir = Path(td) / ".git"
            git_dir.mkdir()
            head_file = git_dir / "HEAD"
            head_file.write_text("ref: refs/heads/feature/my-branch\n")
            assert get_git_branch(working_dir=td) == "feature/my-branch"

    def test_returns_none_for_detached_head(self):
        """Detached HEAD (raw SHA) should return None."""
        with tempfile.TemporaryDirectory() as td:
            git_dir = Path(td) / ".git"
            git_dir.mkdir()
            head_file = git_dir / "HEAD"
            head_file.write_text("abc123def456\n")
            assert get_git_branch(working_dir=td) is None

    def test_returns_none_when_no_git_dir(self):
        """No .git directory should return None."""
        with tempfile.TemporaryDirectory() as td:
            assert get_git_branch(working_dir=td) is None

    def test_returns_none_when_head_missing(self):
        """Existing .git dir without HEAD file should return None."""
        with tempfile.TemporaryDirectory() as td:
            (Path(td) / ".git").mkdir()
            assert get_git_branch(working_dir=td) is None


# ============================================================================
# list_agent_outputs
# ============================================================================


class TestListAgentOutputs(unittest.TestCase):
    """Tests for list_agent_outputs function."""

    def test_returns_empty_when_dir_missing(self):
        """Should return [] when the output directory does not exist."""
        with (
            tempfile.TemporaryDirectory() as td,
            patch("tui.core.runner.EUXIS_HOME", Path(td)),
        ):
            result = list_agent_outputs("architect", project="myproj")
            assert result == []

    def test_returns_sorted_md_files(self):
        """Should return .md files in reverse-sorted order."""
        with tempfile.TemporaryDirectory() as td:
            home = Path(td)
            out_dir = home / "data" / "projects" / "myproj" / "architect" / "output"
            out_dir.mkdir(parents=True)

            (out_dir / "2024-01-01.md").write_text("first")
            (out_dir / "2024-01-02.md").write_text("second")
            (out_dir / "2024-01-03.md").write_text("third")
            (out_dir / "notes.txt").write_text("not md")

            with patch("tui.core.runner.EUXIS_HOME", home):
                result = list_agent_outputs("architect", project="myproj")

            assert len(result) == 3
            names = [p.name for p in result]
            assert names == ["2024-01-03.md", "2024-01-02.md", "2024-01-01.md"]

    def test_returns_empty_when_no_md_files(self):
        """Should return [] when directory exists but has no .md files."""
        with tempfile.TemporaryDirectory() as td:
            home = Path(td)
            out_dir = home / "data" / "projects" / "myproj" / "debugger" / "output"
            out_dir.mkdir(parents=True)
            (out_dir / "notes.txt").write_text("not md")

            with patch("tui.core.runner.EUXIS_HOME", home):
                result = list_agent_outputs("debugger", project="myproj")

            assert result == []


# ============================================================================
# run_agent / run_squad / run_combo async generators
# ============================================================================


class TestRunAgentAsync(unittest.TestCase):
    """Tests for run_agent async generator."""

    def test_rejects_invalid_agent_id(self):
        """Invalid agent_id should raise ValueError immediately."""
        async def _run():
            gen = run_agent("123-bad!", "do stuff")
            # Must advance the generator to trigger validation
            with pytest.raises(ValueError, match="Invalid agent_id"):
                await gen.__anext__()

        asyncio.run(_run())

    def test_rejects_unknown_provider(self):
        """Unknown provider should raise ValueError."""
        async def _run():
            gen = run_agent("architect", "task", provider="unknown")
            with pytest.raises(ValueError, match="Unknown provider"):
                await gen.__anext__()

        asyncio.run(_run())

    def test_yields_output_lines(self):
        """Should yield decoded lines from subprocess stdout."""
        async def _run():
            mock_stdout = AsyncMock()
            mock_stdout.readline = AsyncMock(side_effect=[
                b"line one\n",
                b"line two\n",
                b"",
            ])

            mock_process = AsyncMock()
            mock_process.stdout = mock_stdout
            mock_process.wait = AsyncMock()

            with (
                patch("tui.core.runner.asyncio.create_subprocess_exec", return_value=mock_process),
                patch("tui.core.runner.Path.exists", return_value=True),
            ):
                lines = []
                async for line in run_agent("architect", "task", provider="claude"):
                    lines.append(line)

            assert lines == ["line one", "line two"]

        asyncio.run(_run())

    def test_handles_no_stdout(self):
        """Should return cleanly when stdout is None."""
        async def _run():
            mock_process = AsyncMock()
            mock_process.stdout = None

            with (
                patch("tui.core.runner.asyncio.create_subprocess_exec", return_value=mock_process),
                patch("tui.core.runner.Path.exists", return_value=True),
            ):
                lines = []
                async for line in run_agent("architect", "task", provider="claude"):
                    lines.append(line)

            assert lines == []

        asyncio.run(_run())


class TestRunSquadAsync(unittest.TestCase):
    """Tests for run_squad async generator."""

    def test_rejects_invalid_squad_id(self):
        """Invalid squad_id should raise ValueError."""
        async def _run():
            gen = run_squad("!!bad", "task")
            with pytest.raises(ValueError, match="Invalid squad_id"):
                await gen.__anext__()

        asyncio.run(_run())

    def test_yields_output_lines(self):
        """Should yield decoded lines from subprocess."""
        async def _run():
            mock_stdout = AsyncMock()
            mock_stdout.readline = AsyncMock(side_effect=[
                b"squad output 1\n",
                b"",
            ])

            mock_process = AsyncMock()
            mock_process.stdout = mock_stdout
            mock_process.wait = AsyncMock()

            with (
                patch("tui.core.runner.asyncio.create_subprocess_exec", return_value=mock_process),
                patch("tui.core.runner.Path.exists", return_value=True),
            ):
                lines = []
                async for line in run_squad("alpha-squad", "deploy"):
                    lines.append(line)

            assert lines == ["squad output 1"]

        asyncio.run(_run())


class TestRunComboAsync(unittest.TestCase):
    """Tests for run_combo async generator."""

    def test_rejects_invalid_combo_id(self):
        """Invalid combo_id should raise ValueError."""
        async def _run():
            gen = run_combo("bad id!", "task")
            with pytest.raises(ValueError, match="Invalid combo_id"):
                await gen.__anext__()

        asyncio.run(_run())

    def test_yields_output_lines(self):
        """Should yield decoded lines from subprocess."""
        async def _run():
            mock_stdout = AsyncMock()
            mock_stdout.readline = AsyncMock(side_effect=[
                b"combo step 1\n",
                b"combo step 2\n",
                b"",
            ])

            mock_process = AsyncMock()
            mock_process.stdout = mock_stdout
            mock_process.wait = AsyncMock()

            with (
                patch("tui.core.runner.asyncio.create_subprocess_exec", return_value=mock_process),
                patch("tui.core.runner.Path.exists", return_value=True),
            ):
                lines = []
                async for line in run_combo("review-chain", "audit"):
                    lines.append(line)

            assert lines == ["combo step 1", "combo step 2"]

        asyncio.run(_run())

    def test_handles_no_stdout(self):
        """Should return cleanly when stdout is None."""
        async def _run():
            mock_process = AsyncMock()
            mock_process.stdout = None

            with (
                patch("tui.core.runner.asyncio.create_subprocess_exec", return_value=mock_process),
                patch("tui.core.runner.Path.exists", return_value=True),
            ):
                lines = []
                async for line in run_combo("review-chain", "audit"):
                    lines.append(line)

            assert lines == []

        asyncio.run(_run())


if __name__ == "__main__":
    unittest.main()
