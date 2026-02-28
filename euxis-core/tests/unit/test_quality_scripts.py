# SPDX-License-Identifier: AGPL-3.0-or-later

from __future__ import annotations

import json
import subprocess
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]


def _run(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=REPO_ROOT, text=True, capture_output=True, check=False)


def test_perf_budget_with_policy_current_stage() -> None:
    metrics_file = REPO_ROOT / "euxis-runtime" / "data" / "perf" / "metrics.jsonl"
    policy_file = REPO_ROOT / "scripts" / "perf" / "perf_policy.json"
    result = _run(
        [
            "python3",
            "scripts/perf/check_perf_budget.py",
            str(metrics_file),
            str(policy_file),
            "current",
        ]
    )
    assert result.returncode == 0, result.stderr
    assert "Performance gate passed." in result.stdout


def test_perf_budget_with_policy_strict_stage_fails() -> None:
    metrics_file = REPO_ROOT / "euxis-runtime" / "data" / "perf" / "metrics.jsonl"
    policy_file = REPO_ROOT / "scripts" / "perf" / "perf_policy.json"
    result = _run(
        [
            "python3",
            "scripts/perf/check_perf_budget.py",
            str(metrics_file),
            str(policy_file),
            "target_q4_2026",
        ]
    )
    assert result.returncode != 0
    assert "Performance gate failed." in result.stderr


def test_architecture_checker_detects_new_violation() -> None:
    violating_file = (
        REPO_ROOT / "euxis-core" / "src" / "euxis_core" / "contracts" / "_tmp_violation.py"
    )
    violating_file.write_text("import subprocess\n", encoding="utf-8")
    try:
        result = _run(["python3", "scripts/architecture/check_boundaries.py"])
        assert result.returncode != 0
        assert "IO/network import 'subprocess'" in result.stderr
    finally:
        violating_file.unlink(missing_ok=True)


def test_release_checklist_generator_passes_current_baseline() -> None:
    result = _run(
        [
            "python3",
            "scripts/release/generate_checklist.py",
            "--metrics",
            "euxis-runtime/data/perf/metrics.jsonl",
            "--policy",
            "scripts/perf/perf_policy.json",
            "--stage",
            "current",
            "--baseline",
            "scripts/perf/release_baseline.json",
            "--threshold-percent",
            "10",
            "--output",
            "data/release/checklist-test.md",
            "--json-output",
            "data/release/checklist-test.json",
        ]
    )
    assert result.returncode == 0, result.stderr


def test_release_checklist_generator_fails_on_strict_budget() -> None:
    result = _run(
        [
            "python3",
            "scripts/release/generate_checklist.py",
            "--metrics",
            "euxis-runtime/data/perf/metrics.jsonl",
            "--policy",
            "scripts/perf/perf_policy.json",
            "--stage",
            "target_q4_2026",
            "--baseline",
            "scripts/perf/release_baseline.json",
            "--threshold-percent",
            "10",
            "--output",
            "data/release/checklist-test-strict.md",
            "--json-output",
            "data/release/checklist-test-strict.json",
        ]
    )
    assert result.returncode != 0


def test_propose_release_baseline_generates_expected_value() -> None:
    result = _run(
        [
            "python3",
            "scripts/perf/propose_release_baseline.py",
            "--metrics",
            "euxis-runtime/data/perf/metrics.jsonl",
            "--previous-release",
            "v0.0.2",
            "--next-release",
            "v0.0.3",
            "--output",
            "data/release/proposed-baseline-test.json",
        ]
    )
    assert result.returncode == 0, result.stderr
    out_path = REPO_ROOT / "data" / "release" / "proposed-baseline-test.json"
    content = out_path.read_text(encoding="utf-8")
    assert '"proposed_previous_release_p95_ms": 131643.0' in content


def test_render_trend_summary_outputs_markdown() -> None:
    report_json = REPO_ROOT / "data" / "release" / "trend-summary-input.json"
    report_json.parent.mkdir(parents=True, exist_ok=True)
    report_json.write_text(
        '{"current_p95_ms":131643.0,"budget_ms":90000.0,"baseline_p95_ms":131643.0,"delta_ms":0.0,"delta_percent":0.0}',
        encoding="utf-8",
    )
    result = _run(
        [
            "python3",
            "scripts/perf/render_trend_summary.py",
            "--report",
            "data/release/trend-summary-input.json",
            "--status-code",
            "1",
            "--output",
            "data/release/trend-summary-output.md",
        ]
    )
    assert result.returncode == 0, result.stderr
    summary = (REPO_ROOT / "data" / "release" / "trend-summary-output.md").read_text(
        encoding="utf-8"
    )
    assert "WARNING" in summary


