#!/usr/bin/env python3
"""Euxis Core Dispatch Engine.

Python core extracted from euxis-dispatch shell script.
Provides DispatchEngine class for fleet deployment coordination.
"""

from __future__ import annotations

import sys
from pathlib import Path

import argparse
import json
import logging
import os
import re
import sqlite3
import shutil
import subprocess
import tempfile
import time


def _ensure_repo_paths() -> None:
    repo_root = Path(__file__).resolve().parents[2]
    module_src_dirs = [
        "api/src",
        "adapters/src",
        "cli/src",
        "core/src",
        "crypto/src",
        "metrics/src",
        "ui/src",
        "packages/shared/src",
    ]
    if str(repo_root) not in sys.path:
        sys.path.insert(0, str(repo_root))
    for rel in module_src_dirs:
        path = repo_root / rel
        if path.exists():
            sys.path.insert(0, str(path))


_ensure_repo_paths()
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S"
)

logger = logging.getLogger(__name__)

EUXIS_HOME = Path(os.environ.get("EUXIS_HOME", Path.home() / ".euxis"))
DEFAULT_DISPATCH_MODES = {"hierarchical", "mesh", "federated"}
SHELL_METACHARACTERS = re.compile(r"[;&|`$(){}[\]<>*?~!#]")


class DispatchError(Exception):
    """Base exception for dispatch operations."""



class ValidationError(DispatchError):
    """Raised when manifest or configuration validation fails."""



class AgentError(DispatchError):
    """Raised when agent operations fail."""



@dataclass
class DispatchResult:
    """Result of a single agent dispatch."""

    agent: str
    index: int
    success: bool
    log_file: Path
    duration: float = 0.0
    return_code: int | None = None
    error_message: str | None = None

    @property
    def status(self) -> str:
        """Return human-readable status label."""
        return "SUCCESS" if self.success else "FAILURE"


@dataclass
class ManifestDispatch:
    """Single dispatch entry from manifest."""

    agent: str
    task: str
    priority: str
    verify_cmd: str | None = None
    stage: int = 1
    depends_on: list[str] = field(default_factory=list)
    locks: list[str] = field(default_factory=list)


@dataclass
class Manifest:
    """Parsed dispatch manifest."""

    project: str
    mode: str
    dispatches: list[ManifestDispatch]
    rollout_phases: list[dict[str, Any]] = field(default_factory=list)

    @classmethod
    def from_file(cls, manifest_path: Path) -> Manifest:
        """Load and parse manifest from JSON file."""
        try:
            with manifest_path.open() as f:
                data = json.load(f)
        except (OSError, json.JSONDecodeError) as exc:
            msg = f"Failed to load manifest {manifest_path}: {exc}"
            raise ValidationError(msg) from exc
        return cls.from_dict(data)

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> Manifest:
        """Load and parse manifest from a dictionary."""
        # Validate required fields
        if "dispatches" not in data:
            msg = "Manifest missing 'dispatches' field"
            raise ValidationError(msg)

        dispatches = []
        for i, dispatch_data in enumerate(data["dispatches"]):
            if not isinstance(dispatch_data, dict):
                msg = f"Dispatch #{i} is not a valid object"
                raise ValidationError(msg)

            # Required fields
            if "agent" not in dispatch_data:
                msg = f"Dispatch #{i} missing 'agent' field"
                raise ValidationError(msg)
            if "task" not in dispatch_data:
                msg = f"Dispatch #{i} missing 'task' field"
                raise ValidationError(msg)
            if "priority" not in dispatch_data:
                msg = f"Dispatch #{i} missing 'priority' field"
                raise ValidationError(msg)

            dispatches.append(
                ManifestDispatch(
                    agent=dispatch_data["agent"],
                    task=dispatch_data["task"],
                    priority=dispatch_data["priority"],
                    verify_cmd=dispatch_data.get("verify_cmd"),
                    stage=dispatch_data.get("stage", 1),
                    depends_on=dispatch_data.get("depends_on", []),
                    locks=dispatch_data.get("locks", []),
                )
            )

        return cls(
            project=data.get("project", "unknown"),
            mode=data.get("mode", "hierarchical"),
            dispatches=dispatches,
            rollout_phases=data.get("rollout_phases", []),
        )


