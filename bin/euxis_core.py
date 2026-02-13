#!/usr/bin/env python3
# (c) 2026 Euxis Fleet. All rights reserved.
"""
Euxis Core Dispatch Engine

Python core extracted from euxis-dispatch shell script.
Provides DispatchEngine class for fleet deployment coordination.
"""

from __future__ import annotations

import json
import logging
import os
import sqlite3
import subprocess
import tempfile
import time
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

EUXIS_HOME = Path.home() / ".euxis"
DEFAULT_DISPATCH_MODES = {"hierarchical", "mesh", "federated"}


class DispatchError(Exception):
    """Base exception for dispatch operations."""
    pass


class ValidationError(DispatchError):
    """Raised when manifest or configuration validation fails."""
    pass


class AgentError(DispatchError):
    """Raised when agent operations fail."""
    pass


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
        except (OSError, json.JSONDecodeError) as e:
            raise ValidationError(f"Failed to load manifest {manifest_path}: {e}")

        # Validate required fields
        if "dispatches" not in data:
            raise ValidationError("Manifest missing 'dispatches' field")

        dispatches = []
        for i, dispatch_data in enumerate(data["dispatches"]):
            if not isinstance(dispatch_data, dict):
                raise ValidationError(f"Dispatch #{i} is not a valid object")

            # Required fields
            if "agent" not in dispatch_data:
                raise ValidationError(f"Dispatch #{i} missing 'agent' field")
            if "task" not in dispatch_data:
                raise ValidationError(f"Dispatch #{i} missing 'task' field")
            if "priority" not in dispatch_data:
                raise ValidationError(f"Dispatch #{i} missing 'priority' field")

            dispatches.append(ManifestDispatch(
                agent=dispatch_data["agent"],
                task=dispatch_data["task"],
                priority=dispatch_data["priority"],
                verify_cmd=dispatch_data.get("verify_cmd"),
                stage=dispatch_data.get("stage", 1),
                depends_on=dispatch_data.get("depends_on", []),
                locks=dispatch_data.get("locks", [])
            ))

        return cls(
            project=data.get("project", "unknown"),
            mode=data.get("mode", "hierarchical"),
            dispatches=dispatches,
            rollout_phases=data.get("rollout_phases", [])
        )