def test_perf_governance_validator_passes_defaults() -> None:
    result = _run(
        [
            "python3",
            "scripts/perf/validate_perf_governance.py",
            "--policy",
            "scripts/perf/perf_policy.json",
            "--baseline",
            "scripts/perf/release_baseline.json",
        ]
    )
    assert result.returncode == 0, result.stderr
    assert "Performance governance validation passed." in result.stdout


def test_perf_governance_validator_fails_non_monotonic_policy() -> None:
    tmp_policy = REPO_ROOT / "data" / "release" / "tmp-bad-policy.json"
    tmp_policy.parent.mkdir(parents=True, exist_ok=True)
    bad_policy = {
        "policy_version": "2026-02-28",
        "stages": {
            "current": {"p95_budget_ms": 100000},
            "target_q2_2026": {"p95_budget_ms": 120000},
            "target_q4_2026": {"p95_budget_ms": 90000},
        },
    }
    tmp_policy.write_text(json.dumps(bad_policy), encoding="utf-8")
    result = _run(
        [
            "python3",
            "scripts/perf/validate_perf_governance.py",
            "--policy",
            "data/release/tmp-bad-policy.json",
            "--baseline",
            "scripts/perf/release_baseline.json",
        ]
    )
    assert result.returncode != 0
    assert "monotonic" in result.stderr


def test_perf_governance_validator_fails_invalid_baseline_tag() -> None:
    tmp_baseline = REPO_ROOT / "data" / "release" / "tmp-bad-baseline.json"
    tmp_baseline.parent.mkdir(parents=True, exist_ok=True)
    bad_baseline = {"previous_release": "0.0.2", "previous_release_p95_ms": 1000}
    tmp_baseline.write_text(json.dumps(bad_baseline), encoding="utf-8")
    result = _run(
        [
            "python3",
            "scripts/perf/validate_perf_governance.py",
            "--policy",
            "scripts/perf/perf_policy.json",
            "--baseline",
            "data/release/tmp-bad-baseline.json",
        ]
    )
    assert result.returncode != 0
    assert "vX.Y.Z" in result.stderr


def test_baseline_proposal_review_flags_large_delta() -> None:
    baseline = REPO_ROOT / "data" / "release" / "tmp-baseline-review-baseline.json"
    proposal = REPO_ROOT / "data" / "release" / "tmp-baseline-review-proposal.json"
    baseline.parent.mkdir(parents=True, exist_ok=True)
    baseline.write_text(
        json.dumps({"previous_release": "v0.0.2", "previous_release_p95_ms": 100000}),
        encoding="utf-8",
    )
    proposal.write_text(
        json.dumps(
            {
                "previous_release": "v0.0.2",
                "next_release": "v0.0.3",
                "proposed_previous_release_p95_ms": 140000,
            }
        ),
        encoding="utf-8",
    )
    result = _run(
        [
            "python3",
            "scripts/release/review_baseline_proposal.py",
            "--baseline",
            "data/release/tmp-baseline-review-baseline.json",
            "--proposal",
            "data/release/tmp-baseline-review-proposal.json",
            "--threshold-percent",
            "20",
            "--output",
            "data/release/tmp-baseline-review.json",
            "--markdown-output",
            "data/release/tmp-baseline-review.md",
        ]
    )
    assert result.returncode == 0
    review = json.loads((REPO_ROOT / "data/release/tmp-baseline-review.json").read_text())
    assert review["flagged_for_manual_review"] is True


