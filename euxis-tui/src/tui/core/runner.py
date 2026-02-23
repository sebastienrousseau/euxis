# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Euxis Contributors

"""Agent execution via subprocess with streaming output."""

from __future__ import annotations

import asyncio
import inspect
import os
import re
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import TYPE_CHECKING, Any

from tui.core import EUXIS_HOME

if TYPE_CHECKING:
    from collections.abc import AsyncIterator

# Provider display names
PROVIDERS = {
    "claude": "Claude",
    "gemini": "Gemini",
    "ollama": "Ollama",
    "codex": "Codex",
    "qwen": "Qwen",
    "crush": "Crush",
    "kiro-cli": "Kiro",
    "goose": "Goose",
}

# Full model identifiers per provider (sourced from providers.sh)
PROVIDER_MODELS = {
    "claude": "claude-sonnet-4-20250514",
    "openai": "gpt-4o",
    "gemini": "gemini-2.0-flash",
    "ollama": "llama3.2",
    "qwen": "qwen3-coder",
    "goose": "claude-sonnet-4",
    "crush": "claude-sonnet-4",
    "kiro-cli": "kiro-cli",
    "codex": "codex-mini-latest",
}

# Pattern for valid identifiers (agent IDs, squad IDs, combo IDs)
_SAFE_ID = re.compile(r"^[a-zA-Z][a-zA-Z0-9_-]{0,63}$")

DEFAULT_PROVIDER = "claude"

_SECONDS_PER_MINUTE = 60


@dataclass
class AgentRun:
    """State for a running agent execution."""

    agent_id: str
    task: str
    provider: str
    started_at: float = field(default_factory=time.time)
    output_lines: list[str] = field(default_factory=list)
    return_code: int | None = None
    output_path: str | None = None

    @property
    def is_running(self) -> bool:
        """Return whether the agent is still executing."""
        return self.return_code is None

    @property
    def elapsed(self) -> float:
        """Return seconds since execution started."""
        return time.time() - self.started_at

    @property
    def elapsed_display(self) -> str:
        """Return human-readable elapsed time string."""
        s = int(self.elapsed)
        if s < _SECONDS_PER_MINUTE:
            return f"{s}s"
        return f"{s // _SECONDS_PER_MINUTE}m {s % _SECONDS_PER_MINUTE}s"


@dataclass
class ExecutionMetrics:
    """Execution metrics for an agent run."""

    start_time: float
    end_time: float
    peak_memory_kb: int = 0
    cpu_time_seconds: float = 0.0
    exit_code: int | None = None

    @property
    def duration_seconds(self) -> float:
        """Return the elapsed wall time in seconds."""
        return max(0.0, self.end_time - self.start_time)


@dataclass
class AgentExecutionResult:
    """Structured execution result for an agent run."""

    success: bool
    output: str
    error: str
    exit_code: int | None
    timeout_occurred: bool = False
    metrics: ExecutionMetrics | None = None


class AgentRunner:
    """Execute agents via subprocess, capturing output and metrics."""

    def __init__(self, agent: Any, provider: str, timeout: float = 300.0) -> None:
        self.agent = agent
        self.provider = provider
        self.timeout = timeout

    async def execute(self, task: str, working_dir: str | None = None) -> AgentExecutionResult:
        """Run the agent and return a structured result."""
        _validate_id(self.agent.id, "agent_id")

        euxis_bin = Path.home() / "bin" / "euxis"
        if not euxis_bin.exists():
            euxis_bin = EUXIS_HOME / "bin" / "euxis"

        cmd = [str(euxis_bin), self.agent.id, task, self.provider]

        env = os.environ.copy()
        env["EUXIS_HOME"] = str(EUXIS_HOME)

        start_time = time.time()
        try:
            process = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
                cwd=working_dir or str(Path.cwd()),
                env=env,
            )
            try:
                stdout, stderr = await asyncio.wait_for(process.communicate(), timeout=self.timeout)
                exit_code = process.returncode
                end_time = time.time()
                output = (stdout or b"").decode("utf-8", errors="replace")
                error = (stderr or b"").decode("utf-8", errors="replace")
                metrics = ExecutionMetrics(
                    start_time=start_time,
                    end_time=end_time,
                    exit_code=exit_code,
                )
                return AgentExecutionResult(
                    success=exit_code == 0,
                    output=output,
                    error=error,
                    exit_code=exit_code,
                    timeout_occurred=False,
                    metrics=metrics,
                )
            except asyncio.TimeoutError:
                kill_result = process.kill()
                if inspect.isawaitable(kill_result):
                    await kill_result  # pragma: no cover
                await process.wait()
                end_time = time.time()
                metrics = ExecutionMetrics(start_time=start_time, end_time=end_time, exit_code=None)
                return AgentExecutionResult(
                    success=False,
                    output="",
                    error="Execution timed out.",
                    exit_code=None,
                    timeout_occurred=True,
                    metrics=metrics,
                )
        except (FileNotFoundError, PermissionError, OSError) as exc:
            end_time = time.time()
            metrics = ExecutionMetrics(start_time=start_time, end_time=end_time, exit_code=None)
            return AgentExecutionResult(
                success=False,
                output="",
                error=str(exc),
                exit_code=None,
                timeout_occurred=False,
                metrics=metrics,
            )


