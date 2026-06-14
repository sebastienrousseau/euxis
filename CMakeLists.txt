cmake_minimum_required(VERSION 3.28)
project(euxis VERSION 0.0.10 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Point to cmake/ directory for cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(EuxisDefaults)
include(EuxisPackage)
include(FetchContent)
include(CheckIPOSupported)

# --- Performance Optimizations ---
option(EUXIS_NATIVE_ARCH "Use -march=native for dev builds" OFF)

if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Release")
    check_ipo_supported(RESULT lto_supported OUTPUT error)
    if(lto_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        # Use fat LTO objects: native code + LTO IR in every .o file.
        # Required so test targets (linked via GTest which may not use LTO)
        # can still resolve symbols from static libraries built with LTO.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            add_compile_options(-ffat-lto-objects)
        endif()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        if(EUXIS_NATIVE_ARCH)
            add_compile_options(-march=native)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            add_compile_options(-march=x86-64-v3)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            # Apple Silicon (M1+) and Linux ARM64 servers ship ARMv8.4-A
            # with crypto and CRC extensions; -mcpu=apple-m1 is also
            # acceptable on Darwin but armv8.4-a is portable to Graviton3,
            # Ampere Altra, and aarch64 Linux runners.
            add_compile_options(-march=armv8.4-a+crypto+crc)
        endif()
        add_compile_options(-O3)
    endif()
endif()

# Warnings-as-errors for euxis targets; SYSTEM-marked FetchContent deps are excluded automatically.
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

# --- Unified Dependency Resolution via FetchContent ---

# nlohmann_json
#
# NOTE: we deliberately do NOT use FIND_PACKAGE_ARGS here. inja v3.4.0's
# CMakeLists.txt has unguarded `install(TARGETS nlohmann_json EXPORT
# injaTargets)` calls (lines 52, 63) that run regardless of INJA_INSTALL.
# Those need `nlohmann_json` to be a real, installable target — which
# FetchContent gives us. find_package produces only the namespaced
# `nlohmann_json::nlohmann_json` imported alias, and the unprefixed
# `nlohmann_json` target does not exist, which breaks the configure
# step in CI environments that have nlohmann-json3-dev installed.
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)
if(NOT TARGET nlohmann_json::nlohmann_json)
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
endif()

# inja (Jinja2 for C++)
set(INJA_USE_EMBEDDED_JSON OFF CACHE BOOL "" FORCE)
set(BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    inja
    GIT_REPOSITORY https://github.com/pantor/inja.git
    GIT_TAG        v3.4.0
    SYSTEM
)
FetchContent_MakeAvailable(inja)

# yaml-cpp
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        0.8.0
    SYSTEM
)
FetchContent_MakeAvailable(yaml-cpp)
if(TARGET yaml-cpp)
    # Mirrors the ixwebsocket workaround below: the global
    # CMAKE_COMPILE_WARNING_AS_ERROR ON injects -Werror after any
    # PRIVATE flags, so target-level -Wno-error is overridden. Use the
    # property-based form. yaml-cpp 0.8.0 has -Wshadow false positives
    # under Apple Clang.
    set_target_properties(yaml-cpp PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    target_compile_options(yaml-cpp PRIVATE -Wno-shadow)
endif()

# spdlog (Use latest stable)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.0
    FIND_PACKAGE_ARGS NAMES spdlog
)
FetchContent_MakeAvailable(spdlog)

# googletest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    FIND_PACKAGE_ARGS NAMES GTest
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
if(NOT TARGET GTest::gtest_main)
    add_library(GTest::gtest_main ALIAS gtest_main)
endif()

# Google Benchmark (opt-in for libs/bench statistical harness)
option(EUXIS_BUILD_GBENCH "Fetch Google Benchmark and build the gbench harness" OFF)
if(EUXIS_BUILD_GBENCH)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.9.1
        SYSTEM
    )
    FetchContent_MakeAvailable(benchmark)
endif()

# ixwebsocket (With specific patch for missing cstdint in some versions)
find_package(OpenSSL QUIET)
FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG        v11.4.4
)
if(OpenSSL_FOUND)
    set(USE_TLS ON CACHE BOOL "" FORCE)
    message(STATUS "Euxis: IXWebSocket TLS enabled (OpenSSL ${OPENSSL_VERSION})")
