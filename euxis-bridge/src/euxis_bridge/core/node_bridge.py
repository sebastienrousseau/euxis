"""Bounded Node command execution for bridged JavaScript skills."""

from __future__ import annotations

import json
import os
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Any


class NodeSkillError(RuntimeError):
    """Raised when a bridged Node skill fails."""


@dataclass(slots=True)
class NodeSkillBridge:
    """Execute Node-based skills with strict bounds."""

    timeout_seconds: int = 20
    env_allowlist: tuple[str, ...] = ("PATH", "HOME", "NODE_PATH")

    def _safe_env(self) -> dict[str, str]:
        return {key: value for key, value in os.environ.items() if key in self.env_allowlist}

    def run(self, entrypoint: Path, payload: dict[str, Any]) -> dict[str, Any]:
        if not entrypoint.exists():
            raise NodeSkillError(f"Node entrypoint not found: {entrypoint}")

        process = subprocess.run(
            ["node", str(entrypoint), "--payload", json.dumps(payload, separators=(",", ":"))],
            capture_output=True,
            text=True,
            env=self._safe_env(),
            timeout=self.timeout_seconds,
            check=False,
        )
        if process.returncode != 0:
            stderr = process.stderr.strip() or "unknown node error"
            raise NodeSkillError(f"Node skill failed ({process.returncode}): {stderr}")

        return {
            "status": "ok",
            "stdout": process.stdout,
            "stderr": process.stderr,
            "returncode": process.returncode,
        }
