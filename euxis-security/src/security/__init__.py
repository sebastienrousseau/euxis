# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Security policy defaults for Euxis packages."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict

VALID_AUTH_MODES = {"token", "password", "none"}


@dataclass(frozen=True)
class SecurityPolicy:
    """Portable baseline security policy for gateway-facing packages."""

    auth_mode: str = "token"
    require_https: bool = False
    require_mfa_for_elevated: bool = True
    allow_query_token: bool = False

    def as_dict(self) -> Dict[str, Any]:
        return {
            "auth_mode": self.auth_mode,
            "require_https": self.require_https,
            "require_mfa_for_elevated": self.require_mfa_for_elevated,
            "allow_query_token": self.allow_query_token,
        }


def validate_auth_mode(mode: str) -> str:
    """Normalize and validate auth mode."""
    normalized = (mode or "").strip().lower()
    if normalized not in VALID_AUTH_MODES:
        raise ValueError(f"Unsupported auth mode: {mode}")
    return normalized


def merge_policy(overrides: Dict[str, Any] | None = None) -> SecurityPolicy:
    """Create a policy from trusted defaults plus optional overrides."""
    merged = SecurityPolicy()
    if not overrides:
        return merged
    mode = validate_auth_mode(str(overrides.get("auth_mode", merged.auth_mode)))
    return SecurityPolicy(
        auth_mode=mode,
        require_https=bool(overrides.get("require_https", merged.require_https)),
        require_mfa_for_elevated=bool(
            overrides.get("require_mfa_for_elevated", merged.require_mfa_for_elevated)
        ),
        allow_query_token=bool(overrides.get("allow_query_token", merged.allow_query_token)),
    )


__all__ = [
    "SecurityPolicy",
    "VALID_AUTH_MODES",
    "merge_policy",
    "validate_auth_mode",
]