def validate_agent_id(value: str) -> bool:  # pragma: no cover
    """Return True if the value is a valid agent ID."""
    return bool(_SAFE_ID.match(value))


def _validate_id(value: str, label: str) -> None:
    """Validate an identifier against the safe pattern."""
    if not _SAFE_ID.match(value):
        msg = f"Invalid {label}: {value!r}"
        raise ValueError(msg)


def _validate_provider(provider: str) -> None:
    """Validate provider against known list."""
    if provider not in PROVIDERS:
        msg = f"Unknown provider: {provider!r}"
        raise ValueError(msg)


async def run_agent(
    agent_id: str,
    task: str,
    provider: str = DEFAULT_PROVIDER,
    working_dir: str | None = None,
) -> AsyncIterator[str]:
    """Run an agent and yield output lines as they arrive."""
    _validate_id(agent_id, "agent_id")
    _validate_provider(provider)

    euxis_bin = Path.home() / "bin" / "euxis"
    if not euxis_bin.exists():
        euxis_bin = EUXIS_HOME / "bin" / "euxis"

    cmd = [str(euxis_bin), agent_id, task, provider]

    env = os.environ.copy()
    env["EUXIS_HOME"] = str(EUXIS_HOME)

    process = await asyncio.create_subprocess_exec(
        *cmd,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.STDOUT,
        cwd=working_dir or str(Path.cwd()),
        env=env,
    )

    if process.stdout is None:
        return
    while True:
        line = await process.stdout.readline()
        if not line:
            break
        decoded = line.decode("utf-8", errors="replace").rstrip("\n")
        yield decoded

    await process.wait()


async def run_squad(
    squad_id: str,
    task: str,
    working_dir: str | None = None,
) -> AsyncIterator[str]:
    """Run a squad deployment and yield output lines."""
    _validate_id(squad_id, "squad_id")

    squad_bin = Path.home() / "bin" / "euxis-squad"
    if not squad_bin.exists():
        squad_bin = EUXIS_HOME / "bin" / "euxis-squad"  # pragma: no cover

    cmd = [str(squad_bin), "deploy", squad_id, task]

    env = os.environ.copy()
    env["EUXIS_HOME"] = str(EUXIS_HOME)

    process = await asyncio.create_subprocess_exec(
        *cmd,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.STDOUT,
        cwd=working_dir or str(Path.cwd()),
        env=env,
    )

    if process.stdout is None:
        return
    while True:
        line = await process.stdout.readline()
        if not line:
            break
        decoded = line.decode("utf-8", errors="replace").rstrip("\n")
        yield decoded

    await process.wait()


async def run_combo(
    combo_id: str,
    task: str,
    working_dir: str | None = None,
) -> AsyncIterator[str]:
    """Run a combo chain and yield output lines."""
    _validate_id(combo_id, "combo_id")

    combo_bin = Path.home() / "bin" / "euxis-combo"
    if not combo_bin.exists():
        combo_bin = EUXIS_HOME / "bin" / "euxis-combo"

    cmd = [str(combo_bin), "run", combo_id, task]

    env = os.environ.copy()
    env["EUXIS_HOME"] = str(EUXIS_HOME)

    process = await asyncio.create_subprocess_exec(
        *cmd,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.STDOUT,
        cwd=working_dir or str(Path.cwd()),
        env=env,
    )

    if process.stdout is None:
        return
    while True:
        line = await process.stdout.readline()
        if not line:
            break
        decoded = line.decode("utf-8", errors="replace").rstrip("\n")
        yield decoded

    await process.wait()


