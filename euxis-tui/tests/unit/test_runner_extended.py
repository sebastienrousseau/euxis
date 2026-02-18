# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Extended unit tests for tui.core.runner — coverage of uncovered branches.

Covers:
- ExecutionMetrics.duration_seconds property
- AgentExecutionResult construction
- AgentRunner.__init__ and execute (success, failure, timeout, FileNotFoundError)
- run_agent, run_squad, run_combo subprocess mocking
- get_project_path (home prefix, non-home)
- ArbiterAnalyzer: analyze (cache hit/miss/eviction), explain_budget, _run_arbiter,
  _parse_response, _parse_recovery_step, _fallback_analysis, _detect_cascading_failure
"""

from __future__ import annotations

import asyncio
import time
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from tui.core.runner import (
    AgentExecutionResult,
    AgentRunner,
    ArbiterAnalyzer,
    ErrorAnalysis,
    ExecutionMetrics,
    RecoveryStep,
    _analysis_cache,
    get_project_path,
    run_agent,
    run_combo,
    run_squad,
)


# ============================================================================
# ExecutionMetrics
# ============================================================================


class TestExecutionMetrics:
    """Tests for ExecutionMetrics dataclass."""

    def test_duration_seconds_positive(self):
        """duration_seconds should return end_time - start_time."""
        m = ExecutionMetrics(start_time=100.0, end_time=105.5)
        assert m.duration_seconds == pytest.approx(5.5)

    def test_duration_seconds_zero_when_same(self):
        """duration_seconds should be 0.0 when start == end."""
        m = ExecutionMetrics(start_time=100.0, end_time=100.0)
        assert m.duration_seconds == 0.0

    def test_duration_seconds_clamps_negative(self):
        """duration_seconds should return 0.0 when end < start (max clamp)."""
        m = ExecutionMetrics(start_time=200.0, end_time=100.0)
        assert m.duration_seconds == 0.0

    def test_optional_fields(self):
        """Optional fields should have proper defaults."""
        m = ExecutionMetrics(start_time=0.0, end_time=1.0)
        assert m.peak_memory_kb == 0
        assert m.cpu_time_seconds == 0.0
        assert m.exit_code is None


# ============================================================================
# AgentExecutionResult
# ============================================================================


class TestAgentExecutionResult:
    """Tests for AgentExecutionResult dataclass."""

    def test_success_construction(self):
        """Successful result should have expected fields."""
        r = AgentExecutionResult(
            success=True, output="hello", error="", exit_code=0,
        )
        assert r.success is True
        assert r.output == "hello"
        assert r.error == ""
        assert r.exit_code == 0
        assert r.timeout_occurred is False
        assert r.metrics is None

    def test_failure_construction(self):
        """Failure result with metrics should round-trip correctly."""
        metrics = ExecutionMetrics(start_time=1.0, end_time=2.0, exit_code=1)
        r = AgentExecutionResult(
            success=False, output="", error="boom", exit_code=1,
            timeout_occurred=False, metrics=metrics,
        )
        assert r.success is False
        assert r.metrics is metrics
        assert r.metrics.duration_seconds == pytest.approx(1.0)

    def test_timeout_construction(self):
        """Timeout result should have timeout_occurred=True."""
        r = AgentExecutionResult(
            success=False, output="", error="Execution timed out.",
            exit_code=None, timeout_occurred=True,
        )
        assert r.timeout_occurred is True
        assert r.exit_code is None


# ============================================================================
# AgentRunner
# ============================================================================


class TestAgentRunner:
    """Tests for AgentRunner.__init__ and execute."""

    def _make_agent(self, agent_id: str = "architect"):
        return SimpleNamespace(id=agent_id)

    def test_init_stores_attributes(self):
        """__init__ should store agent, provider, and timeout."""
        agent = self._make_agent()
        runner = AgentRunner(agent, "claude", timeout=60.0)
        assert runner.agent is agent
        assert runner.provider == "claude"
        assert runner.timeout == 60.0

    @pytest.mark.asyncio
    async def test_execute_success(self):
        """Successful subprocess (exit code 0) should return success=True."""
        agent = self._make_agent()
        runner = AgentRunner(agent, "claude", timeout=10.0)

        mock_process = AsyncMock()
        mock_process.communicate = AsyncMock(return_value=(b"output data", b""))
        mock_process.returncode = 0
        mock_process.kill = MagicMock()

        with (
            patch("tui.core.runner.asyncio.create_subprocess_exec", return_value=mock_process),
            patch("tui.core.runner.Path.exists", return_value=True),
        ):
            result = await runner.execute("review code")

        assert result.success is True
        assert result.output == "output data"
        assert result.error == ""
        assert result.exit_code == 0
        assert result.timeout_occurred is False
        assert result.metrics is not None
        assert result.metrics.duration_seconds >= 0.0

    @pytest.mark.asyncio
    async def test_execute_failure_nonzero_exit(self):
        """Non-zero exit code should return success=False."""
        agent = self._make_agent()
        runner = AgentRunner(agent, "claude", timeout=10.0)

        mock_process = AsyncMock()
        mock_process.communicate = AsyncMock(return_value=(b"", b"Error occurred"))
        mock_process.returncode = 1
        mock_process.kill = MagicMock()

        with (
            patch("tui.core.runner.asyncio.create_subprocess_exec", return_value=mock_process),
            patch("tui.core.runner.Path.exists", return_value=True),
        ):
            result = await runner.execute("bad task")

        assert result.success is False
        assert result.exit_code == 1
        assert result.error == "Error occurred"
        assert result.timeout_occurred is False

    @pytest.mark.asyncio
    async def test_execute_timeout(self):
        """asyncio.TimeoutError should produce timeout result."""
        agent = self._make_agent()
        runner = AgentRunner(agent, "claude", timeout=0.001)

        mock_process = AsyncMock()
        mock_process.communicate = AsyncMock(side_effect=asyncio.TimeoutError)
        mock_process.kill = MagicMock(return_value=None)  # non-awaitable
        mock_process.wait = AsyncMock()

        with (
            patch("tui.core.runner.asyncio.create_subprocess_exec", return_value=mock_process),
            patch("tui.core.runner.asyncio.wait_for", side_effect=asyncio.TimeoutError),
            patch("tui.core.runner.Path.exists", return_value=True),
        ):
            result = await runner.execute("slow task")

        assert result.success is False
        assert result.timeout_occurred is True
        assert result.error == "Execution timed out."
        assert result.exit_code is None

    @pytest.mark.asyncio
    async def test_execute_file_not_found(self):
        """FileNotFoundError should be caught and returned as error result."""
        agent = self._make_agent()
        runner = AgentRunner(agent, "claude")

        with (
            patch(
                "tui.core.runner.asyncio.create_subprocess_exec",
                side_effect=FileNotFoundError("euxis not found"),
            ),
            patch("tui.core.runner.Path.exists", return_value=False),
        ):
            result = await runner.execute("task")

        assert result.success is False
        assert "euxis not found" in result.error
        assert result.timeout_occurred is False
        assert result.metrics is not None

    @pytest.mark.asyncio
    async def test_execute_invalid_agent_id_raises(self):
        """Invalid agent ID should raise ValueError."""
        agent = SimpleNamespace(id="123-bad!")
        runner = AgentRunner(agent, "claude")

        with pytest.raises(ValueError, match="Invalid agent_id"):
            await runner.execute("task")

    @pytest.mark.asyncio
    async def test_execute_euxis_bin_fallback(self):
        """When ~/bin/euxis does not exist, should fall back to EUXIS_HOME/bin/euxis."""
        agent = self._make_agent()
        runner = AgentRunner(agent, "claude", timeout=10.0)

        mock_process = AsyncMock()
        mock_process.communicate = AsyncMock(return_value=(b"ok", b""))
        mock_process.returncode = 0

        call_args = []

        async def mock_create(*args, **kwargs):
            call_args.append(args)
            return mock_process

        # First call to Path.exists returns False (~/bin/euxis not found)
        with (
            patch("tui.core.runner.asyncio.create_subprocess_exec", side_effect=mock_create),
            patch("tui.core.runner.Path.exists", return_value=False),
        ):
            result = await runner.execute("task")

        assert result.success is True


# ============================================================================
# get_project_path
# ============================================================================


class TestGetProjectPath:
    """Tests for get_project_path."""

    def test_home_prefix(self):
        """Directory under home should be abbreviated with ~/."""
        home = str(Path.home())
        subdir = home + "/projects/my-app"
        result = get_project_path(working_dir=subdir)
        assert result.startswith("~/")
        assert "my-app" in result

    def test_non_home_path(self):
        """Directory not under home should return absolute path."""
        result = get_project_path(working_dir="/tmp/test-project")
        assert result == "/tmp/test-project"

    def test_defaults_to_cwd(self):
        """Without args, should use cwd."""
        result = get_project_path()
        # Should either start with ~/ or be an absolute path
        assert result.startswith("~/") or result.startswith("/")


# ============================================================================
# ArbiterAnalyzer
# ============================================================================


class TestArbiterAnalyzerInit:
    """Tests for ArbiterAnalyzer.__init__."""

    def test_default_timeout(self):
        """Default timeout should be 15.0."""
        analyzer = ArbiterAnalyzer()
        assert analyzer.timeout == 15.0

    def test_custom_timeout(self):
        """Custom timeout should be stored."""
        analyzer = ArbiterAnalyzer(timeout=5.0)
        assert analyzer.timeout == 5.0


class TestArbiterAnalyzerAnalyze:
    """Tests for ArbiterAnalyzer.analyze."""

    def setup_method(self):
        """Clear the analysis cache before each test."""
        _analysis_cache.clear()

    @pytest.mark.asyncio
    async def test_cache_hit(self):
        """Pre-populated cache should be returned without calling arbiter."""
        cached = ErrorAnalysis(
            summary="cached result", root_cause="cached",
            recovery_steps=[], severity="info", confidence=0.9,
        )
        cache_key = f"architect:{hash('some error')}"
        _analysis_cache[cache_key] = cached

        analyzer = ArbiterAnalyzer()
        result = await analyzer.analyze("architect", "T1", "some error")
        assert result.summary == "cached result"

    @pytest.mark.asyncio
    async def test_cache_miss_arbiter_success(self):
        """On cache miss with successful arbiter, should return parsed analysis."""
        arbiter_response = (
            "SUMMARY: API rate limit hit\n"
            "ROOT_CAUSE: Too many requests\n"
            "SEVERITY: warning\n"
            "CONFIDENCE: 0.85\n"
            "CAUSED_BY: NONE\n"
            "AUTO_RECOVER: YES\n"
            "RECOVERY:\n"
            "- Wait 60 seconds|0.95|LIVE\n"
            "- Reduce concurrency|0.80|SANDBOX\n"
            "END\n"
        )
        analyzer = ArbiterAnalyzer()
        with patch.object(analyzer, "_run_arbiter", return_value=arbiter_response):
            result = await analyzer.analyze("architect", "T1", "rate limit error")

        assert result.summary == "API rate limit hit"
        assert result.root_cause == "Too many requests"
        assert result.severity == "warning"
        assert result.confidence == pytest.approx(0.85)
        assert result.caused_by_agent is None
        assert result.can_auto_recover is True
        assert len(result.recovery_steps) == 2
        assert result.recovery_steps[0].action == "Wait 60 seconds"
        assert result.recovery_steps[1].requires_sandbox is True

    @pytest.mark.asyncio
    async def test_cache_miss_arbiter_failure_fallback(self):
        """On arbiter failure, should fall back to rule-based classification."""
        analyzer = ArbiterAnalyzer()
        with patch.object(analyzer, "_run_arbiter", side_effect=asyncio.TimeoutError):
            result = await analyzer.analyze(
                "architect", "T1", "429 rate limit exceeded"
            )

        assert "re-trying" in result.summary.lower() or "retry" in result.summary.lower()
        assert result.can_auto_recover is True

    @pytest.mark.asyncio
    async def test_cache_eviction_at_max_size(self):
        """When cache is at max size (50), oldest entry should be evicted."""
        # Fill the cache with 50 entries
        for i in range(50):
            _analysis_cache[f"key-{i}"] = ErrorAnalysis(
                summary=f"entry-{i}", root_cause="r", recovery_steps=[],
                severity="info", confidence=0.5,
            )

        assert len(_analysis_cache) == 50
        first_key = next(iter(_analysis_cache))
        assert first_key == "key-0"

        analyzer = ArbiterAnalyzer()
        with patch.object(analyzer, "_run_arbiter", side_effect=ValueError("fail")):
            await analyzer.analyze("newagent", "T1", "some unique error xyz")

        # key-0 should have been evicted
        assert "key-0" not in _analysis_cache
        assert len(_analysis_cache) == 50  # still 50 (evicted 1, added 1)


class TestArbiterAnalyzerExplainBudget:
    """Tests for ArbiterAnalyzer.explain_budget."""

    @pytest.mark.asyncio
    async def test_success_with_recent_tasks(self):
        """Successful arbiter response with recent_tasks should return explanation."""
        analyzer = ArbiterAnalyzer()
        with patch.object(
            analyzer, "_run_arbiter",
            return_value="High cost due to complex code review tasks.",
        ):
            result = await analyzer.explain_budget(
                "architect", "T1", cost=4.50, budget=5.00,
                request_count=100, recent_tasks=["review PR #1", "audit codebase"],
            )

        assert "complex code review" in result.lower()

    @pytest.mark.asyncio
    async def test_success_without_recent_tasks(self):
        """Successful arbiter response without recent_tasks should work."""
        analyzer = ArbiterAnalyzer()
        with patch.object(
            analyzer, "_run_arbiter",
            return_value="Budget nearing limit.",
        ):
            result = await analyzer.explain_budget(
                "architect", "T1", cost=4.50, budget=5.00, request_count=100,
            )

        assert "budget" in result.lower()

    @pytest.mark.asyncio
    async def test_fallback_high_per_request_cost(self):
        """Fallback for high per-request cost (>0.05)."""
        analyzer = ArbiterAnalyzer()
        with patch.object(analyzer, "_run_arbiter", side_effect=asyncio.TimeoutError):
            result = await analyzer.explain_budget(
                "architect", "T1", cost=3.0, budget=5.0,
                request_count=10,  # 0.30 per request > 0.05
            )

        assert "expensive" in result.lower() or "$" in result

    @pytest.mark.asyncio
    async def test_fallback_high_request_count(self):
        """Fallback for high request count (>50) but low per-request cost."""
        analyzer = ArbiterAnalyzer()
        with patch.object(analyzer, "_run_arbiter", side_effect=OSError("fail")):
            result = await analyzer.explain_budget(
                "architect", "T1", cost=2.0, budget=5.0,
                request_count=100,  # 0.02 per request (< 0.05)
            )

        assert "100 requests" in result

    @pytest.mark.asyncio
    async def test_fallback_default_case(self):
        """Fallback generic case (low cost, low requests)."""
        analyzer = ArbiterAnalyzer()
        with patch.object(analyzer, "_run_arbiter", side_effect=ValueError("fail")):
            result = await analyzer.explain_budget(
                "architect", "T1", cost=2.0, budget=5.0,
                request_count=10,  # 0.20/req but cost/budget < threshold triggers else
            )

        # This hits the high per_request_cost branch (0.20 > 0.05)
        assert "architect" in result

    @pytest.mark.asyncio
    async def test_fallback_default_truly_generic(self):
        """Fallback for the true else case (low per-req cost, few requests)."""
        analyzer = ArbiterAnalyzer()
        with patch.object(analyzer, "_run_arbiter", side_effect=ValueError("fail")):
            result = await analyzer.explain_budget(
                "architect", "T1", cost=1.0, budget=5.0,
                request_count=30,  # 0.033/req < 0.05, and 30 < 50
            )

        assert "architect" in result
        assert "%" in result

    @pytest.mark.asyncio
    async def test_fallback_empty_response(self):
        """Empty arbiter response should trigger ValueError -> fallback."""
        analyzer = ArbiterAnalyzer()
        with patch.object(analyzer, "_run_arbiter", return_value="   "):
            result = await analyzer.explain_budget(
                "architect", "T1", cost=1.0, budget=5.0, request_count=30,
            )

        # Should hit fallback
        assert "architect" in result


class TestArbiterRunArbiter:
    """Tests for ArbiterAnalyzer._run_arbiter."""

    @pytest.mark.asyncio
    async def test_run_arbiter_success(self):
        """Should execute subprocess and return stdout."""
        analyzer = ArbiterAnalyzer(timeout=5.0)

        mock_process = AsyncMock()
        mock_process.communicate = AsyncMock(return_value=(b"arbiter output", b""))

        with (
            patch("tui.core.runner.asyncio.create_subprocess_exec", return_value=mock_process),
            patch("tui.core.runner.Path.exists", return_value=True),
        ):
            result = await analyzer._run_arbiter("test prompt")

        assert result == "arbiter output"


class TestArbiterParseResponse:
    """Tests for ArbiterAnalyzer._parse_response."""

    def setup_method(self):
        self.analyzer = ArbiterAnalyzer()

    def test_full_valid_response(self):
        """Full response with all fields should parse correctly."""
        response = (
            "SUMMARY: API rate limit hit\n"
            "ROOT_CAUSE: Too many requests per minute\n"
            "SEVERITY: critical\n"
            "CONFIDENCE: 0.92\n"
            "CAUSED_BY: orchestrator\n"
            "AUTO_RECOVER: YES\n"
            "RECOVERY:\n"
            "- Wait 60s before retry|0.95|LIVE\n"
            "- Reduce concurrency|0.80|SANDBOX\n"
            "- Check API quota|0.70|LIVE\n"
            "END\n"
        )
        result = self.analyzer._parse_response(response)

        assert result.summary == "API rate limit hit"
        assert result.root_cause == "Too many requests per minute"
        assert result.severity == "critical"
        assert result.confidence == pytest.approx(0.92)
        assert result.caused_by_agent == "orchestrator"
        assert result.can_auto_recover is True
        assert len(result.recovery_steps) == 3
        assert result.raw_output == response

    def test_partial_response(self):
        """Response with only SUMMARY and ROOT_CAUSE should use defaults for the rest."""
        response = "SUMMARY: Something broke\nROOT_CAUSE: Unknown issue\n"
        result = self.analyzer._parse_response(response)

        assert result.summary == "Something broke"
        assert result.root_cause == "Unknown issue"
        assert result.severity == "warning"  # default
        assert result.confidence == pytest.approx(0.5)  # default
        assert result.caused_by_agent is None
        assert result.can_auto_recover is False

    def test_empty_summary_raises(self):
        """Response with no SUMMARY should raise ValueError."""
        response = "ROOT_CAUSE: Some cause\nSEVERITY: info\n"
        with pytest.raises(ValueError, match="Failed to parse"):
            self.analyzer._parse_response(response)

    def test_caused_by_none_literal(self):
        """CAUSED_BY: NONE should result in None."""
        response = "SUMMARY: Error\nCAUSED_BY: NONE\n"
        result = self.analyzer._parse_response(response)
        assert result.caused_by_agent is None

    def test_invalid_confidence_ignored(self):
        """Non-float confidence should keep default."""
        response = "SUMMARY: Error\nCONFIDENCE: not-a-number\n"
        result = self.analyzer._parse_response(response)
        assert result.confidence == pytest.approx(0.5)

    def test_invalid_severity_ignored(self):
        """Invalid severity should keep default 'warning'."""
        response = "SUMMARY: Error\nSEVERITY: panic\n"
        result = self.analyzer._parse_response(response)
        assert result.severity == "warning"

    def test_auto_recover_no(self):
        """AUTO_RECOVER: NO should result in False."""
        response = "SUMMARY: Error\nAUTO_RECOVER: NO\n"
        result = self.analyzer._parse_response(response)
        assert result.can_auto_recover is False


class TestArbiterParseRecoveryStep:
    """Tests for ArbiterAnalyzer._parse_recovery_step."""

    def setup_method(self):
        self.analyzer = ArbiterAnalyzer()

    def test_full_format(self):
        """Full step with action|probability|mode should parse correctly."""
        step = self.analyzer._parse_recovery_step("Restart agent|0.85|SANDBOX")
        assert step.action == "Restart agent"
        assert step.success_probability == pytest.approx(0.85)
        assert step.requires_sandbox is True

    def test_full_format_live(self):
        """LIVE mode should result in requires_sandbox=False."""
        step = self.analyzer._parse_recovery_step("Check logs|0.90|LIVE")
        assert step.action == "Check logs"
        assert step.success_probability == pytest.approx(0.90)
        assert step.requires_sandbox is False

    def test_partial_format_no_mode(self):
        """Step with action|probability but no mode."""
        step = self.analyzer._parse_recovery_step("Wait 60s|0.95")
        assert step.action == "Wait 60s"
        assert step.success_probability == pytest.approx(0.95)
        assert step.requires_sandbox is False

    def test_action_only(self):
        """Step with only action (no pipes)."""
        step = self.analyzer._parse_recovery_step("Restart the process")
        assert step.action == "Restart the process"
        assert step.success_probability == pytest.approx(0.7)  # default
        assert step.requires_sandbox is False

    def test_bad_probability_uses_default(self):
        """Non-float probability should fall back to 0.7 default."""
        step = self.analyzer._parse_recovery_step("Do something|bad|LIVE")
        assert step.action == "Do something"
        assert step.success_probability == pytest.approx(0.7)


class TestArbiterFallbackAnalysis:
    """Tests for ArbiterAnalyzer._fallback_analysis all branches."""

    def setup_method(self):
        self.analyzer = ArbiterAnalyzer()

    def test_rate_limit(self):
        """'rate limit' in error should trigger rate limit analysis."""
        result = self.analyzer._fallback_analysis("a", "T1", "rate limit exceeded")
        assert "retry" in result.summary.lower() or "re-trying" in result.summary.lower()
        assert result.severity == "warning"
        assert result.can_auto_recover is True
        assert result.confidence == pytest.approx(0.9)

    def test_429_error(self):
        """'429' in error should trigger rate limit analysis."""
        result = self.analyzer._fallback_analysis("a", "T1", "HTTP 429 error")
        assert "retry" in result.summary.lower() or "re-trying" in result.summary.lower()

    def test_timeout(self):
        """'timeout' in error should trigger timeout analysis."""
        result = self.analyzer._fallback_analysis("a", "T1", "Request timeout")
        assert "time" in result.summary.lower()
        assert result.severity == "warning"
        assert result.can_auto_recover is False

    def test_timed_out(self):
        """'timed out' in error should trigger timeout analysis."""
        result = self.analyzer._fallback_analysis("a", "T1", "Connection timed out")
        assert "time" in result.summary.lower()

    def test_permission(self):
        """'permission' in error should trigger permission analysis."""
        result = self.analyzer._fallback_analysis("a", "T1", "Permission denied")
        assert "security" in result.summary.lower() or "blocked" in result.summary.lower()
        assert result.severity == "critical"
        assert result.can_auto_recover is False

    def test_403_error(self):
        """'403' in error should trigger permission analysis."""
        result = self.analyzer._fallback_analysis("a", "T1", "HTTP 403 Forbidden")
        assert result.severity == "critical"

    def test_not_found(self):
        """'not found' in error should trigger not-found analysis."""
        result = self.analyzer._fallback_analysis("a", "T1", "File not found")
        assert "missing" in result.summary.lower() or "resource" in result.summary.lower()
        assert result.severity == "warning"

    def test_404_error(self):
        """'404' in error should trigger not-found analysis."""
        result = self.analyzer._fallback_analysis("a", "T1", "HTTP 404")
        assert "missing" in result.summary.lower() or "resource" in result.summary.lower()

    def test_connection(self):
        """'connection' in error should trigger network analysis."""
        result = self.analyzer._fallback_analysis("a", "T1", "Connection refused")
        assert "network" in result.summary.lower() or "connectivity" in result.summary.lower()
        assert result.severity == "critical"
        assert result.can_auto_recover is True

    def test_network(self):
        """'network' in error should trigger network analysis."""
        result = self.analyzer._fallback_analysis("a", "T1", "Network unreachable")
        assert result.severity == "critical"

    def test_memory(self):
        """'memory' in error should trigger memory analysis."""
        result = self.analyzer._fallback_analysis("a", "T1", "Out of memory")
        assert "memory" in result.summary.lower()
        assert result.severity == "critical"
        assert result.can_auto_recover is False

    def test_oom(self):
        """'oom' in error should trigger memory analysis."""
        result = self.analyzer._fallback_analysis("a", "T1", "OOM killed")
        assert "memory" in result.summary.lower()

    def test_token(self):
        """'token' in error should trigger token/context analysis."""
        result = self.analyzer._fallback_analysis("a", "T1", "Token limit exceeded")
        assert "large" in result.summary.lower() or "split" in result.summary.lower()
        assert result.severity == "warning"

    def test_context(self):
        """'context' in error should trigger token/context analysis."""
        result = self.analyzer._fallback_analysis("a", "T1", "Context window exceeded")
        assert result.severity == "warning"

    def test_generic_fallback(self):
        """Unknown error should trigger generic fallback."""
        result = self.analyzer._fallback_analysis("a", "T1", "Something completely unexpected")
        assert "investigating" in result.summary.lower() or "check" in result.summary.lower()
        assert result.severity == "info"
        assert result.confidence == pytest.approx(0.3)
        assert result.can_auto_recover is False

    def test_cascading_failure_detected(self):
        """Error with 'caused by' pattern should set caused_by_agent."""
        result = self.analyzer._fallback_analysis(
            "reviewer", "T1", "rate limit caused by orchestrator flooding",
        )
        assert result.caused_by_agent == "orchestrator"


class TestArbiterDetectCascadingFailure:
    """Tests for ArbiterAnalyzer._detect_cascading_failure."""

    def setup_method(self):
        self.analyzer = ArbiterAnalyzer()

    def test_caused_by_pattern(self):
        """'caused by <agent>' should detect the agent."""
        result = self.analyzer._detect_cascading_failure("Error caused by orchestrator")
        assert result == "orchestrator"

    def test_triggered_by_pattern(self):
        """'triggered by <agent>' should detect the agent."""
        result = self.analyzer._detect_cascading_failure("Failure triggered by reviewer")
        assert result == "reviewer"

    def test_waiting_on_pattern(self):
        """'waiting on <agent>' should detect the agent."""
        result = self.analyzer._detect_cascading_failure("Timeout waiting on debugger")
        assert result == "debugger"

    def test_blocked_by_pattern(self):
        """'blocked by <agent>' should detect the agent."""
        result = self.analyzer._detect_cascading_failure("Task blocked by architect")
        assert result == "architect"

    def test_depends_on_pattern(self):
        """'depends on <agent>' should detect the agent."""
        result = self.analyzer._detect_cascading_failure("This task depends on tester")
        assert result == "tester"

    def test_upstream_failed_pattern(self):
        """'upstream <agent> failed' should detect the agent."""
        result = self.analyzer._detect_cascading_failure("upstream builder failed with error")
        assert result == "builder"

    def test_no_match(self):
        """Message without cascading pattern should return None."""
        result = self.analyzer._detect_cascading_failure("Generic error message")
        assert result is None


class TestRecoveryStepDataclass:
    """Tests for RecoveryStep dataclass."""

    def test_defaults(self):
        """Default values should be set properly."""
        step = RecoveryStep(action="Do something")
        assert step.success_probability == pytest.approx(0.7)
        assert step.requires_sandbox is False

    def test_custom_values(self):
        """Custom values should be stored."""
        step = RecoveryStep(action="Restart", success_probability=0.95, requires_sandbox=True)
        assert step.action == "Restart"
        assert step.success_probability == pytest.approx(0.95)
        assert step.requires_sandbox is True


class TestErrorAnalysisProperties:
    """Tests for ErrorAnalysis properties not covered elsewhere."""

    def test_confidence_tier_high(self):
        """Confidence >= 0.85 should return 'high'."""
        ea = ErrorAnalysis(
            summary="x", root_cause="y", recovery_steps=[],
            severity="info", confidence=0.90,
        )
        assert ea.confidence_tier == "high"

    def test_confidence_tier_medium(self):
        """Confidence >= 0.60 and < 0.85 should return 'medium'."""
        ea = ErrorAnalysis(
            summary="x", root_cause="y", recovery_steps=[],
            severity="info", confidence=0.70,
        )
        assert ea.confidence_tier == "medium"

    def test_confidence_tier_low(self):
        """Confidence < 0.60 should return 'low'."""
        ea = ErrorAnalysis(
            summary="x", root_cause="y", recovery_steps=[],
            severity="info", confidence=0.30,
        )
        assert ea.confidence_tier == "low"

    def test_top_recovery_empty(self):
        """No recovery steps should return None."""
        ea = ErrorAnalysis(
            summary="x", root_cause="y", recovery_steps=[],
            severity="info", confidence=0.5,
        )
        assert ea.top_recovery is None

    def test_top_recovery_highest(self):
        """Should return the step with highest success_probability."""
        steps = [
            RecoveryStep("A", 0.5),
            RecoveryStep("B", 0.95),
            RecoveryStep("C", 0.7),
        ]
        ea = ErrorAnalysis(
            summary="x", root_cause="y", recovery_steps=steps,
            severity="info", confidence=0.5,
        )
        assert ea.top_recovery.action == "B"


class TestRunAgentExtended:
    """Extended tests for run_agent async generator."""

    @pytest.mark.asyncio
    async def test_run_agent_euxis_bin_fallback(self):
        """When ~/bin/euxis doesn't exist, should use EUXIS_HOME/bin/euxis."""
        mock_stdout = AsyncMock()
        mock_stdout.readline = AsyncMock(side_effect=[b"line\n", b""])
        mock_process = AsyncMock()
        mock_process.stdout = mock_stdout
        mock_process.wait = AsyncMock()

        with (
            patch("tui.core.runner.asyncio.create_subprocess_exec", return_value=mock_process),
            patch("tui.core.runner.Path.exists", return_value=False),
        ):
            lines = []
            async for line in run_agent("architect", "task", provider="claude"):
                lines.append(line)

        assert lines == ["line"]


