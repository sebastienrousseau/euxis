.PHONY: all test lint format clean install dev architecture-check perf-gate scorecard gate-all verify-signed-artifacts release-checklist propose-release-baseline perf-governance-check baseline-proposal-review release-evidence validate-release-evidence validate-release-evidence-strict phase-completion-check code-coverage-100 docs-coverage-100 workspace-topology-check package-resource-governance-check package-excellence-check package-excellence-scorecard package-harmony-check package-bench-collect package-bench-gate package-bench-regression-gate package-bench-baseline-propose package-bench-baseline-review package-bench-baseline-governance-check package-structure-matrix package-structure-matrix-check package-structure-matrix-report template-overlay-apply template-conformance-check package-structure-enforce split-readiness-report workspace-bootstrap gateway-tests-stable core-tests-stable cli-tests-stable metrics-tests-stable adapters-tests-stable security-tests-stable runtime-tests-stable scripts-tests-stable docs-tests-stable sdk-rust-tests-stable crypto-packages-tests-stable tui-tests-stable crypto-lib-tests-stable verify-all-packages

all: install test

test:
	pytest

test-cov:
	pytest --cov=euxis-core/src --cov-report=term-missing

lint:
	ruff check .

format:
	ruff format .

clean:
	find . -type d -name "__pycache__" -exec rm -rf {} +
	find . -type d -name ".pytest_cache" -exec rm -rf {} +
	find . -type d -name ".ruff_cache" -exec rm -rf {} +

install:
	pip install -e ".[dev]"

architecture-check:
	python3 euxis-ops/architecture/check_boundaries.py

perf-gate:
	python3 euxis-ops/perf/check_perf_budget.py euxis-runtime/data/perf/metrics.jsonl euxis-ops/perf/perf_policy.json current

perf-governance-check:
	python3 euxis-ops/perf/validate_perf_governance.py \
		--policy euxis-ops/perf/perf_policy.json \
		--baseline euxis-ops/perf/release_baseline.json

scorecard:
	python3 euxis-ops/eval/scorecard.py euxis-data/scorecard/metrics.sample.json euxis-data/scorecard/latest.json

release-checklist:
	python3 euxis-ops/release/generate_checklist.py \
		--metrics euxis-runtime/data/perf/metrics.jsonl \
		--policy euxis-ops/perf/perf_policy.json \
		--stage current \
		--baseline euxis-ops/perf/release_baseline.json \
		--threshold-percent 10 \
		--output euxis-data/release/checklist.md \
		--json-output euxis-data/release/checklist.json

propose-release-baseline:
	python3 euxis-ops/perf/propose_release_baseline.py \
		--metrics euxis-runtime/data/perf/metrics.jsonl \
		--previous-release v0.0.2 \
		--next-release v0.0.3 \
		--output euxis-data/release/proposed-baseline.json

baseline-proposal-review:
	python3 euxis-ops/release/review_baseline_proposal.py \
		--baseline euxis-ops/perf/release_baseline.json \
		--proposal euxis-data/release/proposed-baseline.json \
		--threshold-percent 20 \
		--output euxis-data/release/baseline-review.json \
		--markdown-output euxis-data/release/baseline-review.md

release-evidence:
	python3 euxis-ops/release/aggregate_release_evidence.py \
		--scorecard euxis-data/scorecard/latest.json \
		--checklist euxis-data/release/checklist.json \
		--proposal euxis-data/release/proposed-baseline.json \
		--baseline-review euxis-data/release/baseline-review.json \
		--signature-status euxis-data/release/signature-verification.json \
		--output euxis-data/release/release-evidence.json \
		--markdown-output euxis-data/release/release-evidence.md

validate-release-evidence:
	python3 euxis-ops/release/validate_release_evidence.py \
		--evidence euxis-data/release/release-evidence.json \
		--min-overall 9.0 \
		--min-category 8.0

validate-release-evidence-strict:
	python3 euxis-ops/release/validate_release_evidence.py \
		--evidence euxis-data/release/release-evidence.json \
		--min-overall 9.0 \
		--min-category 8.0 \
		--require-signature-verified

