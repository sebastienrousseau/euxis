#!/usr/bin/env python3
"""Validate root workspace topology for naming consistency and repo-split readiness."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


def _is_allowed_dir(name: str, allowed_workspace_dirs: set[str], prefix: str) -> bool:
    if name in allowed_workspace_dirs:
        return True
    if name.startswith(prefix):
        return True
    return False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument(
        "--policy",
        default="euxis-ops/quality/workspace_topology_policy.json",
    )
    parser.add_argument(
        "--json-output",
        default="euxis-data/release/workspace-topology.json",
    )
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    policy_path = repo_root / args.policy
    output_path = repo_root / args.json_output

    policy = json.loads(policy_path.read_text(encoding="utf-8"))
    prefix = str(policy["allowed_prefix_for_packages"])
    required_package_dirs = set(policy["required_package_dirs"])
    canonical_package_dirs = set(policy.get("canonical_package_dirs", required_package_dirs))
    legacy_package_aliases = dict(policy.get("legacy_package_aliases", {}))
    package_name_regex = re.compile(policy.get("package_name_regex", r"^euxis-[a-z0-9]+$"))
    allowed_workspace_dirs = set(policy["allowed_workspace_dirs"])
    deprecated_workspace_dirs = set(policy.get("deprecated_workspace_dirs", []))

    root_dirs = sorted(
        entry.name for entry in repo_root.iterdir() if entry.is_dir()
    )

    invalid_dirs = [
        name
        for name in root_dirs
        if not _is_allowed_dir(name, allowed_workspace_dirs, prefix)
    ]
    missing_package_dirs = sorted(required_package_dirs - set(root_dirs))
    unexpected_package_dirs = sorted(
        name
        for name in root_dirs
        if name.startswith(prefix)
        and name not in required_package_dirs
        and name not in allowed_workspace_dirs
    )
    invalid_package_name_format = sorted(
        name
        for name in root_dirs
        if name.startswith(prefix)
        and name not in legacy_package_aliases
        and not package_name_regex.match(name)
    )
    legacy_aliases_present = sorted(
        name for name in root_dirs if name in legacy_package_aliases
    )
    missing_canonical_targets = sorted(
        canonical
        for canonical in canonical_package_dirs
        if canonical not in root_dirs and canonical not in legacy_package_aliases.values()
    )
    deprecated_present = sorted(
        name for name in root_dirs if name in deprecated_workspace_dirs
    )

    failures: list[str] = []
    if invalid_dirs:
        failures.append(f"invalid_root_dirs={invalid_dirs}")
    if missing_package_dirs:
        failures.append(f"missing_required_packages={missing_package_dirs}")
    if unexpected_package_dirs:
        failures.append(f"unexpected_euxis_package_dirs={unexpected_package_dirs}")
    if invalid_package_name_format:
        failures.append(f"invalid_package_name_format={invalid_package_name_format}")
    if missing_canonical_targets:
        failures.append(f"missing_canonical_package_names={missing_canonical_targets}")
    if legacy_aliases_present:
        failures.append(f"legacy_alias_dirs_present={legacy_aliases_present}")
    if deprecated_present:
        failures.append(f"deprecated_root_dirs_present={deprecated_present}")

    result = {
        "status": "ok" if not failures else "failed",
        "root_dirs": root_dirs,
        "invalid_dirs": invalid_dirs,
        "missing_required_package_dirs": missing_package_dirs,
        "unexpected_euxis_package_dirs": unexpected_package_dirs,
        "invalid_package_name_format": invalid_package_name_format,
        "legacy_aliases_present": legacy_aliases_present,
        "legacy_alias_target_map": legacy_package_aliases,
        "deprecated_dirs_present": deprecated_present,
        "policy_path": str(policy_path),
        "failures": failures,
    }

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")

    if failures:
        print("Workspace topology validation failed.")
        print(json.dumps(result, indent=2))
        return 1

    print("Workspace topology validation passed.")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
