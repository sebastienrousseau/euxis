# SPDX-License-Identifier: AGPL-3.0-or-later

from pathlib import Path

import pytest

from security import SecurityPolicy, merge_policy, validate_auth_mode

def test_security_source_layout_exists() -> None:
    repo_root = Path(__file__).resolve().parents[2]
    assert (repo_root / "euxis-security" / "src" / "security" / "__init__.py").exists()


def test_validate_auth_mode() -> None:
    assert validate_auth_mode("token") == "token"
    assert validate_auth_mode(" PASSWORD ") == "password"
    with pytest.raises(ValueError):
        validate_auth_mode("oauth2")


def test_merge_policy_defaults_and_overrides() -> None:
    default_policy = merge_policy()
    assert isinstance(default_policy, SecurityPolicy)
    assert default_policy.as_dict()["auth_mode"] == "token"

    policy = merge_policy(
        {
            "auth_mode": "none",
            "require_https": True,
            "require_mfa_for_elevated": False,
            "allow_query_token": True,
        }
    )
    assert policy.auth_mode == "none"
    assert policy.require_https is True
    assert policy.require_mfa_for_elevated is False
    assert policy.allow_query_token is True