phase-completion-check:
	python3 euxis-ops/release/validate_phase_completion.py \
		--repo-root . \
		--json-output euxis-data/release/phase-completion.json

code-coverage-100:
	python3 euxis-ops/quality/enforce_phase_code_coverage.py

docs-coverage-100:
	python3 euxis-ops/quality/validate_phase_docs_coverage.py \
		--repo-root . \
		--json-output euxis-data/release/docs-coverage.json

workspace-topology-check:
	python3 euxis-ops/quality/validate_workspace_topology.py \
		--repo-root . \
		--policy euxis-ops/quality/workspace_topology_policy.json \
		--json-output euxis-data/release/workspace-topology.json

package-resource-governance-check:
	python3 euxis-ops/perf/validate_package_resource_governance.py \
		--standards euxis-ops/quality/package_standards.json \
		--policy euxis-ops/perf/package_resource_policy.json

package-excellence-check:
	python3 euxis-ops/quality/validate_package_excellence.py \
		--repo-root . \
		--standards euxis-ops/quality/package_standards.json \
		--resource-policy euxis-ops/perf/package_resource_policy.json \
		--json-output euxis-data/release/package-excellence.json

package-excellence-scorecard:
	python3 euxis-ops/eval/package_excellence_scorecard.py \
		--standards euxis-ops/quality/package_standards.json \
		--resource-policy euxis-ops/perf/package_resource_policy.json \
		--output euxis-data/scorecard/package-excellence.json

package-harmony-check:
	python3 euxis-ops/quality/validate_package_harmony.py \
		--repo-root . \
		--standards euxis-ops/quality/package_standards.json \
		--baseline euxis-ops/quality/package_harmony_baseline.json \
		--json-output euxis-data/release/package-harmony.json

package-structure-matrix:
	python3 euxis-ops/quality/render_package_structure_matrix.py \
		--repo-root . \
		--standards euxis-ops/quality/package_standards.json \
		--output euxis-docs/docs/architecture/package-structure-matrix.md

package-structure-matrix-check:
	python3 euxis-ops/quality/render_package_structure_matrix.py \
		--repo-root . \
		--standards euxis-ops/quality/package_standards.json \
		--output euxis-docs/docs/architecture/package-structure-matrix.md \
		--check

package-structure-matrix-report:
	python3 euxis-ops/quality/report_package_structure_matrix_staleness.py \
		--repo-root . \
		--standards euxis-ops/quality/package_standards.json \
		--matrix euxis-docs/docs/architecture/package-structure-matrix.md \
		--summary-output euxis-data/release/package-structure-matrix-staleness.md \
		--diff-output euxis-data/release/package-structure-matrix-staleness.diff \
		--warn-only

template-overlay-apply:
	python3 euxis-ops/templates/apply_template_overlay.py \
		--repo-root . \
		--standards euxis-ops/quality/package_standards.json

template-conformance-check:
	python3 euxis-ops/quality/validate_template_conformance.py \
		--repo-root . \
		--standards euxis-ops/quality/package_standards.json \
		--json-output euxis-data/release/template-conformance.json

package-structure-enforce:
	python3 euxis-ops/templates/enforce_package_structure.py \
		--repo-root . \
		--standards euxis-ops/quality/package_standards.json \
		--json-output euxis-data/release/structure-enforcement.json

split-readiness-report:
	python3 euxis-ops/quality/generate_split_readiness_report.py \
		--repo-root . \
		--standards euxis-ops/quality/package_standards.json \
		--json-output euxis-data/release/repo-split-readiness.json \
		--md-output euxis-data/release/repo-split-readiness.md

package-bench-collect:
	python3 euxis-ops/perf/collect_package_benchmarks.py \
		--repo-root . \
		--standards euxis-ops/quality/package_standards.json \
		--iterations 3 \
		--output euxis-data/perf/package-benchmarks.json