else()
    set(USE_TLS OFF CACHE BOOL "" FORCE)
    message(STATUS "Euxis: IXWebSocket TLS disabled (OpenSSL not found)")
endif()
FetchContent_MakeAvailable(ixwebsocket)
if(TARGET ixwebsocket)
    # CMAKE_COMPILE_WARNING_AS_ERROR injects -Werror after PRIVATE flags, so
    # target-level -Wno-error is overridden. Property-based opt-out wins.
    set_target_properties(ixwebsocket PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    target_compile_options(ixwebsocket PRIVATE -Wno-shadow -Wno-array-bounds)
endif()
if(NOT TARGET ixwebsocket::ixwebsocket)
    add_library(ixwebsocket::ixwebsocket ALIAS ixwebsocket)
endif()

# httplib
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.7
)
set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(httplib)
if(NOT TARGET httplib::httplib)
    add_library(httplib::httplib ALIAS httplib)
endif()

# SQLite3
#
# CMake's FindSQLite3 module is supposed to create the SQLite3::SQLite3
# IMPORTED target whenever SQLite3 is found. In practice, this is not
# uniform across CMake versions and runner configurations — some
# environments find the library, set SQLite3_FOUND=TRUE, but do not
# materialise the imported target. The fallback below mirrors what
# modern FindSQLite3 modules do: synthesise the imported target from
# the variables that find_package always populates.
find_package(SQLite3 REQUIRED)
if(NOT TARGET SQLite3::SQLite3)
    add_library(SQLite3::SQLite3 INTERFACE IMPORTED)
    if(DEFINED SQLite3_INCLUDE_DIRS)
        set_target_properties(SQLite3::SQLite3 PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${SQLite3_INCLUDE_DIRS}")
    endif()
    if(DEFINED SQLite3_LIBRARIES)
        set_target_properties(SQLite3::SQLite3 PROPERTIES
            INTERFACE_LINK_LIBRARIES "${SQLite3_LIBRARIES}")
    endif()
    message(STATUS "Euxis: synthesised SQLite3::SQLite3 imported target")
endif()

# Libsodium
find_package(unofficial-sodium CONFIG QUIET)
if(NOT unofficial-sodium_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
    add_library(unofficial-sodium::sodium ALIAS PkgConfig::SODIUM)
endif()

# --- RPATH settings for installed binaries ---
set(CMAKE_SKIP_BUILD_RPATH OFF)
set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)
set(CMAKE_INSTALL_RPATH_USE_LINK_LIBRARIES TRUE)
if(APPLE)
    set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
else()
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
endif()

# --- Debug info compression for Release builds ---
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-g1)
    add_link_options(-Wl,--compress-debug-sections=zlib)
endif()

enable_testing()

# Build order (libs -> apps)
add_subdirectory(libs/crypto)
add_subdirectory(libs/bridge)
add_subdirectory(libs/memory)
add_subdirectory(libs/identity)
add_subdirectory(libs/inference)
add_subdirectory(libs/a2a-types)
add_subdirectory(libs/a2a)
add_subdirectory(libs/security)
add_subdirectory(libs/sbom)
add_subdirectory(libs/sca)
add_subdirectory(libs/slopsquatting)
add_subdirectory(libs/platform)
add_subdirectory(libs/runtime)
add_subdirectory(libs/metrics)
add_subdirectory(libs/network)
add_subdirectory(libs/core)
add_subdirectory(libs/publisher)
add_subdirectory(libs/adapters)
add_subdirectory(apps/gateway)
add_subdirectory(apps/cli)
add_subdirectory(apps/publisher)
add_subdirectory(libs/bench)
add_subdirectory(apps/etx)

# Optional: SDK examples (opt-in to avoid slowing default builds)
option(EUXIS_BUILD_EXAMPLES "Build C++ SDK examples under docs/examples/cpp/" OFF)
if(EUXIS_BUILD_EXAMPLES)
    add_subdirectory(docs/examples/cpp)
endif()

# Optional: Doxygen API reference target (auto-skips if doxygen absent)
option(EUXIS_BUILD_DOCS "Generate API docs with Doxygen if available" ON)
include(EuxisDocs)

# --- CPack packaging ---
set(CPACK_PACKAGE_NAME "euxis")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Euxis — multi-agent code certification CLI")
set(CPACK_GENERATOR "TGZ")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND CPACK_GENERATOR "DEB" "RPM")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "euxis maintainers")
endif()
include(CPack)
