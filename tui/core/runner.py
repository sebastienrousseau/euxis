# (c) 2026 Euxis Fleet. All rights reserved.
"""Agent execution via subprocess with streaming output."""

from __future__ import annotations

import asyncio
import os
import re
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import TYPE_CHECKING

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
        squad_bin = EUXIS_HOME / "bin" / "euxis-squad"

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