package-bench-gate:
	python3 euxis-ops/perf/validate_package_benchmark_budget.py \
		--benchmarks euxis-data/perf/package-benchmarks.json \
		--policy euxis-ops/perf/package_resource_policy.json \
		--stage current

package-bench-regression-gate:
	python3 euxis-ops/perf/validate_package_benchmark_regression.py \
		--current euxis-data/perf/package-benchmarks.json \
		--baseline euxis-ops/perf/package_benchmarks_baseline.json \
		--threshold-percent 20 \
		--summary-output euxis-data/perf/package-benchmark-regression.md

package-bench-baseline-propose:
	python3 euxis-ops/perf/propose_package_benchmark_baseline.py \
		--current euxis-data/perf/package-benchmarks.json \
		--baseline euxis-ops/perf/package_benchmarks_baseline.json \
		--previous-release v0.0.2 \
		--next-release v0.0.3 \
		--output euxis-data/perf/proposed-package-benchmarks-baseline.json

package-bench-baseline-review:
	python3 euxis-ops/release/review_package_benchmark_baseline.py \
		--proposal euxis-data/perf/proposed-package-benchmarks-baseline.json \
		--threshold-percent 25 \
		--output euxis-data/perf/package-benchmark-baseline-review.json \
		--markdown-output euxis-data/perf/package-benchmark-baseline-review.md

package-bench-baseline-governance-check:
	python3 euxis-ops/perf/validate_package_benchmark_baseline_governance.py \
		--baseline euxis-ops/perf/package_benchmarks_baseline.json \
		--proposal euxis-data/perf/proposed-package-benchmarks-baseline.json \
		--review euxis-data/perf/package-benchmark-baseline-review.json \
		--json-output euxis-data/perf/package-benchmark-baseline-governance.json

gate-all: architecture-check perf-governance-check perf-gate scorecard release-checklist propose-release-baseline baseline-proposal-review release-evidence validate-release-evidence phase-completion-check code-coverage-100 docs-coverage-100 workspace-topology-check template-conformance-check package-resource-governance-check package-excellence-check package-excellence-scorecard package-harmony-check package-structure-matrix-check package-bench-collect package-bench-gate package-bench-baseline-propose package-bench-baseline-review package-bench-baseline-governance-check

workspace-bootstrap:
	bash euxis-ops/bootstrap/verify_workspace.sh

gateway-tests-stable:
	bash euxis-ops/quality/run_gateway_tests_stable.sh

core-tests-stable:
	bash euxis-ops/quality/run_core_tests_stable.sh

cli-tests-stable:
	bash euxis-ops/quality/run_cli_tests_stable.sh

metrics-tests-stable:
	bash euxis-ops/quality/run_metrics_tests_stable.sh

adapters-tests-stable:
	bash euxis-ops/quality/run_adapters_tests_stable.sh

security-tests-stable:
	bash euxis-ops/quality/run_security_tests_stable.sh

runtime-tests-stable:
	bash euxis-ops/quality/run_runtime_tests_stable.sh

scripts-tests-stable:
	bash euxis-ops/quality/run_scripts_tests_stable.sh

docs-tests-stable:
	bash euxis-ops/quality/run_docs_tests_stable.sh

sdk-rust-tests-stable:
	bash euxis-ops/quality/run_sdk_rust_tests_stable.sh

crypto-packages-tests-stable:
	bash euxis-ops/quality/run_crypto_packages_tests_stable.sh

tui-tests-stable:
	bash euxis-ops/quality/run_tui_tests_stable.sh

crypto-lib-tests-stable:
	bash euxis-ops/quality/run_crypto_lib_tests_stable.sh

verify-all-packages: gate-all gateway-tests-stable core-tests-stable cli-tests-stable metrics-tests-stable adapters-tests-stable security-tests-stable runtime-tests-stable scripts-tests-stable docs-tests-stable sdk-rust-tests-stable crypto-packages-tests-stable tui-tests-stable crypto-lib-tests-stable

verify-signed-artifacts:
	bash euxis-ops/supply_chain/verify_signed_artifacts.sh release-artifacts