def test_release_evidence_aggregator_generates_bundle() -> None:
    scorecard = REPO_ROOT / "data/release/tmp-scorecard.json"
    checklist = REPO_ROOT / "data/release/tmp-checklist.json"
    proposal = REPO_ROOT / "data/release/tmp-proposal.json"
    review = REPO_ROOT / "data/release/tmp-review.json"
    signature = REPO_ROOT / "data/release/tmp-signature.json"
    scorecard.write_text(
        json.dumps(
            {
                "overall": 9.2,
                "release_gate_pass": True,
                "categories": {
                    "portability": 9.0,
                    "usability": 9.0,
                    "functionality": 9.0,
                    "security": 9.0,
                    "speed": 9.0,
                    "cost": 9.0,
                    "design": 9.0,
                    "accuracy": 9.0,
                },
            }
        ),
        encoding="utf-8",
    )
    checklist.write_text(json.dumps({"release_ok": True}), encoding="utf-8")
    proposal.write_text(json.dumps({"proposed_previous_release_p95_ms": 1000}), encoding="utf-8")
    review.write_text(json.dumps({"flagged_for_manual_review": False}), encoding="utf-8")
    signature.write_text(json.dumps({"status": "verified"}), encoding="utf-8")

    result = _run(
        [
            "python3",
            "scripts/release/aggregate_release_evidence.py",
            "--scorecard",
            "data/release/tmp-scorecard.json",
            "--checklist",
            "data/release/tmp-checklist.json",
            "--proposal",
            "data/release/tmp-proposal.json",
            "--baseline-review",
            "data/release/tmp-review.json",
            "--signature-status",
            "data/release/tmp-signature.json",
            "--output",
            "data/release/tmp-evidence.json",
            "--markdown-output",
            "data/release/tmp-evidence.md",
        ]
    )
    assert result.returncode == 0, result.stderr
    bundle = json.loads((REPO_ROOT / "data/release/tmp-evidence.json").read_text())
    assert bundle["signature_verification"]["status"] == "verified"


def test_release_evidence_validator_passes_bundle() -> None:
    result = _run(
        [
            "python3",
            "scripts/release/validate_release_evidence.py",
            "--evidence",
            "data/release/tmp-evidence.json",
            "--min-overall",
            "9.0",
            "--min-category",
            "8.0",
            "--require-signature-verified",
        ]
    )
    assert result.returncode == 0, result.stderr


def test_release_evidence_validator_fails_on_unknown_signature_when_required() -> None:
    unknown_sig_evidence = REPO_ROOT / "data/release/tmp-evidence-unknown-signature.json"
    unknown_sig_evidence.write_text(
        json.dumps(
            {
                "scorecard": {
                    "overall": 9.5,
                    "categories": {
                        "portability": 9.0,
                        "usability": 9.0,
                        "functionality": 9.0,
                        "security": 9.0,
                        "speed": 9.0,
                        "cost": 9.0,
                        "design": 9.0,
                        "accuracy": 9.0,
                    },
                },
                "release_checklist": {"release_ok": True},
                "baseline_review": {"flagged_for_manual_review": False},
                "signature_verification": {"status": "unknown"},
            }
        ),
        encoding="utf-8",
    )
    result = _run(
        [
            "python3",
            "scripts/release/validate_release_evidence.py",
            "--evidence",
            "data/release/tmp-evidence-unknown-signature.json",
            "--require-signature-verified",
        ]
    )
    assert result.returncode != 0
    assert "expected 'verified'" in result.stderr


def test_phase_completion_validator_passes_repo() -> None:
    result = _run(
        [
            "python3",
            "scripts/release/validate_phase_completion.py",
            "--repo-root",
            ".",
            "--json-output",
            "data/release/phase-completion-test.json",
        ]
    )
    assert result.returncode == 0, result.stderr
    payload = json.loads((REPO_ROOT / "data/release/phase-completion-test.json").read_text())
    assert payload["status"] == "ok"


def test_phase_completion_validator_fails_missing_files() -> None:
    empty_root = REPO_ROOT / "data/release/phase-completion-empty-root"
    empty_root.mkdir(parents=True, exist_ok=True)
    result = _run(
        [
            "python3",
            "scripts/release/validate_phase_completion.py",
            "--repo-root",
            "data/release/phase-completion-empty-root",
            "--json-output",
            "data/release/phase-completion-empty.json",
        ]
    )
    assert result.returncode != 0
    payload = json.loads((REPO_ROOT / "data/release/phase-completion-empty.json").read_text())
    assert payload["status"] == "failed"


def test_docs_coverage_validator_passes_repo() -> None:
    result = _run(
        [
            "python3",
            "scripts/quality/validate_phase_docs_coverage.py",
            "--repo-root",
            ".",
            "--json-output",
            "data/release/docs-coverage-test.json",
        ]
    )
    assert result.returncode == 0, result.stderr
    payload = json.loads((REPO_ROOT / "data/release/docs-coverage-test.json").read_text())
    assert payload["coverage_percent"] == 100.0


