# (c) 2026 Euxis Fleet. All rights reserved.
"""Agent execution via subprocess with streaming output."""

from __future__ import annotations

import asyncio
import os
import re
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import AsyncIterator


EUXIS_HOME = Path.home() / ".euxis"

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

# Pattern for valid identifiers (agent IDs, squad IDs, combo IDs)
_SAFE_ID = re.compile(r"^[a-z][a-z0-9_-]{0,63}$")

DEFAULT_PROVIDER = "claude"


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
        return self.return_code is None

    @property
    def elapsed(self) -> float:
        return time.time() - self.started_at

    @property
    def elapsed_display(self) -> str:
        s = int(self.elapsed)
        if s < 60:
            return f"{s}s"
        return f"{s // 60}m {s % 60}s"


def _validate_id(value: str, label: str) -> None:
    """Validate an identifier against the safe pattern."""
    if not _SAFE_ID.match(value):
        raise ValueError(f"Invalid {label}: {value!r}")


def _validate_provider(provider: str) -> None:
    """Validate provider against known list."""
    if provider not in PROVIDERS:
        raise ValueError(f"Unknown provider: {provider!r}")


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
        cwd=working_dir or os.getcwd(),
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
        cwd=working_dir or os.getcwd(),
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
        cwd=working_dir or os.getcwd(),
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
    cwd = Path(working_dir or os.getcwd())
    return cwd.name


def get_git_branch(working_dir: str | None = None) -> str | None:
    """Get the current git branch."""
    cwd = Path(working_dir or os.getcwd())
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
