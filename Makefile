.PHONY: all test lint format clean install dev architecture-check perf-gate scorecard gate-all verify-signed-artifacts release-checklist propose-release-baseline perf-governance-check baseline-proposal-review release-evidence validate-release-evidence validate-release-evidence-strict phase-completion-check code-coverage-100 docs-coverage-100 package-resource-governance-check package-excellence-check package-excellence-scorecard package-harmony-check package-bench-collect package-bench-gate package-bench-regression-gate package-bench-baseline-propose package-bench-baseline-review package-bench-baseline-governance-check package-structure-matrix package-structure-matrix-check package-structure-matrix-report gateway-tests-stable core-tests-stable cli-tests-stable metrics-tests-stable adapters-tests-stable security-tests-stable runtime-tests-stable scripts-tests-stable docs-tests-stable sdk-rust-tests-stable crypto-packages-tests-stable tui-tests-stable crypto-lib-tests-stable verify-all-packages

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
	python3 scripts/architecture/check_boundaries.py

perf-gate:
	python3 scripts/perf/check_perf_budget.py euxis-runtime/data/perf/metrics.jsonl scripts/perf/perf_policy.json current

perf-governance-check:
	python3 scripts/perf/validate_perf_governance.py \
		--policy scripts/perf/perf_policy.json \
		--baseline scripts/perf/release_baseline.json

scorecard:
	python3 scripts/eval/scorecard.py data/scorecard/metrics.sample.json data/scorecard/latest.json

release-checklist:
	python3 scripts/release/generate_checklist.py \
		--metrics euxis-runtime/data/perf/metrics.jsonl \
		--policy scripts/perf/perf_policy.json \
		--stage current \
		--baseline scripts/perf/release_baseline.json \
		--threshold-percent 10 \
		--output data/release/checklist.md \
		--json-output data/release/checklist.json

propose-release-baseline:
	python3 scripts/perf/propose_release_baseline.py \
		--metrics euxis-runtime/data/perf/metrics.jsonl \
		--previous-release v0.0.2 \
		--next-release v0.0.3 \
		--output data/release/proposed-baseline.json

baseline-proposal-review:
	python3 scripts/release/review_baseline_proposal.py \
		--baseline scripts/perf/release_baseline.json \
		--proposal data/release/proposed-baseline.json \
		--threshold-percent 20 \
		--output data/release/baseline-review.json \
		--markdown-output data/release/baseline-review.md

release-evidence:
	python3 scripts/release/aggregate_release_evidence.py \
		--scorecard data/scorecard/latest.json \
		--checklist data/release/checklist.json \
		--proposal data/release/proposed-baseline.json \
		--baseline-review data/release/baseline-review.json \
		--signature-status data/release/signature-verification.json \
		--output data/release/release-evidence.json \
		--markdown-output data/release/release-evidence.md

validate-release-evidence:
	python3 scripts/release/validate_release_evidence.py \
		--evidence data/release/release-evidence.json \
		--min-overall 9.0 \
		--min-category 8.0

validate-release-evidence-strict:
	python3 scripts/release/validate_release_evidence.py \
		--evidence data/release/release-evidence.json \
		--min-overall 9.0 \
		--min-category 8.0 \
		--require-signature-verified

phase-completion-check:
	python3 scripts/release/validate_phase_completion.py \
		--repo-root . \
		--json-output data/release/phase-completion.json

code-coverage-100:
	python3 scripts/quality/enforce_phase_code_coverage.py

docs-coverage-100:
	python3 scripts/quality/validate_phase_docs_coverage.py \
		--repo-root . \
		--json-output data/release/docs-coverage.json

package-resource-governance-check:
	python3 scripts/perf/validate_package_resource_governance.py \
		--standards scripts/quality/package_standards.json \
		--policy scripts/perf/package_resource_policy.json

package-excellence-check:
	python3 scripts/quality/validate_package_excellence.py \
		--repo-root . \
		--standards scripts/quality/package_standards.json \
		--resource-policy scripts/perf/package_resource_policy.json \
		--json-output data/release/package-excellence.json

package-excellence-scorecard:
	python3 scripts/eval/package_excellence_scorecard.py \
		--standards scripts/quality/package_standards.json \
		--resource-policy scripts/perf/package_resource_policy.json \
		--output data/scorecard/package-excellence.json

package-harmony-check:
	python3 scripts/quality/validate_package_harmony.py \
		--repo-root . \
		--standards scripts/quality/package_standards.json \
		--baseline scripts/quality/package_harmony_baseline.json \
		--json-output data/release/package-harmony.json