class DispatchEngine:
    """Core dispatch engine for fleet coordination."""

    def __init__(self, euxis_home: Path | None = None):
        self.euxis_home = euxis_home or EUXIS_HOME
        self.registry_path = self.euxis_home / "registry.json"
        self.registry_db = self.euxis_home / "registry.db"
        self.log_dir: Path | None = None
        self._valid_agents: set[str] | None = None

        logger.info(f"Initialized DispatchEngine with EUXIS_HOME: {self.euxis_home}")

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
                logger.info(f"Loaded {len(self._valid_agents)} agents from registry.db")
                return self._valid_agents
            except sqlite3.Error as e:
                logger.warning(f"SQLite registry query failed, falling back to JSON: {e}")

        # Fall back to JSON
        if not self.registry_path.exists():
            logger.warning(f"Registry not found at {self.registry_path}")
            self._valid_agents = set()
            return self._valid_agents

        try:
            with self.registry_path.open() as f:
                registry_data = json.load(f)

            self._valid_agents = {
                agent["id"] for agent in registry_data.get("agents", [])
                if isinstance(agent, dict) and "id" in agent
            }
            logger.info(f"Loaded {len(self._valid_agents)} agents from registry.json")

        except (OSError, json.JSONDecodeError) as e:
            logger.error(f"Failed to load agent registry: {e}")
            self._valid_agents = set()

        return self._valid_agents

    def validate_manifest(self, manifest: Manifest) -> None:
        """Validate manifest against registry and system constraints."""
        logger.info(f"Validating manifest: {manifest.project} ({manifest.mode})")

        # Validate dispatch mode
        if manifest.mode not in DEFAULT_DISPATCH_MODES:
            raise ValidationError(
                f"Invalid dispatch mode '{manifest.mode}'. "
                f"Must be one of: {', '.join(sorted(DEFAULT_DISPATCH_MODES))}"
            )

        # Load valid agents
        valid_agents = self._load_agent_registry()

        # Validate agents
        invalid_agents = []
        for dispatch in manifest.dispatches:
            if valid_agents and dispatch.agent not in valid_agents:
                invalid_agents.append(dispatch.agent)

        if invalid_agents:
            raise ValidationError(
                f"Manifest references unknown agents: {', '.join(invalid_agents)}. "
                f"Valid agents: {', '.join(sorted(valid_agents))}"
            )

        # Validate verify commands
        missing_cmds = []
        for i, dispatch in enumerate(manifest.dispatches):
            if dispatch.verify_cmd:
                # Extract first binary from command
                cmd_parts = dispatch.verify_cmd.strip().lstrip("! ").split()
                if cmd_parts:
                    binary = cmd_parts[0]
                    if not self._check_command_available(binary):
                        missing_cmds.append(f"[{dispatch.agent}] {binary}")

        if missing_cmds:
            raise ValidationError(
                f"Manifest verify commands reference missing tools: {', '.join(missing_cmds)}"
            )

        logger.info(f"Manifest validation passed: {len(manifest.dispatches)} dispatches")

    def _check_command_available(self, command: str) -> bool:
        """Check if a command is available in PATH."""
        try:
            subprocess.run(
                ["command", "-v", command],
                check=True,
                capture_output=True,
                text=True
            )
            return True
        except subprocess.CalledProcessError:
            return False

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
                raise ValidationError(
                    f"Dispatch cannot run on protected branch '{branch}'. "
                    "Switch to a feature branch first: git checkout -b feat/<version>"
                )

            # Check for compliant feature branch naming
            compliant_prefixes = ("feat/", "fix/", "refactor/", "chore/")
            if not branch.startswith(compliant_prefixes):
                logger.warning(f"Non-standard branch name '{branch}'. Proceeding...")

            logger.info(f"Branch check passed: {branch}")

        except subprocess.CalledProcessError:
            logger.warning("Could not determine current git branch")

    def setup_log_directory(self) -> Path:
        """Create temporary directory for dispatch logs."""
        self.log_dir = Path(tempfile.mkdtemp(prefix="euxis_dispatch_"))
        logger.info(f"Created log directory: {self.log_dir}")
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
        euxis_loop = Path.home() / "bin" / "euxis-loop"
        if not euxis_loop.exists():
            euxis_loop = self.euxis_home / "bin" / "euxis-loop"

        cmd = [
            str(euxis_loop),
            dispatch.agent,
            task,
            dispatch.verify_cmd or "",
            "3"  # max attempts
        ]

        logger.info(f"[{dispatch.priority}] -> {dispatch.agent} ({mode})")

        # Start process
        with log_file.open("w") as f:
            process = subprocess.Popen(
                cmd,
                stdout=f,
                stderr=subprocess.STDOUT,
                cwd=os.getcwd()
            )

        return process

    def execute_dispatches(
        self,
        manifest: Manifest,
        mode: str | None = None
    ) -> list[DispatchResult]:
        """Execute all dispatches in the manifest."""
        dispatch_mode = mode or manifest.mode

        logger.info(f"EUXIS FLEET DEPLOYMENT")
        logger.info(f"  Project:    {manifest.project}")
        logger.info(f"  Mode:       {dispatch_mode}")
        logger.info(f"  Dispatches: {len(manifest.dispatches)}")

        if len(manifest.dispatches) == 0:
            logger.info("No dispatches found in manifest")
            return []

        # Check for advanced execution modes
        has_stages = any(d.stage > 1 or d.depends_on or d.locks for d in manifest.dispatches)
        has_phases = len(manifest.rollout_phases) > 0

        if has_phases:
            return self._execute_phased_rollout(manifest, dispatch_mode)
        elif has_stages:
            return self._execute_by_stages(manifest, dispatch_mode)
        else:
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

        logger.info(f"All {len(manifest.dispatches)} agents deployed ({mode} mode)")
        logger.info(f"Logs: {self.log_dir}")

        # Wait for completion and collect results
        results = []
        for process, dispatch, index in processes:
            start_time = time.time()
            return_code = process.wait()
            duration = time.time() - start_time

            log_file = self.log_dir / f"{dispatch.agent}_{index}.log"
            success = return_code == 0

            if success:
                logger.info(f"[PASS] {dispatch.agent}")
            else:
                logger.error(f"[FAIL] {dispatch.agent} (see {log_file})")

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
        logger.info(f"  Stages: 1-{max_stage}")

        completed_agents: set[str] = set()
        all_results: list[DispatchResult] = []

        for current_stage in range(1, max_stage + 1):
            logger.info(f"---------------------------------------------------")
            logger.info(f"  STAGE {current_stage}")
            logger.info(f"---------------------------------------------------")

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
                        logger.info(f"  [SKIP] Agent {dispatch.agent} waiting for: {', '.join(missing_deps)}")

            if not stage_dispatches:
                logger.info(f"  No agents ready for Stage {current_stage}")
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
                    logger.info(f"  [PASS] {dispatch.agent}")
                    completed_agents.add(dispatch.agent)
                else:
                    logger.error(f"  [FAIL] {dispatch.agent} (see {log_file})")
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
                logger.error(f"  [STAGE {current_stage} FAILED] {stage_failed} agent(s) failed")
                logger.error(f"  ABORTING remaining stages due to failures")
                break

            logger.info(f"  [STAGE {current_stage} COMPLETE] {len(stage_dispatches)} agent(s) succeeded")

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

            logger.info(f"---------------------------------------------------")
            logger.info(f"  PHASE {phase_num}: {phase_name}")
            logger.info(f"---------------------------------------------------")

            # Get dispatch indices for this phase
            dispatch_indices = phase.get("parallel_group", [])
            phase_processes = []

            for dispatch_idx in dispatch_indices:
                if dispatch_idx >= len(manifest.dispatches):
                    logger.error(f"Invalid dispatch index {dispatch_idx}")
                    continue

                dispatch = manifest.dispatches[dispatch_idx]
                process = self.dispatch_single_agent(
                    dispatch, dispatch_idx, mode, manifest.project
                )
                phase_processes.append((process, dispatch, dispatch_idx))
                time.sleep(2)  # Stagger startup

            logger.info(f"  Waiting for Phase {phase_num} to complete...")

            # Wait for phase completion
            phase_failed = 0
            for process, dispatch, index in phase_processes:
                start_time = time.time()
                return_code = process.wait()
                duration = time.time() - start_time

                log_file = self.log_dir / f"{dispatch.agent}_{index}.log"
                success = return_code == 0

                if success:
                    logger.info(f"  [PASS] {dispatch.agent} (dispatch #{index})")
                else:
                    logger.error(f"  [FAIL] {dispatch.agent} (dispatch #{index} — see {log_file})")
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
                logger.error(f"  [PHASE {phase_num} FAILED] {phase_failed} dispatch(es) failed.")
                if checkpoint:
                    logger.error(f"  Checkpoint: {checkpoint}")
                logger.error("  ABORTING remaining phases.")
                total_failed += phase_failed
                break

            # Evaluate checkpoint
            if checkpoint:
                logger.info(f"  [CHECKPOINT] Phase {phase_num} — {checkpoint}")
                logger.info(f"  [CHECKPOINT] Phase {phase_num} → CONTINUE")

        return all_results


def main():
    """CLI entry point for testing."""
    import argparse

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
            logger.info(f"ALL {total_dispatches} MISSIONS COMPLETE ({manifest.mode} mode).")
            return 0
        else:
            logger.error(f"{failed_dispatches} of {total_dispatches} missions failed.")
            return 1

    except (ValidationError, AgentError) as e:
        logger.error(f"Error: {e}")
        return 1
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        return 1


if __name__ == "__main__":
    exit(main())