def get_project_name(working_dir: str | None = None) -> str:
    """Get the current project name from directory."""
    cwd = Path(working_dir or str(Path.cwd()))
    return cwd.name


def get_project_path(working_dir: str | None = None) -> str:
    """Get the current project path, abbreviated with ~ for home directory."""
    cwd = Path(working_dir or str(Path.cwd()))
    try:
        return "~/" + str(cwd.relative_to(Path.home()))
    except ValueError:
        return str(cwd)


def get_git_branch(working_dir: str | None = None) -> str | None:
    """Get the current git branch."""
    cwd = Path(working_dir or str(Path.cwd()))
    head = cwd / ".git" / "HEAD"
    if head.exists():
        content = head.read_text().strip()
        if content.startswith("ref: refs/heads/"):
            return content[16:]
    return None


def list_agent_outputs(
    agent_id: str, project: str | None = None
) -> list[Path]:
    """List output files for an agent."""
    proj = project or get_project_name()
    output_dir = EUXIS_HOME / "data" / "projects" / proj / agent_id / "output"
    if not output_dir.exists():
        return []
    return sorted(output_dir.glob("*.md"), reverse=True)


# =============================================================================
# ARBITER ERROR ANALYSIS — LLM-augmented error diagnosis (2026 UX)
# =============================================================================

@dataclass
class RecoveryStep:
    """A recovery action with success probability."""

    action: str
    success_probability: float = 0.7  # 0.0 to 1.0
    requires_sandbox: bool = False  # True if should test in dry-run first


@dataclass
class ErrorAnalysis:
    """Structured error analysis from Arbiter agent.

    2026 UX: Transparency-as-a-Feature
    - Confidence scoring for trust calibration
    - Cascading failure attribution
    - Recovery steps ranked by success probability
    """

    summary: str
    root_cause: str
    recovery_steps: list[RecoveryStep]
    severity: str  # "critical", "warning", "info"
    confidence: float  # 0.0 to 1.0
    caused_by_agent: str | None = None  # Attribution for cascading failures
    can_auto_recover: bool = False  # Safe for automatic retry
    raw_output: str = ""

    @property
    def confidence_tier(self) -> str:
        """Return confidence tier for color coding."""
        if self.confidence >= 0.85:
            return "high"  # Green
        elif self.confidence >= 0.60:
            return "medium"  # Yellow
        return "low"  # Red

    @property
    def top_recovery(self) -> RecoveryStep | None:
        """Return highest-probability recovery step."""
        if not self.recovery_steps:
            return None
        return max(self.recovery_steps, key=lambda s: s.success_probability)


# Simple in-memory cache for analysis results (keyed by error message hash)
_analysis_cache: dict[str, ErrorAnalysis] = {}
_CACHE_MAX_SIZE = 50