def test_docs_coverage_validator_fails_empty_repo() -> None:
    empty_root = REPO_ROOT / "data/release/docs-coverage-empty-root"
    empty_root.mkdir(parents=True, exist_ok=True)
    result = _run(
        [
            "python3",
            "scripts/quality/validate_phase_docs_coverage.py",
            "--repo-root",
            "data/release/docs-coverage-empty-root",
            "--json-output",
            "data/release/docs-coverage-empty.json",
        ]
    )
    assert result.returncode != 0
    payload = json.loads((empty_root / "data/release/docs-coverage-empty.json").read_text())
    assert payload["status"] == "failed"


def test_package_resource_governance_validator_passes_repo() -> None:
    result = _run(
        [
            "python3",
            "scripts/perf/validate_package_resource_governance.py",
            "--standards",
            "scripts/quality/package_standards.json",
            "--policy",
            "scripts/perf/package_resource_policy.json",
        ]
    )
    assert result.returncode == 0, result.stderr


def test_package_resource_governance_validator_fails_missing_package_policy() -> None:
    bad_policy = REPO_ROOT / "data/release/tmp-bad-package-policy.json"
    policy = json.loads((REPO_ROOT / "scripts/perf/package_resource_policy.json").read_text())
    policy["packages"].pop("euxis-core", None)
    bad_policy.parent.mkdir(parents=True, exist_ok=True)
    bad_policy.write_text(json.dumps(policy), encoding="utf-8")
    result = _run(
        [
            "python3",
            "scripts/perf/validate_package_resource_governance.py",
            "--standards",
            "scripts/quality/package_standards.json",
            "--policy",
            "data/release/tmp-bad-package-policy.json",
        ]
    )
    assert result.returncode != 0
    assert "missing package policy" in result.stderr


def test_package_excellence_validator_passes_repo() -> None:
    result = _run(
        [
            "python3",
            "scripts/quality/validate_package_excellence.py",
            "--repo-root",
            ".",
            "--standards",
            "scripts/quality/package_standards.json",
            "--resource-policy",
            "scripts/perf/package_resource_policy.json",
            "--json-output",
            "data/release/package-excellence-test.json",
        ]
    )
    assert result.returncode == 0, result.stderr
    payload = json.loads((REPO_ROOT / "data/release/package-excellence-test.json").read_text())
    assert payload["status"] == "ok"


def test_package_excellence_scorecard_generates_output() -> None:
    result = _run(
        [
            "python3",
            "scripts/eval/package_excellence_scorecard.py",
            "--standards",
            "scripts/quality/package_standards.json",
            "--resource-policy",
            "scripts/perf/package_resource_policy.json",
            "--output",
            "data/scorecard/package-excellence-test.json",
        ]
    )
    assert result.returncode == 0, result.stderr
    payload = json.loads((REPO_ROOT / "data/scorecard/package-excellence-test.json").read_text())
    assert "overall" in payload
    assert "packages" in payload


def test_package_benchmark_collector_generates_dataset() -> None:
    result = _run(
        [
            "python3",
            "scripts/perf/collect_package_benchmarks.py",
            "--repo-root",
            ".",
            "--standards",
            "scripts/quality/package_standards.json",
            "--iterations",
            "1",
            "--output",
            "data/perf/package-benchmarks-test.json",
        ]
    )
    assert result.returncode == 0, result.stderr
    payload = json.loads((REPO_ROOT / "data/perf/package-benchmarks-test.json").read_text())
    assert payload["status"] == "ok"
    assert "euxis-core" in payload["packages"]


def test_package_benchmark_budget_validator_passes_collected_dataset() -> None:
    result = _run(
        [
            "python3",
            "scripts/perf/validate_package_benchmark_budget.py",
            "--benchmarks",
            "data/perf/package-benchmarks-test.json",
            "--policy",
            "scripts/perf/package_resource_policy.json",
            "--stage",
            "current",
        ]
    )
    assert result.returncode == 0, result.stderr


def test_package_benchmark_trend_renderer_outputs_markdown() -> None:
    result = _run(
        [
            "python3",
            "scripts/perf/render_package_benchmark_trend.py",
            "--current",
            "data/perf/package-benchmarks-test.json",
            "--baseline",
            "scripts/perf/package_benchmarks_baseline.json",
            "--threshold-percent",
            "10",
            "--output",
            "data/perf/package-benchmark-trend-test.md",
        ]
    )
    assert result.returncode == 0, result.stderr
    content = (REPO_ROOT / "data/perf/package-benchmark-trend-test.md").read_text(encoding="utf-8")
    assert "Package Benchmark Trend" in content