package-structure-matrix:
	python3 scripts/quality/render_package_structure_matrix.py \
		--repo-root . \
		--standards scripts/quality/package_standards.json \
		--output euxis-docs/docs/architecture/package-structure-matrix.md

package-structure-matrix-check:
	python3 scripts/quality/render_package_structure_matrix.py \
		--repo-root . \
		--standards scripts/quality/package_standards.json \
		--output euxis-docs/docs/architecture/package-structure-matrix.md \
		--check

package-structure-matrix-report:
	python3 scripts/quality/report_package_structure_matrix_staleness.py \
		--repo-root . \
		--standards scripts/quality/package_standards.json \
		--matrix euxis-docs/docs/architecture/package-structure-matrix.md \
		--summary-output data/release/package-structure-matrix-staleness.md \
		--diff-output data/release/package-structure-matrix-staleness.diff \
		--warn-only

package-bench-collect:
	python3 scripts/perf/collect_package_benchmarks.py \
		--repo-root . \
		--standards scripts/quality/package_standards.json \
		--iterations 3 \
		--output data/perf/package-benchmarks.json

package-bench-gate:
	python3 scripts/perf/validate_package_benchmark_budget.py \
		--benchmarks data/perf/package-benchmarks.json \
		--policy scripts/perf/package_resource_policy.json \
		--stage current

package-bench-regression-gate:
	python3 scripts/perf/validate_package_benchmark_regression.py \
		--current data/perf/package-benchmarks.json \
		--baseline scripts/perf/package_benchmarks_baseline.json \
		--threshold-percent 20 \
		--summary-output data/perf/package-benchmark-regression.md

package-bench-baseline-propose:
	python3 scripts/perf/propose_package_benchmark_baseline.py \
		--current data/perf/package-benchmarks.json \
		--baseline scripts/perf/package_benchmarks_baseline.json \
		--previous-release v0.0.2 \
		--next-release v0.0.3 \
		--output data/perf/proposed-package-benchmarks-baseline.json

package-bench-baseline-review:
	python3 scripts/release/review_package_benchmark_baseline.py \
		--proposal data/perf/proposed-package-benchmarks-baseline.json \
		--threshold-percent 25 \
		--output data/perf/package-benchmark-baseline-review.json \
		--markdown-output data/perf/package-benchmark-baseline-review.md

package-bench-baseline-governance-check:
	python3 scripts/perf/validate_package_benchmark_baseline_governance.py \
		--baseline scripts/perf/package_benchmarks_baseline.json \
		--proposal data/perf/proposed-package-benchmarks-baseline.json \
		--review data/perf/package-benchmark-baseline-review.json \
		--json-output data/perf/package-benchmark-baseline-governance.json

gate-all: architecture-check perf-governance-check perf-gate scorecard release-checklist propose-release-baseline baseline-proposal-review release-evidence validate-release-evidence phase-completion-check code-coverage-100 docs-coverage-100 package-resource-governance-check package-excellence-check package-excellence-scorecard package-harmony-check package-structure-matrix-check package-bench-collect package-bench-gate package-bench-baseline-propose package-bench-baseline-review package-bench-baseline-governance-check

gateway-tests-stable:
	bash scripts/quality/run_gateway_tests_stable.sh

core-tests-stable:
	bash scripts/quality/run_core_tests_stable.sh

cli-tests-stable:
	bash scripts/quality/run_cli_tests_stable.sh

metrics-tests-stable:
	bash scripts/quality/run_metrics_tests_stable.sh

adapters-tests-stable:
	bash scripts/quality/run_adapters_tests_stable.sh

security-tests-stable:
	bash scripts/quality/run_security_tests_stable.sh

runtime-tests-stable:
	bash scripts/quality/run_runtime_tests_stable.sh

scripts-tests-stable:
	bash scripts/quality/run_scripts_tests_stable.sh

docs-tests-stable:
	bash scripts/quality/run_docs_tests_stable.sh

sdk-rust-tests-stable:
	bash scripts/quality/run_sdk_rust_tests_stable.sh

crypto-packages-tests-stable:
	bash scripts/quality/run_crypto_packages_tests_stable.sh

tui-tests-stable:
	bash scripts/quality/run_tui_tests_stable.sh

crypto-lib-tests-stable:
	bash scripts/quality/run_crypto_lib_tests_stable.sh

verify-all-packages: gate-all gateway-tests-stable core-tests-stable cli-tests-stable metrics-tests-stable adapters-tests-stable security-tests-stable runtime-tests-stable scripts-tests-stable docs-tests-stable sdk-rust-tests-stable crypto-packages-tests-stable tui-tests-stable crypto-lib-tests-stable

verify-signed-artifacts:
	bash scripts/supply_chain/verify_signed_artifacts.sh release-artifacts