class ArbiterAnalyzer:
    """Analyze errors using the Arbiter agent for LLM-augmented diagnostics.

    2026 UX Pattern: LLM-Augmented Error Analysis
    - Quick timeout (15s) for responsive UI
    - Structured output parsing for actionable insights
    - Graceful fallback to rule-based classification
    - In-memory caching to avoid repeated LLM calls
    """

    ANALYSIS_PROMPT_TEMPLATE = """Analyze this agent error and provide actionable diagnostics.

AGENT: {agent_id}
TIER: {tier}
LAST TASK: {last_task}
ERROR MESSAGE: {error_message}
TIMESTAMP: {timestamp}

Respond in this exact format (no markdown, plain text only):
SUMMARY: <outcome-focused one-liner, e.g. "Re-trying in 60s to resolve API congestion">
ROOT_CAUSE: <technical root cause explanation>
SEVERITY: <critical|warning|info>
CONFIDENCE: <0.0-1.0 confidence score>
CAUSED_BY: <agent_id if cascading failure, or NONE>
AUTO_RECOVER: <YES if safe for automatic retry, NO otherwise>
RECOVERY:
- <step 1>|<success_probability 0.0-1.0>|<SANDBOX if needs dry-run, else LIVE>
- <step 2>|<success_probability>|<SANDBOX or LIVE>
- <step 3>|<success_probability>|<SANDBOX or LIVE>
END"""

    def __init__(self, timeout: float = 15.0) -> None:
        self.timeout = timeout

    async def analyze(
        self,
        agent_id: str,
        tier: str,
        error_message: str,
        last_task: str = "Unknown",
        timestamp: str = "",
    ) -> ErrorAnalysis:
        """Analyze an error using the Arbiter agent.

        Returns structured analysis or falls back to rule-based classification.
        """
        # Check cache first
        cache_key = f"{agent_id}:{hash(error_message)}"
        if cache_key in _analysis_cache:
            return _analysis_cache[cache_key]

        # Build analysis prompt
        prompt = self.ANALYSIS_PROMPT_TEMPLATE.format(
            agent_id=agent_id,
            tier=tier,
            error_message=error_message[:500],  # Truncate for efficiency
            last_task=last_task[:200],
            timestamp=timestamp,
        )

        try:
            # Run Arbiter with short timeout
            result = await self._run_arbiter(prompt)
            analysis = self._parse_response(result)
        except (asyncio.TimeoutError, ValueError, OSError):
            # Fallback to rule-based classification
            analysis = self._fallback_analysis(agent_id, tier, error_message)

        # Cache result (with size limit)
        if len(_analysis_cache) >= _CACHE_MAX_SIZE:
            # Remove oldest entry (first key)
            oldest = next(iter(_analysis_cache))
            del _analysis_cache[oldest]
        _analysis_cache[cache_key] = analysis

        return analysis

    async def explain_budget(
        self,
        agent_id: str,
        tier: str,
        cost: float,
        budget: float,
        request_count: int,
        recent_tasks: list[str] | None = None,
    ) -> str:
        """Use Arbiter to explain why an agent is nearing budget limit.

        2026 UX: Explainability on Demand
        Returns a human-readable explanation of budget consumption.
        """
        tasks_summary = ""
        if recent_tasks:
            tasks_summary = "\nRecent tasks:\n" + "\n".join(f"- {t}" for t in recent_tasks[:5])

        prompt = f"""Explain why this agent is nearing its token budget limit.

AGENT: {agent_id}
TIER: {tier}
CURRENT COST: ${cost:.2f}
BUDGET LIMIT: ${budget:.2f}
UTILIZATION: {(cost/budget*100):.1f}%
REQUEST COUNT: {request_count}
{tasks_summary}

Provide a brief, actionable explanation (2-3 sentences) focusing on:
1. Why costs are high (task complexity, repetition, etc.)
2. Specific recommendation to reduce costs

Format: Just provide the explanation text, no labels or formatting."""

        try:
            result = await self._run_arbiter(prompt)
            # Clean up response
            explanation = result.strip()
            if not explanation:
                raise ValueError("Empty response")
            return explanation
        except (asyncio.TimeoutError, ValueError, OSError):
            # Fallback explanation
            per_request_cost = cost / request_count if request_count > 0 else 0
            if per_request_cost > 0.05:
                return (
                    f"{agent_id} is using expensive operations at "
                    f"${per_request_cost:.3f}/request. Consider using a lighter model "
                    f"or splitting complex tasks."
                )
            elif request_count > 50:
                return (
                    f"{agent_id} has made {request_count} requests. "
                    f"Check for repetitive operations or stuck loops."
                )
            else:
                return (
                    f"{agent_id} at {(cost/budget*100):.0f}% budget. "
                    f"Review recent tasks for optimization opportunities."
                )

    async def _run_arbiter(self, prompt: str) -> str:
        """Execute Arbiter agent and return output."""
        euxis_bin = Path.home() / "bin" / "euxis"
        if not euxis_bin.exists():
            euxis_bin = EUXIS_HOME / "bin" / "euxis"  # pragma: no cover

        # Use euxis-loop for single-shot execution with arbiter
        cmd = [str(euxis_bin), "arbiter", prompt, "claude"]

        env = os.environ.copy()
        env["EUXIS_HOME"] = str(EUXIS_HOME)
        env["EUXIS_MODEL_OVERRIDE"] = "claude-haiku-3-5"  # Fast model for quick analysis

        process = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            env=env,
        )

        stdout, _ = await asyncio.wait_for(
            process.communicate(), timeout=self.timeout
        )
        return (stdout or b"").decode("utf-8", errors="replace")

    def _parse_response(self, response: str) -> ErrorAnalysis:
        """Parse structured response from Arbiter."""
        lines = response.strip().split("\n")
        result: dict[str, Any] = {
            "summary": "",
            "root_cause": "",
            "severity": "warning",
            "confidence": 0.5,
            "recovery_steps": [],
            "caused_by": None,
            "auto_recover": False,
        }

        in_recovery = False
        for line in lines:
            line = line.strip()
            if line.startswith("SUMMARY:"):
                result["summary"] = line[8:].strip()
            elif line.startswith("ROOT_CAUSE:"):
                result["root_cause"] = line[11:].strip()
            elif line.startswith("SEVERITY:"):
                sev = line[9:].strip().lower()
                if sev in ("critical", "warning", "info"):
                    result["severity"] = sev
            elif line.startswith("CONFIDENCE:"):
                try:
                    result["confidence"] = float(line[11:].strip())
                except ValueError:
                    pass
            elif line.startswith("CAUSED_BY:"):
                val = line[10:].strip()
                if val.upper() != "NONE":
                    result["caused_by"] = val
            elif line.startswith("AUTO_RECOVER:"):
                result["auto_recover"] = line[13:].strip().upper() == "YES"
            elif line.startswith("RECOVERY:"):
                in_recovery = True
            elif line == "END":  # pragma: no cover
                in_recovery = False
            elif in_recovery and line.startswith("- "):  # pragma: no cover
                step = self._parse_recovery_step(line[2:])
                result["recovery_steps"].append(step)

        if not result["summary"]:
            msg = "Failed to parse Arbiter response"
            raise ValueError(msg)

        return ErrorAnalysis(
            summary=result["summary"],
            root_cause=result["root_cause"],
            recovery_steps=result["recovery_steps"],
            severity=result["severity"],
            confidence=result["confidence"],
            caused_by_agent=result["caused_by"],
            can_auto_recover=result["auto_recover"],
            raw_output=response,
        )

    def _parse_recovery_step(self, line: str) -> RecoveryStep:
        """Parse a recovery step line: action|probability|mode."""
        parts = line.split("|")
        action = parts[0].strip()
        probability = 0.7
        requires_sandbox = False

        if len(parts) >= 2:
            try:
                probability = float(parts[1].strip())
            except ValueError:
                pass
        if len(parts) >= 3:
            requires_sandbox = parts[2].strip().upper() == "SANDBOX"

        return RecoveryStep(
            action=action,
            success_probability=probability,
            requires_sandbox=requires_sandbox,
        )

    def _fallback_analysis(
        self, agent_id: str, tier: str, message: str
    ) -> ErrorAnalysis:
        """Rule-based fallback when Arbiter is unavailable."""
        message_lower = message.lower()

        # Check for cascading failure attribution
        caused_by = self._detect_cascading_failure(message)

        # Classification rules with RecoveryStep objects
        if "rate limit" in message_lower or "429" in message_lower:
            return ErrorAnalysis(
                summary="Re-trying in 60s to resolve API congestion",
                root_cause="Too many requests sent to the API provider in a short time window.",
                recovery_steps=[
                    RecoveryStep("Wait 60 seconds before retrying", 0.95, False),
                    RecoveryStep("Reduce concurrent agent count", 0.85, False),
                    RecoveryStep("Check API quota in provider dashboard", 0.70, False),
                ],
                severity="warning",
                confidence=0.9,
                caused_by_agent=caused_by,
                can_auto_recover=True,  # Safe to auto-retry after delay
            )
        elif "timeout" in message_lower or "timed out" in message_lower:
            return ErrorAnalysis(
                summary="Task exceeded time limit—consider splitting workload",
                root_cause="Task exceeded maximum allowed execution time.",
                recovery_steps=[
                    RecoveryStep("Break task into smaller subtasks", 0.80, False),
                    RecoveryStep("Check network connectivity", 0.75, False),
                    RecoveryStep("Increase timeout in config", 0.60, True),  # Sandbox
                ],
                severity="warning",
                confidence=0.85,
                caused_by_agent=caused_by,
                can_auto_recover=False,
            )
        elif "permission" in message_lower or "403" in message_lower:
            return ErrorAnalysis(
                summary="Security policy blocked operation—review access rules",
                root_cause="Insufficient access rights or security policy violation.",
                recovery_steps=[
                    RecoveryStep("Verify API key has required permissions", 0.85, False),
                    RecoveryStep("Check security rules in manifest", 0.80, False),
                    RecoveryStep("Review recent policy changes", 0.70, False),
                ],
                severity="critical",
                confidence=0.9,
                caused_by_agent=caused_by,
                can_auto_recover=False,
            )
        elif "not found" in message_lower or "404" in message_lower:
            return ErrorAnalysis(
                summary="Requested resource missing—verify paths",
                root_cause="Requested file, endpoint, or resource does not exist.",
                recovery_steps=[
                    RecoveryStep("Verify file paths in task description", 0.85, False),
                    RecoveryStep("Check if resource was moved or deleted", 0.75, False),
                    RecoveryStep("Update manifest with correct paths", 0.70, True),
                ],
                severity="warning",
                confidence=0.85,
                caused_by_agent=caused_by,
                can_auto_recover=False,
            )
        elif "connection" in message_lower or "network" in message_lower:
            return ErrorAnalysis(
                summary="Network unavailable—check connectivity",
                root_cause="Failed to establish connection to required service.",
                recovery_steps=[
                    RecoveryStep("Check internet connectivity", 0.90, False),
                    RecoveryStep("Verify API endpoint is reachable", 0.80, False),
                    RecoveryStep("Check for firewall or proxy issues", 0.65, False),
                ],
                severity="critical",
                confidence=0.8,
                caused_by_agent=caused_by,
                can_auto_recover=True,  # Network may recover
            )
        elif "memory" in message_lower or "oom" in message_lower:
            return ErrorAnalysis(
                summary="Memory exhausted—reduce task scope",
                root_cause="Process exceeded available memory limits.",
                recovery_steps=[
                    RecoveryStep("Reduce context window size", 0.85, True),  # Sandbox
                    RecoveryStep("Process data in smaller batches", 0.80, True),
                    RecoveryStep("Close other applications to free memory", 0.70, False),
                ],
                severity="critical",
                confidence=0.9,
                caused_by_agent=caused_by,
                can_auto_recover=False,
            )
        elif "token" in message_lower or "context" in message_lower:
            return ErrorAnalysis(
                summary="Input too large for model—split the task",
                root_cause="Input or conversation exceeded model's token limit.",
                recovery_steps=[
                    RecoveryStep("Split task into smaller chunks", 0.90, False),
                    RecoveryStep("Summarize previous context", 0.85, False),
                    RecoveryStep("Use a model with larger context window", 0.75, True),
                ],
                severity="warning",
                confidence=0.85,
                caused_by_agent=caused_by,
                can_auto_recover=False,
            )

        # Generic fallback
        return ErrorAnalysis(
            summary="Investigating error—check logs for details",
            root_cause="Unable to determine root cause from error message.",
            recovery_steps=[
                RecoveryStep("Check agent logs for detailed stack trace", 0.80, False),
                RecoveryStep("Restart the agent and monitor", 0.60, True),  # Sandbox
                RecoveryStep("Report issue if problem persists", 0.50, False),
            ],
            severity="info",
            confidence=0.3,
            caused_by_agent=caused_by,
            can_auto_recover=False,
        )

    def _detect_cascading_failure(self, message: str) -> str | None:
        """Detect if error was caused by another agent (cascading failure)."""
        # Pattern: "caused by <agent>", "triggered by <agent>", "waiting on <agent>"
        import re

        patterns = [
            r"caused by (\w+)",
            r"triggered by (\w+)",
            r"waiting on (\w+)",
            r"blocked by (\w+)",
            r"depends on (\w+)",
            r"upstream (\w+) failed",
        ]
        message_lower = message.lower()
        for pattern in patterns:
            match = re.search(pattern, message_lower)
            if match:
                return match.group(1)
        return None