def test_package_benchmark_regression_validator_passes() -> None:
    result = _run(
        [
            "python3",
            "scripts/perf/validate_package_benchmark_regression.py",
            "--current",
            "data/perf/package-benchmarks-test.json",
            "--baseline",
            "scripts/perf/package_benchmarks_baseline.json",
            "--threshold-percent",
            "200",
            "--summary-output",
            "data/perf/package-benchmark-regression-test.md",
        ]
    )
    assert result.returncode == 0, result.stderr


def test_package_benchmark_regression_validator_warn_only() -> None:
    result = _run(
        [
            "python3",
            "scripts/perf/validate_package_benchmark_regression.py",
            "--current",
            "data/perf/package-benchmarks-test.json",
            "--baseline",
            "scripts/perf/package_benchmarks_baseline.json",
            "--threshold-percent",
            "0.0001",
            "--warn-only",
            "--summary-output",
            "data/perf/package-benchmark-regression-warn-test.md",
        ]
    )
    assert result.returncode == 0, result.stderr


def test_package_benchmark_baseline_proposal_generates_output() -> None:
    result = _run(
        [
            "python3",
            "scripts/perf/propose_package_benchmark_baseline.py",
            "--current",
            "data/perf/package-benchmarks-test.json",
            "--baseline",
            "scripts/perf/package_benchmarks_baseline.json",
            "--previous-release",
            "v0.0.2",
            "--next-release",
            "v0.0.3",
            "--output",
            "data/perf/proposed-package-benchmarks-baseline-test.json",
        ]
    )
    assert result.returncode == 0, result.stderr
    payload = json.loads(
        (REPO_ROOT / "data/perf/proposed-package-benchmarks-baseline-test.json").read_text()
    )
    assert payload["previous_release"] == "v0.0.2"
    assert "deltas_vs_current_baseline" in payload


def test_package_benchmark_baseline_review_runs() -> None:
    result = _run(
        [
            "python3",
            "scripts/release/review_package_benchmark_baseline.py",
            "--proposal",
            "data/perf/proposed-package-benchmarks-baseline-test.json",
            "--threshold-percent",
            "25",
            "--output",
            "data/perf/package-benchmark-baseline-review-test.json",
            "--markdown-output",
            "data/perf/package-benchmark-baseline-review-test.md",
        ]
    )
    assert result.returncode == 0, result.stderr
    payload = json.loads(
        (REPO_ROOT / "data/perf/package-benchmark-baseline-review-test.json").read_text()
    )
    assert "flagged_for_manual_review" in payload
    assert payload["review_schema_version"] == "2026.1"
    assert isinstance(payload["proposal_sha256"], str)
    assert len(payload["proposal_sha256"]) == 64


def test_package_benchmark_baseline_governance_validator_passes() -> None:
    result = _run(
        [
            "python3",
            "scripts/perf/validate_package_benchmark_baseline_governance.py",
            "--baseline",
            "scripts/perf/package_benchmarks_baseline.json",
            "--proposal",
            "data/perf/proposed-package-benchmarks-baseline-test.json",
            "--review",
            "data/perf/package-benchmark-baseline-review-test.json",
            "--json-output",
            "data/perf/package-benchmark-baseline-governance-test.json",
        ]
    )
    assert result.returncode == 0, result.stderr
    payload = json.loads(
        (REPO_ROOT / "data/perf/package-benchmark-baseline-governance-test.json").read_text()
    )
    assert payload["status"] == "ok"


def test_package_benchmark_baseline_governance_validator_fails_on_tampered_review() -> None:
    tampered = REPO_ROOT / "data/perf/package-benchmark-baseline-review-tampered.json"
    review = json.loads(
        (REPO_ROOT / "data/perf/package-benchmark-baseline-review-test.json").read_text()
    )
    review["flagged_for_manual_review"] = not bool(review.get("flagged_for_manual_review"))
    tampered.parent.mkdir(parents=True, exist_ok=True)
    tampered.write_text(json.dumps(review), encoding="utf-8")

    result = _run(
        [
            "python3",
            "scripts/perf/validate_package_benchmark_baseline_governance.py",
            "--baseline",
            "scripts/perf/package_benchmarks_baseline.json",
            "--proposal",
            "data/perf/proposed-package-benchmarks-baseline-test.json",
            "--review",
            "data/perf/package-benchmark-baseline-review-tampered.json",
        ]
    )
    assert result.returncode != 0
    assert "inconsistent" in result.stderr