class TestRunSquadExtended:
    """Extended tests for run_squad async generator."""

    @pytest.mark.asyncio
    async def test_run_squad_no_stdout(self):
        """Should handle None stdout gracefully."""
        mock_process = AsyncMock()
        mock_process.stdout = None

        with (
            patch("tui.core.runner.asyncio.create_subprocess_exec", return_value=mock_process),
            patch("tui.core.runner.Path.exists", return_value=True),
        ):
            lines = []
            async for line in run_squad("alpha-squad", "deploy"):
                lines.append(line)

        assert lines == []


class TestRunComboExtended:
    """Extended tests for run_combo async generator."""

    @pytest.mark.asyncio
    async def test_run_combo_euxis_bin_fallback(self):
        """When ~/bin/euxis-combo doesn't exist, should fall back."""
        mock_stdout = AsyncMock()
        mock_stdout.readline = AsyncMock(side_effect=[b"step1\n", b""])
        mock_process = AsyncMock()
        mock_process.stdout = mock_stdout
        mock_process.wait = AsyncMock()

        with (
            patch("tui.core.runner.asyncio.create_subprocess_exec", return_value=mock_process),
            patch("tui.core.runner.Path.exists", return_value=False),
        ):
            lines = []
            async for line in run_combo("review-chain", "audit"):
                lines.append(line)

        assert lines == ["step1"]
