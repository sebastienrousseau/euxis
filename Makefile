.PHONY: all test lint format clean install dev build bench coverage check cpp-configure cpp-build cpp-test cpp-bench cpp-clean cpp-format cpp-coverage cpp-clang-tidy cpp-clang-tidy-perf cpp-clang-tidy-perf-fix

# Auto-detect available cores; override per run as needed.
CPP_BUILD_JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

all: build test

test: cpp-test

build: cpp-build

clean: cpp-clean

format: cpp-format

check: cpp-test
	@echo "All checks passed."

cpp-configure:
	cmake -B build/cmake-build -S . \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
		$(if $(VCPKG_ROOT),-DCMAKE_TOOLCHAIN_FILE=$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake,) \
		-DCMAKE_BUILD_TYPE=Release \
		-DEUXIS_COVERAGE=OFF -DEUXIS_DISABLE_SANITIZERS=ON

cpp-build: cpp-configure
	cmake --build build/cmake-build --parallel $(CPP_BUILD_JOBS)

cpp-test: cpp-build
	ctest --test-dir build/cmake-build --output-on-failure

cpp-clean:
	rm -rf build/cmake-build

cpp-bench: cpp-build
	@echo "Running C++ benchmarks..."
	@if [ -f build/cmake-build/libs/bench/euxis-bench-cpp_tests ]; then \
		build/cmake-build/libs/bench/euxis-bench-cpp_tests; \
	else \
		echo "No benchmark binary found — skipping."; \
	fi

cpp-format:
	find . -name '*.cpp' -o -name '*.hpp' | grep -v build/ | xargs clang-format -i --style=file:build/.clang-format

cpp-clang-tidy: cpp-configure
	find apps/ libs/ -name '*.cpp' | grep -v build/ | xargs -P4 clang-tidy -p build/cmake-build

# Issue #57: scoped clang-tidy for performance-unnecessary-copy-initialization.
# `cpp-clang-tidy-perf` surveys (read-only) so you can review the flagged
# sites against the policy doc; `cpp-clang-tidy-perf-fix` applies the
# mechanical --fix in place. Always survey first.
cpp-clang-tidy-perf: cpp-configure
	scripts/quality/clang_tidy_perf.sh survey

cpp-clang-tidy-perf-fix: cpp-configure
	scripts/quality/clang_tidy_perf.sh fix

# C++ code coverage (requires gcovr; run after cpp-test)
cpp-coverage:
	cmake -B build/cmake-build -S . \
		$(if $(VCPKG_ROOT),-DCMAKE_TOOLCHAIN_FILE=$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake,) \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
		-DCMAKE_BUILD_TYPE=Debug \
		-DEUXIS_COVERAGE=ON -DEUXIS_DISABLE_SANITIZERS=ON
	cmake --build build/cmake-build --parallel $(CPP_BUILD_JOBS)
	ctest --test-dir build/cmake-build --output-on-failure
	@mkdir -p coverage-report
	gcovr --root . --filter 'apps/' --filter 'libs/' \
		--exclude '.*tests/.*' --exclude '.*/build/.*' \
		--exclude 'apps/etx/src/main.cpp' \
		--html-details coverage-report/index.html \
		--json coverage-report/coverage.json \
		--print-summary --fail-under-line 98
