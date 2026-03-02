.PHONY: all test lint format clean install dev architecture-check core-platform-boundary-check perf-gate scorecard gate-all verify-signed-artifacts release-checklist propose-release-baseline perf-governance-check baseline-proposal-review release-evidence validate-release-evidence validate-release-evidence-strict phase-completion-check code-coverage-100 docs-coverage-100 workspace-topology-check package-resource-governance-check package-excellence-check package-excellence-scorecard package-harmony-check package-bench-collect package-bench-gate package-bench-regression-gate package-bench-baseline-propose package-bench-baseline-review package-bench-baseline-governance-check package-structure-matrix package-structure-matrix-check package-structure-matrix-report template-overlay-apply template-conformance-check package-structure-enforce split-readiness-report workspace-bootstrap sdk-rust-tests-stable bench-security bench-autonomy bench-performance bench-portability bench-interop bench-all verify-all-packages cpp-configure cpp-build cpp-test cpp-bench cpp-clean cpp-format cpp-format-check cpp-tidy cpp-coverage

# Conservative default for laptop thermals/memory; override per run as needed.
CPP_BUILD_JOBS ?= 4

all: install test

test:
	pytest

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

core-platform-boundary-check:
	python3 euxis-ops/quality/validate_core_platform_boundaries.py \
		--repo-root . \
		--standards euxis-ops/quality/package_standards.json \
		--json-output euxis-data/release/core-platform-boundaries.json

perf-gate:
	python3 euxis-ops/perf/check_perf_budget.py euxis-data/runtime/perf/metrics.jsonl euxis-ops/perf/perf_policy.json current

perf-governance-check:
	python3 euxis-ops/perf/validate_perf_governance.py \
		--policy euxis-ops/perf/perf_policy.json \
		--baseline euxis-ops/perf/release_baseline.json

scorecard:
	python3 euxis-ops/eval/scorecard.py euxis-data/scorecard/metrics.sample.json euxis-data/scorecard/latest.json

release-checklist:
	python3 euxis-ops/release/generate_checklist.py \
		--metrics euxis-data/runtime/perf/metrics.jsonl \
		--policy euxis-ops/perf/perf_policy.json \
		--stage current \
		--baseline euxis-ops/perf/release_baseline.json \
		--threshold-percent 10 \
		--output euxis-data/release/checklist.md \
		--json-output euxis-data/release/checklist.json

propose-release-baseline:
	python3 euxis-ops/perf/propose_release_baseline.py \
		--metrics euxis-data/runtime/perf/metrics.jsonl \
		--previous-release v0.0.3 \
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
		--output euxis-data/docs/architecture/package-structure-matrix.md

package-structure-matrix-check:
	python3 euxis-ops/quality/render_package_structure_matrix.py \
		--repo-root . \
		--standards euxis-ops/quality/package_standards.json \
		--output euxis-data/docs/architecture/package-structure-matrix.md \
		--check

package-structure-matrix-report:
	python3 euxis-ops/quality/report_package_structure_matrix_staleness.py \
		--repo-root . \
		--standards euxis-ops/quality/package_standards.json \
		--matrix euxis-data/docs/architecture/package-structure-matrix.md \
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
		--previous-release v0.0.3 \
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

gate-all: architecture-check core-platform-boundary-check perf-governance-check perf-gate scorecard release-checklist propose-release-baseline baseline-proposal-review release-evidence validate-release-evidence phase-completion-check code-coverage-100 docs-coverage-100 workspace-topology-check template-conformance-check package-resource-governance-check package-excellence-check package-excellence-scorecard package-harmony-check package-structure-matrix-check package-bench-collect package-bench-gate package-bench-baseline-propose package-bench-baseline-review package-bench-baseline-governance-check

workspace-bootstrap:
	bash euxis-ops/bootstrap/verify_workspace.sh

sdk-rust-tests-stable:
	bash euxis-ops/quality/run_sdk_rust_tests_stable.sh

bench-security:
	python3 euxis-ops/benchmarks/runner.py --suite security

bench-autonomy:
	python3 euxis-ops/benchmarks/runner.py --suite autonomy

bench-performance:
	python3 euxis-ops/benchmarks/runner.py --suite performance

bench-portability:
	python3 euxis-ops/benchmarks/runner.py --suite portability

bench-interop:
	python3 euxis-ops/benchmarks/runner.py --suite interop

bench-all:
	python3 euxis-ops/benchmarks/runner.py --suite all --output euxis-data/perf/benchmark-results.json

verify-all-packages: gate-all sdk-rust-tests-stable cpp-test

verify-signed-artifacts:
	bash euxis-ops/supply_chain/verify_signed_artifacts.sh release-artifacts

# C++23 targets
cpp-configure:
	cmake -B euxis-cpp/build -S euxis-cpp \
		-DCMAKE_TOOLCHAIN_FILE=$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake \
		-DCMAKE_BUILD_TYPE=Release \
		$(if $(shell command -v ccache 2>/dev/null),-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache,)

cpp-build: cpp-configure
	cmake --build euxis-cpp/build --parallel $(CPP_BUILD_JOBS)

cpp-test: cpp-build
	ctest --test-dir euxis-cpp/build --output-on-failure

cpp-bench: cpp-build
	euxis-cpp/build/euxis-bench-cpp/euxis_bench --suite all \
		--output euxis-data/perf/cpp-benchmark-results.json

cpp-clean:
	rm -rf euxis-cpp/build

# C++ style guide enforcement (Google C++ Style via clang-format/clang-tidy)
cpp-format:
	find euxis-cpp -name '*.cpp' -o -name '*.hpp' | grep -v build/ | xargs clang-format -i --style=file:euxis-cpp/.clang-format

cpp-format-check:
	find euxis-cpp -name '*.cpp' -o -name '*.hpp' | grep -v build/ | xargs clang-format --dry-run --Werror --style=file:euxis-cpp/.clang-format

cpp-tidy: cpp-build
	find euxis-cpp -name '*.cpp' | grep -v build/ | grep -v tests/ | \
		xargs -P $(CPP_BUILD_JOBS) -I{} clang-tidy {} -p euxis-cpp/build --config-file=euxis-cpp/.clang-tidy

# C++ code coverage (requires lcov; run after cpp-test)
cpp-coverage:
	cmake -B euxis-cpp/build -S euxis-cpp \
		-DCMAKE_TOOLCHAIN_FILE=$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake \
		-DCMAKE_BUILD_TYPE=Debug \
		-DEUXIS_COVERAGE=ON -DEUXIS_DISABLE_SANITIZERS=ON \
		$(if $(shell command -v ccache 2>/dev/null),-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache,)
	cmake --build euxis-cpp/build --parallel $(CPP_BUILD_JOBS)
	ctest --test-dir euxis-cpp/build --output-on-failure
	lcov --capture --directory euxis-cpp/build --output-file euxis-cpp/build/coverage.info \
		--ignore-errors mismatch
	lcov --remove euxis-cpp/build/coverage.info '/usr/*' '*/vcpkg_installed/*' '*/tests/*' '*/build/*' \
		--output-file euxis-cpp/build/coverage-filtered.info --ignore-errors unused
	genhtml euxis-cpp/build/coverage-filtered.info --output-directory euxis-cpp/build/coverage-report
	@echo "Coverage report: euxis-cpp/build/coverage-report/index.html"