def test_package_benchmark_baseline_governance_validator_fails_on_review_hash_mismatch() -> None:
    tampered = REPO_ROOT / "data/perf/package-benchmark-baseline-review-hash-tampered.json"
    review = json.loads(
        (REPO_ROOT / "data/perf/package-benchmark-baseline-review-test.json").read_text()
    )
    review["proposal_sha256"] = "0" * 64
    tampered.parent.mkdir(parents=True, exist_ok=True)
    tampered.write_text(json.dumps(review), encoding="utf-8")

    result = _run(
        [
            "python3",
            "scripts/perf/validate_package_benchmark_baseline_governance.py",
            "--baseline",
            "scripts/perf/package_benchmarks_baseline.json",
            "--proposal",
            "data/perf/proposed-package-benchmarks-baseline-test.json",
            "--review",
            "data/perf/package-benchmark-baseline-review-hash-tampered.json",
        ]
    )
    assert result.returncode != 0
    assert "proposal_sha256" in result.stderr


def test_package_structure_matrix_generator_writes_markdown() -> None:
    out = REPO_ROOT / "data/release/package-structure-matrix-test.md"
    result = _run(
        [
            "python3",
            "scripts/quality/render_package_structure_matrix.py",
            "--repo-root",
            ".",
            "--standards",
            "scripts/quality/package_standards.json",
            "--output",
            "data/release/package-structure-matrix-test.md",
        ]
    )
    assert result.returncode == 0, result.stderr
    content = out.read_text(encoding="utf-8")
    assert "Package Structure Matrix" in content
    assert "`euxis-core`" in content


def test_package_structure_matrix_check_fails_when_stale() -> None:
    stale = REPO_ROOT / "data/release/package-structure-matrix-stale.md"
    stale.parent.mkdir(parents=True, exist_ok=True)
    stale.write_text("# stale\n", encoding="utf-8")

    result = _run(
        [
            "python3",
            "scripts/quality/render_package_structure_matrix.py",
            "--repo-root",
            ".",
            "--standards",
            "scripts/quality/package_standards.json",
            "--output",
            "data/release/package-structure-matrix-stale.md",
            "--check",
        ]
    )
    assert result.returncode != 0
    assert "stale" in result.stderr


def test_package_structure_matrix_staleness_report_warn_only_passes() -> None:
    result = _run(
        [
            "python3",
            "scripts/quality/report_package_structure_matrix_staleness.py",
            "--repo-root",
            ".",
            "--standards",
            "scripts/quality/package_standards.json",
            "--matrix",
            "euxis-docs/docs/architecture/package-structure-matrix.md",
            "--summary-output",
            "data/release/package-structure-matrix-staleness-test.md",
            "--diff-output",
            "data/release/package-structure-matrix-staleness-test.diff",
            "--warn-only",
        ]
    )
    assert result.returncode == 0, result.stderr
    summary = (REPO_ROOT / "data/release/package-structure-matrix-staleness-test.md").read_text()
    assert "up-to-date" in summary


def test_package_structure_matrix_staleness_report_fails_when_stale() -> None:
    stale = REPO_ROOT / "data/release/package-structure-matrix-stale-report.md"
    stale.parent.mkdir(parents=True, exist_ok=True)
    stale.write_text("# stale\n", encoding="utf-8")

    result = _run(
        [
            "python3",
            "scripts/quality/report_package_structure_matrix_staleness.py",
            "--repo-root",
            ".",
            "--standards",
            "scripts/quality/package_standards.json",
            "--matrix",
            "data/release/package-structure-matrix-stale-report.md",
            "--summary-output",
            "data/release/package-structure-matrix-stale-report-summary.md",
            "--diff-output",
            "data/release/package-structure-matrix-stale-report.diff",
        ]
    )
    assert result.returncode != 0
    summary = (
        REPO_ROOT / "data/release/package-structure-matrix-stale-report-summary.md"
    ).read_text()
    diff = (REPO_ROOT / "data/release/package-structure-matrix-stale-report.diff").read_text()
    assert "stale" in summary
    assert "--- data/release/package-structure-matrix-stale-report.md" in diff
