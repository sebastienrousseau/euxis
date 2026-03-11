.PHONY: all test lint format clean install dev build bench coverage

# Conservative default for laptop thermals/memory; override per run as needed.
CPP_BUILD_JOBS ?= 4

all: build test

test: cpp-test

build: cpp-build

clean: cpp-clean

format: cpp-format

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

cpp-format:
	find . -name '*.cpp' -o -name '*.hpp' | grep -v build/ | xargs clang-format -i --style=file:build/.clang-format

# C++ code coverage (requires lcov; run after cpp-test)
cpp-coverage:
	cmake -B build/cmake-build -S . \
		$(if $(VCPKG_ROOT),-DCMAKE_TOOLCHAIN_FILE=$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake,) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DEUXIS_COVERAGE=ON -DEUXIS_DISABLE_SANITIZERS=ON
	cmake --build build/cmake-build --parallel $(CPP_BUILD_JOBS)
	ctest --test-dir build/cmake-build --output-on-failure