class DispatchEngine:
    """Core dispatch engine for fleet coordination."""

    def __init__(self, euxis_home: Path | None = None) -> None:
        self.euxis_home = Path(euxis_home) if euxis_home else EUXIS_HOME
        self.repo_root = Path(__file__).resolve().parents[2]
        self.registry_path = self.euxis_home / "euxis-core/agents/registry.json"
        self.registry_db = self.euxis_home / "euxis-core/agents/registry.db"
        self.log_dir: Path | None = None
        self._valid_agents: set[str] | None = None

        logger.info("Initialized DispatchEngine with EUXIS_HOME: %s", self.euxis_home)

    def _load_agent_registry(self) -> set[str]:
        """Load valid agent IDs from registry (SQLite-first, JSON fallback)."""
        if self._valid_agents is not None:
            return self._valid_agents

        # Try SQLite first
        if self.registry_db.exists():
            try:
                conn = sqlite3.connect(str(self.registry_db))
                cursor = conn.execute("SELECT id FROM agents")
                self._valid_agents = {row[0] for row in cursor}
                conn.close()
                logger.info("Loaded %s agents from agents/registry.db", len(self._valid_agents))
                return self._valid_agents
            except sqlite3.Error as exc:
                logger.warning("SQLite registry query failed, falling back to JSON: %s", exc)

        # Fall back to JSON
        if not self.registry_path.exists():
            logger.warning("Registry not found at %s", self.registry_path)
            self._valid_agents = set()
            return self._valid_agents

        try:
            with self.registry_path.open() as f:
                registry_data = json.load(f)

            self._valid_agents = {
                agent["id"] for agent in registry_data.get("agents", [])
                if isinstance(agent, dict) and "id" in agent
            }
            logger.info("Loaded %s agents from agents/registry.json", len(self._valid_agents))

        except (OSError, json.JSONDecodeError):
            logger.exception("Failed to load agent registry")
            self._valid_agents = set()

        return self._valid_agents

    def validate_manifest(self, manifest: Manifest) -> None:
        """Validate manifest against registry and system constraints."""
        logger.info("Validating manifest: %s (%s)", manifest.project, manifest.mode)

        # Validate dispatch mode
        if manifest.mode not in DEFAULT_DISPATCH_MODES:
            msg = (
                f"Invalid dispatch mode '{manifest.mode}'. "
                f"Must be one of: {', '.join(sorted(DEFAULT_DISPATCH_MODES))}"
            )
            raise ValidationError(msg)

        # Load valid agents
        valid_agents = self._load_agent_registry()

        # Validate agents
        invalid_agents = []
        unsafe_agents = []
        for dispatch in manifest.dispatches:
            # Check if agent exists in registry
            if valid_agents and dispatch.agent not in valid_agents:
                invalid_agents.append(dispatch.agent)

            # Check for shell metacharacters in agent names
            if SHELL_METACHARACTERS.search(dispatch.agent):
                unsafe_agents.append(dispatch.agent)

        if invalid_agents:
            msg = (
                f"Manifest references unknown agents: {', '.join(invalid_agents)}. "
                f"Valid agents: {', '.join(sorted(valid_agents))}"
            )
            raise ValidationError(msg)

        if unsafe_agents:
            msg = (
                f"Agent names contain shell metacharacters: {', '.join(unsafe_agents)}. "
                "Agent names must be alphanumeric with hyphens/underscores only."
            )
            raise ValidationError(msg)

        # Validate verify commands
        missing_cmds = []
        for _index, dispatch in enumerate(manifest.dispatches):
            if dispatch.verify_cmd:
                # Extract first binary from command
                cmd_parts = dispatch.verify_cmd.strip().lstrip("! ").split()
                if cmd_parts:
                    binary = cmd_parts[0]
                    if not self._check_command_available(binary):
                        missing_cmds.append(f"[{dispatch.agent}] {binary}")

        if missing_cmds:
            msg = f"Manifest verify commands reference missing tools: {', '.join(missing_cmds)}"
            raise ValidationError(msg)

        logger.info("Manifest validation passed: %s dispatches", len(manifest.dispatches))

    def execute_manifest(self, manifest: Manifest, mode: str | None = None) -> list[DispatchResult]:
        """Validate and execute a manifest, returning dispatch results."""
        self.validate_manifest(manifest)
        if not self.log_dir:
            self.setup_log_directory()
        return self.execute_dispatches(manifest, mode=mode)

    def _check_command_available(self, command: str) -> bool:
        """Check if a command is available in PATH."""
        shell_builtins = {"cd", "true", "false", ":"}
        if command in shell_builtins:
            return True
        return shutil.which(command) is not None

    def _resolve_euxis_loop_path(self) -> Path:
        """Resolve path to euxis-loop executable using configurable strategy.

        Search order:
        1. EUXIS_LOOP_PATH environment variable (if set)
        2. ${REPO_ROOT}/cli/bin/euxis-loop (repo-local install)
        3. ~/bin/euxis-loop (user binary location)
        4. ${EUXIS_HOME}/bin/euxis-loop (local installation)
        5. ${EUXIS_HOME}/euxis-cli/bin/euxis-loop (legacy local installation)

        Raises:
            AgentError: If euxis-loop executable cannot be found

        Returns:
            Path to euxis-loop executable

        """
        # Check environment variable override first
        env_path = os.environ.get("EUXIS_LOOP_PATH")
        if env_path:
            loop_path = Path(env_path)
            if loop_path.is_file():
                return loop_path
            logger.warning("EUXIS_LOOP_PATH points to non-existent file: %s", env_path)

        repo_loop = self.repo_root / "cli" / "bin" / "euxis-loop"
        use_repo_loop = self.euxis_home.resolve() == self.repo_root.resolve()
        if use_repo_loop and repo_loop.exists():
            return repo_loop

        # Check user bin directory
        user_bin_loop = Path.home() / "bin" / "euxis-loop"
        if user_bin_loop.exists():
            return user_bin_loop

        # Check EUXIS_HOME bin directory
        local_loop = self.euxis_home / "bin" / "euxis-loop"
        if local_loop.exists():
            return local_loop

        # Check legacy EUXIS_HOME/cli/bin path
        legacy_loop = self.euxis_home / "cli" / "bin" / "euxis-loop"
        if legacy_loop.exists():
            return legacy_loop

        # Not found - raise error with helpful context
        msg = (
            "euxis-loop executable not found. Searched:\n"
            f"  - EUXIS_LOOP_PATH: {env_path or '(not set)'}\n"
            f"  - Repo bin: {repo_loop if use_repo_loop else '(skipped)'}\n"
            f"  - User bin: {user_bin_loop}\n"
            f"  - EUXIS_HOME: {local_loop}\n"
            f"  - EUXIS_HOME legacy: {legacy_loop}\n"
            "Run 'make install' to set up symlinks."
        )
        raise AgentError(msg)

    def check_branch_protection(self) -> None:
        """Check if current branch is protected."""
        try:
            result = subprocess.run(
                ["git", "rev-parse", "--abbrev-ref", "HEAD"],
                capture_output=True,
                text=True,
                check=True
            )
            branch = result.stdout.strip()

            protected_branches = {"main", "master", "develop", "production", "staging"}
            if branch in protected_branches:
                msg = (
                    f"Dispatch cannot run on protected branch '{branch}'. "
                    "Switch to a feature branch first: git checkout -b feat/<version>"
                )
                raise ValidationError(msg)

            # Check for compliant feature branch naming
            compliant_prefixes = ("feat/", "fix/", "refactor/", "chore/")
            if not branch.startswith(compliant_prefixes):
                logger.warning("Non-standard branch name '%s'. Proceeding...", branch)

            logger.info("Branch check passed: %s", branch)

        except subprocess.CalledProcessError:
            logger.warning("Could not determine current git branch")

    def setup_log_directory(self) -> Path:
        """Create temporary directory for dispatch logs."""
        self.log_dir = Path(tempfile.mkdtemp(prefix="euxis_dispatch_"))
        logger.info("Created log directory: %s", self.log_dir)
        return self.log_dir

    def dispatch_single_agent(
        self,
        dispatch: ManifestDispatch,
        index: int,
        mode: str,
        project: str
    ) -> subprocess.Popen[bytes]:
        """Dispatch a single agent and return the process."""
        if not self.log_dir:
            self.setup_log_directory()

        log_file = self.log_dir / f"{dispatch.agent}_{index}.log"

        # Build task based on mode
        task = dispatch.task
        if mode == "mesh":
            # Check if agent has dispatch authority
            dispatch_agents = {"architect", "inspector", "responder", "gatekeeper"}
            if dispatch.agent in dispatch_agents:
                task = (
                    f"[MESH MODE] You have dispatch authority. You MAY produce a "
                    f"sub-manifest to coordinate specialized agents directly. {task}"
                )
        elif mode == "federated":
            task = (
                f"[FEDERATED MODE] Operate autonomously on project '{project}'. "
                f"Produce complete output independently. {task}"
            )

        # Add atomic file operation instructions
        atomic_instructions = """

ATOMIC FILE OPERATIONS REQUIRED:
- Use temp files for all write operations: write to 'file.tmp' then rename to 'file'
- This prevents corruption if your process is interrupted
- Pattern: write_content > temp_file && mv temp_file final_file"""

        task += atomic_instructions

        # Build command
        euxis_loop = self._resolve_euxis_loop_path()

        # Security: Agent names validated against registry allowlist and shell
        # metacharacter regex in validate_manifest(). subprocess.Popen with a list
        # (no shell=True) passes args directly to execvp — no shell interpretation
        # occurs, so shlex.quote() is NOT needed and would add literal quotes.
        cmd = [
            str(euxis_loop),
            dispatch.agent,
            task,
            dispatch.verify_cmd or "true",
            "3"  # max attempts
        ]

        logger.info("[%s] -> %s (%s)", dispatch.priority, dispatch.agent, mode)

        # Start process
        with log_file.open("w") as f:
            return subprocess.Popen(
                cmd,
                stdout=f,
                stderr=subprocess.STDOUT,
                cwd=Path.cwd(),
            )

    def execute_dispatches(
        self,
        manifest: Manifest,
        mode: str | None = None
    ) -> list[DispatchResult]:
        """Execute all dispatches in the manifest."""
        dispatch_mode = mode or manifest.mode

        logger.info("EUXIS FLEET DEPLOYMENT")
        logger.info("  Project:    %s", manifest.project)
        logger.info("  Mode:       %s", dispatch_mode)
        logger.info("  Dispatches: %s", len(manifest.dispatches))

        if len(manifest.dispatches) == 0:
            logger.info("No dispatches found in manifest")
            return []

        # Check for advanced execution modes
        has_stages = any(d.stage > 1 or d.depends_on or d.locks for d in manifest.dispatches)
        has_phases = len(manifest.rollout_phases) > 0

        if has_phases:
            return self._execute_phased_rollout(manifest, dispatch_mode)
        if has_stages:
            return self._execute_by_stages(manifest, dispatch_mode)
        return self._execute_flat_dispatch(manifest, dispatch_mode)

    def _execute_flat_dispatch(
        self,
        manifest: Manifest,
        mode: str
    ) -> list[DispatchResult]:
        """Execute flat (legacy) dispatch mode."""
        if not self.log_dir:
            self.setup_log_directory()

        processes: list[tuple[subprocess.Popen[bytes], ManifestDispatch, int]] = []

        # Start all processes
        for i, dispatch in enumerate(manifest.dispatches):
            process = self.dispatch_single_agent(dispatch, i, mode, manifest.project)
            processes.append((process, dispatch, i))
            time.sleep(2)  # Stagger startup

        logger.info("All %s agents deployed (%s mode)", len(manifest.dispatches), mode)
        logger.info("Logs: %s", self.log_dir)

        # Wait for completion and collect results
        results = []
        for process, dispatch, index in processes:
            start_time = time.time()
            return_code = process.wait()
            duration = time.time() - start_time

            log_file = self.log_dir / f"{dispatch.agent}_{index}.log"
            success = return_code == 0

            if success:
                logger.info("[PASS] %s", dispatch.agent)
            else:
                logger.error("[FAIL] %s (see %s)", dispatch.agent, log_file)

            results.append(DispatchResult(
                agent=dispatch.agent,
                index=index,
                success=success,
                log_file=log_file,
                duration=duration,
                return_code=return_code
            ))

        return results

    def _execute_by_stages(
        self,
        manifest: Manifest,
        mode: str
    ) -> list[DispatchResult]:
        """Execute stage-based dispatch mode."""
        logger.info("=== STAGE-BASED EXECUTION MODE DETECTED ===")

        # Determine max stage
        max_stage = max((d.stage for d in manifest.dispatches), default=1)
        logger.info("  Stages: 1-%s", max_stage)

        completed_agents: set[str] = set()
        all_results: list[DispatchResult] = []

        for current_stage in range(1, max_stage + 1):
            logger.info("---------------------------------------------------")
            logger.info("  STAGE %s", current_stage)
            logger.info("---------------------------------------------------")

            # Get agents for this stage
            stage_dispatches = []
            for i, dispatch in enumerate(manifest.dispatches):
                if dispatch.stage == current_stage:
                    # Check dependencies
                    deps_met = all(dep in completed_agents for dep in dispatch.depends_on)
                    if deps_met:
                        stage_dispatches.append((dispatch, i))
                    else:
                        missing_deps = set(dispatch.depends_on) - completed_agents
                        logger.info(
                            "  [SKIP] Agent %s waiting for: %s",
                            dispatch.agent,
                            ", ".join(missing_deps),
                        )

            if not stage_dispatches:
                logger.info("  No agents ready for Stage %s", current_stage)
                continue

            # Execute stage agents in parallel
            stage_processes = []
            for dispatch, index in stage_dispatches:
                process = self.dispatch_single_agent(dispatch, index, mode, manifest.project)
                stage_processes.append((process, dispatch, index))
                time.sleep(1)  # Stagger startup

            # Wait for stage completion
            stage_failed = 0
            for process, dispatch, index in stage_processes:
                start_time = time.time()
                return_code = process.wait()
                duration = time.time() - start_time

                log_file = self.log_dir / f"{dispatch.agent}_{index}.log"
                success = return_code == 0

                if success:
                    logger.info("  [PASS] %s", dispatch.agent)
                    completed_agents.add(dispatch.agent)
                else:
                    logger.error("  [FAIL] %s (see %s)", dispatch.agent, log_file)
                    stage_failed += 1

                all_results.append(DispatchResult(
                    agent=dispatch.agent,
                    index=index,
                    success=success,
                    log_file=log_file,
                    duration=duration,
                    return_code=return_code
                ))

            if stage_failed > 0:
                logger.error(
                    "  [STAGE %s FAILED] %s agent(s) failed",
                    current_stage,
                    stage_failed,
                )
                logger.error("  ABORTING remaining stages due to failures")
                break

            logger.info(
                "  [STAGE %s COMPLETE] %s agent(s) succeeded",
                current_stage,
                len(stage_dispatches),
            )

        return all_results

    def _execute_phased_rollout(
        self,
        manifest: Manifest,
        mode: str
    ) -> list[DispatchResult]:
        """Execute phased rollout mode."""
        logger.info("=== PHASED ROLLOUT ===")

        all_results: list[DispatchResult] = []
        total_failed = 0

        for phase_idx, phase in enumerate(manifest.rollout_phases):
            phase_name = phase.get("name", f"Phase {phase_idx}")
            phase_num = phase.get("phase", phase_idx + 1)
            checkpoint = phase.get("checkpoint", "")

            logger.info("---------------------------------------------------")
            logger.info("  PHASE %s: %s", phase_num, phase_name)
            logger.info("---------------------------------------------------")

            # Get dispatch indices for this phase
            dispatch_indices = phase.get("parallel_group", [])
            phase_processes = []

            for dispatch_idx in dispatch_indices:
                if dispatch_idx >= len(manifest.dispatches):
                    logger.error("Invalid dispatch index %s", dispatch_idx)
                    continue

                dispatch = manifest.dispatches[dispatch_idx]
                process = self.dispatch_single_agent(
                    dispatch, dispatch_idx, mode, manifest.project
                )
                phase_processes.append((process, dispatch, dispatch_idx))
                time.sleep(2)  # Stagger startup

            logger.info("  Waiting for Phase %s to complete...", phase_num)

            # Wait for phase completion
            phase_failed = 0
            for process, dispatch, index in phase_processes:
                start_time = time.time()
                return_code = process.wait()
                duration = time.time() - start_time

                log_file = self.log_dir / f"{dispatch.agent}_{index}.log"
                success = return_code == 0

                if success:
                    logger.info("  [PASS] %s (dispatch #%s)", dispatch.agent, index)
                else:
                    logger.error(
                        "  [FAIL] %s (dispatch #%s — see %s)",
                        dispatch.agent,
                        index,
                        log_file,
                    )
                    phase_failed += 1

                all_results.append(DispatchResult(
                    agent=dispatch.agent,
                    index=index,
                    success=success,
                    log_file=log_file,
                    duration=duration,
                    return_code=return_code
                ))

            if phase_failed > 0:
                logger.error(
                    "  [PHASE %s FAILED] %s dispatch(es) failed.",
                    phase_num,
                    phase_failed,
                )
                if checkpoint:
                    logger.error("  Checkpoint: %s", checkpoint)
                logger.error("  ABORTING remaining phases.")
                total_failed += phase_failed
                break

            # Evaluate checkpoint
            if checkpoint:
                logger.info("  [CHECKPOINT] Phase %s — %s", phase_num, checkpoint)
                logger.info("  [CHECKPOINT] Phase %s → CONTINUE", phase_num)

        return all_results


