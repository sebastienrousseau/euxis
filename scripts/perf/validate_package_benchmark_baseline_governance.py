#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later

"""Validate package benchmark baseline governance artifact consistency."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path


SEMVER_TAG_RE = re.compile(r"^v(\d+)\.(\d+)\.(\d+)$")
REQUIRED_METRICS = ("wall_ms", "cpu_ms", "memory_mb")


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _canonical_sha256(payload: dict) -> str:
    canonical = json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(canonical).hexdigest()


def _fail(message: str) -> int:
    print(f"Package benchmark baseline governance validation failed: {message}", file=sys.stderr)
    return 1


def _parse_tag(value: object, field: str) -> tuple[int, int, int]:
    if not isinstance(value, str):
        raise ValueError(f"{field} must be a string")
    match = SEMVER_TAG_RE.match(value)
    if not match:
        raise ValueError(f"{field} must match vX.Y.Z")
    return tuple(int(piece) for piece in match.groups())


def _validate_packages_shape(payload: dict, name: str) -> None:
    packages = payload.get("packages")
    if not isinstance(packages, dict) or not packages:
        raise ValueError(f"{name}.packages must be a non-empty object")

    for package_name, metrics in packages.items():
        if not isinstance(package_name, str) or not package_name:
            raise ValueError(f"{name}.packages key must be non-empty string")
        if not isinstance(metrics, dict):
            raise ValueError(f"{name}.packages[{package_name}] must be object")
        for metric_name in REQUIRED_METRICS:
            metric = metrics.get(metric_name)
            if not isinstance(metric, dict):
                raise ValueError(
                    f"{name}.packages[{package_name}].{metric_name} must be object"
                )
            p95 = metric.get("p95")
            if not isinstance(p95, (int, float)):
                raise ValueError(
                    f"{name}.packages[{package_name}].{metric_name}.p95 must be numeric"
                )
            if float(p95) < 0:
                raise ValueError(
                    f"{name}.packages[{package_name}].{metric_name}.p95 must be >= 0"
                )


def _validate_baseline_metadata(baseline: dict) -> None:
    status = baseline.get("status")
    if status != "ok":
        raise ValueError("baseline.status must be 'ok'")
    iterations = baseline.get("iterations")
    if not isinstance(iterations, int) or iterations <= 0:
        raise ValueError("baseline.iterations must be a positive integer")


def _compute_flagged(
    deltas: dict[str, dict[str, object]],
    threshold_percent: float,
) -> dict[str, list[str]]:
    flagged_packages: dict[str, list[str]] = {}
    for package_name, metric_values in deltas.items():
        if not isinstance(metric_values, dict):
            raise ValueError(f"proposal.deltas_vs_current_baseline[{package_name}] must be object")
        flags: list[str] = []
        for metric_name in (
            "wall_p95_delta_percent",
            "cpu_ms_p95_delta_percent",
            "memory_p95_delta_percent",
        ):
            raw_delta = metric_values.get(metric_name)
            if not isinstance(raw_delta, (int, float)):
                raise ValueError(
                    f"proposal.deltas_vs_current_baseline[{package_name}].{metric_name} must be numeric"
                )
            delta = float(raw_delta)
            if abs(delta) > threshold_percent:
                flags.append(f"{metric_name}={delta:.3f}%")
        if flags:
            flagged_packages[package_name] = flags
    return flagged_packages


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--proposal", required=True)
    parser.add_argument("--review", required=True)
    parser.add_argument("--json-output", default="")
    args = parser.parse_args()

    try:
        baseline = _load_json(Path(args.baseline))
        proposal = _load_json(Path(args.proposal))
        review = _load_json(Path(args.review))

        _validate_packages_shape(baseline, "baseline")
        _validate_baseline_metadata(baseline)
        _validate_packages_shape(proposal, "proposal")

        proposal_prev = _parse_tag(proposal.get("previous_release"), "proposal.previous_release")
        proposal_next = _parse_tag(proposal.get("next_release"), "proposal.next_release")

        baseline_prev_raw = baseline.get("previous_release")
        if baseline_prev_raw is not None:
            baseline_prev = _parse_tag(baseline_prev_raw, "baseline.previous_release")
            if proposal_prev != baseline_prev:
                raise ValueError("proposal.previous_release must match baseline.previous_release")
        if proposal_next <= proposal_prev:
            raise ValueError("proposal.next_release must be greater than proposal.previous_release")

        baseline_pkg_names = set(baseline["packages"].keys())
        proposal_pkg_names = set(proposal["packages"].keys())
        if baseline_pkg_names != proposal_pkg_names:
            missing_in_proposal = sorted(baseline_pkg_names - proposal_pkg_names)
            missing_in_baseline = sorted(proposal_pkg_names - baseline_pkg_names)
            raise ValueError(
                "package set mismatch between baseline and proposal "
                f"(missing_in_proposal={missing_in_proposal}, missing_in_baseline={missing_in_baseline})"
            )

        deltas = proposal.get("deltas_vs_current_baseline")
        if not isinstance(deltas, dict):
            raise ValueError("proposal.deltas_vs_current_baseline must be an object")
        if set(deltas.keys()) != proposal_pkg_names:
            raise ValueError(
                "proposal.deltas_vs_current_baseline must contain exactly one entry per proposal package"
            )

        threshold = review.get("threshold_percent")
        if not isinstance(threshold, (int, float)) or float(threshold) <= 0:
            raise ValueError("review.threshold_percent must be a positive number")
        threshold_percent = float(threshold)

        review_flagged = review.get("flagged_packages")
        if not isinstance(review_flagged, dict):
            raise ValueError("review.flagged_packages must be an object")

        computed_flagged = _compute_flagged(deltas, threshold_percent)
        if review_flagged != computed_flagged:
            raise ValueError("review.flagged_packages is inconsistent with proposal deltas")

        expected_manual_review = bool(computed_flagged)
        if review.get("flagged_for_manual_review") is not expected_manual_review:
            raise ValueError(
                "review.flagged_for_manual_review is inconsistent with review.flagged_packages"
            )

        if review.get("previous_release") != proposal.get("previous_release"):
            raise ValueError("review.previous_release must match proposal.previous_release")
        if review.get("next_release") != proposal.get("next_release"):
            raise ValueError("review.next_release must match proposal.next_release")
        if review.get("review_schema_version") != "2026.1":
            raise ValueError("review.review_schema_version must equal '2026.1'")

        proposal_sha256 = review.get("proposal_sha256")
        if not isinstance(proposal_sha256, str) or len(proposal_sha256) != 64:
            raise ValueError("review.proposal_sha256 must be a 64-char hex string")
        expected_sha256 = _canonical_sha256(proposal)
        if proposal_sha256 != expected_sha256:
            raise ValueError("review.proposal_sha256 is inconsistent with proposal payload")

    except Exception as exc:
        return _fail(str(exc))

    report = {
        "status": "ok",
        "previous_release": proposal.get("previous_release"),
        "next_release": proposal.get("next_release"),
        "threshold_percent": threshold_percent,
        "package_count": len(proposal_pkg_names),
        "flagged_packages_count": len(computed_flagged),
        "proposal_sha256": expected_sha256,
    }

    if args.json_output:
        output_path = Path(args.json_output)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(report, indent=2), encoding="utf-8")

    print("Package benchmark baseline governance validation passed.")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
