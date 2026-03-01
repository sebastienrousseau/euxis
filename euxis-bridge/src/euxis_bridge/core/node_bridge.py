"""Bounded Node command execution for bridged JavaScript skills."""

from __future__ import annotations

import json
import os
import subprocess
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from euxis_bridge.core.audit import AuditLogger
from euxis_bridge.core.policy import SkillExecutionPolicy
from euxis_bridge.core.verification import require_verified


class NodeSkillError(RuntimeError):
    """Raised when a bridged Node skill fails."""


@dataclass(slots=True)
class NodeSkillBridge:
    """Execute Node-based skills with strict bounds."""

    timeout_seconds: int = 20
    env_allowlist: tuple[str, ...] = ("PATH", "HOME", "NODE_PATH")
    policy: SkillExecutionPolicy | None = field(default=None)

    def _safe_env(self) -> dict[str, str]:
        env = {key: value for key, value in os.environ.items() if key in self.env_allowlist}
        if self.policy is not None:
            env["NODE_OPTIONS"] = f"--max-old-space-size={self.policy.resources.memory_mb}"
        return env

    def _effective_timeout(self) -> int:
        if self.policy is not None:
            return self.policy.resources.timeout_seconds
        return self.timeout_seconds

    def _audit_logger(self) -> AuditLogger | None:
        if self.policy is not None and self.policy.audit_log_path is not None:
            return AuditLogger(self.policy.audit_log_path)
        return None

    def run(
        self,
        entrypoint: Path,
        payload: dict[str, Any],
        skill_slug: str = "unknown",
    ) -> dict[str, Any]:
        if not entrypoint.exists():
            raise NodeSkillError(f"Node entrypoint not found: {entrypoint}")

        logger = self._audit_logger()

        if self.policy is not None and self.policy.require_signature:
            if self.policy.public_key_path is None:
                raise NodeSkillError("policy requires signature but no public_key_path set")
            require_verified(entrypoint, self.policy.public_key_path)
            if logger:
                logger.log_verify(skill_slug, str(entrypoint), success=True)

        if logger:
            logger.log_execute(skill_slug, str(entrypoint))

        try:
            process = subprocess.run(
                ["node", str(entrypoint), "--payload", json.dumps(payload, separators=(",", ":"))],
                capture_output=True,
                text=True,
                env=self._safe_env(),
                timeout=self._effective_timeout(),
                check=False,
            )
        except Exception as exc:
            if logger:
                logger.log_error(skill_slug, str(entrypoint), {"error": str(exc)})
            raise

        if process.returncode != 0:
            stderr = process.stderr.strip() or "unknown node error"
            if logger:
                logger.log_error(skill_slug, str(entrypoint), {"returncode": process.returncode})
            raise NodeSkillError(f"Node skill failed ({process.returncode}): {stderr}")

        result: dict[str, Any] = {
            "status": "ok",
            "stdout": process.stdout,
            "stderr": process.stderr,
            "returncode": process.returncode,
        }

        if (
            self.policy is not None
            and self.policy.output_schema is not None
            and process.stdout.strip()
        ):
            try:
                output_data = json.loads(process.stdout)
                required = self.policy.output_schema.get("required", [])
                for field_name in required:
                    if field_name not in output_data:
                        raise NodeSkillError(
                            f"output missing required field: {field_name}"
                        )
            except json.JSONDecodeError:
                pass

        if logger:
            logger.log_complete(skill_slug, str(entrypoint))

        return result