def main():
    """CLI entry point for testing."""
    parser = argparse.ArgumentParser(description="Euxis Core Dispatch Engine")
    parser.add_argument("manifest", help="Path to manifest JSON file")
    parser.add_argument("--mode", choices=list(DEFAULT_DISPATCH_MODES),
                       help="Override dispatch mode")
    parser.add_argument("--validate-only", action="store_true",
                       help="Only validate manifest, don't execute")

    args = parser.parse_args()

    try:
        engine = DispatchEngine()
        manifest = Manifest.from_file(Path(args.manifest))

        # Validate manifest
        engine.validate_manifest(manifest)
        engine.check_branch_protection()

        if args.validate_only:
            logger.info("Manifest validation passed")
            return 0

        # Execute dispatches
        results = engine.execute_dispatches(manifest, args.mode)

        # Report final results
        total_dispatches = len(results)
        failed_dispatches = sum(1 for r in results if not r.success)

        if failed_dispatches == 0:
            logger.info(
                "ALL %s MISSIONS COMPLETE (%s mode).",
                total_dispatches,
                manifest.mode,
            )
            return 0
        logger.error("%s of %s missions failed.", failed_dispatches, total_dispatches)
        return 1

    except (ValidationError, AgentError):
        logger.exception("Error")
        return 1
    except Exception:
        logger.exception("Unexpected error")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
