cmake_minimum_required(VERSION 3.28)
project(euxis VERSION 0.0.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Point to build/ directory for cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build/cmake")

include(EuxisDefaults)
include(EuxisPackage)
include(FetchContent)
include(CheckIPOSupported)

# --- Performance Optimizations ---
if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Release")
    check_ipo_supported(RESULT lto_supported OUTPUT error)
    if(lto_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options(-march=native -O3 -flto)
    endif()
endif()

# Disable warnings-as-errors for external dependencies that might trigger modern compiler diagnostics
set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)

# --- Unified Dependency Resolution via FetchContent ---

# nlohmann_json
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
)
FetchContent_MakeAvailable(inja)

# yaml-cpp
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        0.8.0
)
FetchContent_MakeAvailable(yaml-cpp)

# spdlog (Use latest stable)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.0
)
FetchContent_MakeAvailable(spdlog)

# googletest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
if(NOT TARGET GTest::gtest_main)
    add_library(GTest::gtest_main ALIAS gtest_main)
endif()

# ixwebsocket (With specific patch for missing cstdint in some versions)
FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG        v11.4.4
)
set(USE_TLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ixwebsocket)
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
find_package(SQLite3 REQUIRED)

# Libsodium
find_package(unofficial-sodium CONFIG QUIET)
if(NOT unofficial-sodium_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
    add_library(unofficial-sodium::sodium ALIAS PkgConfig::SODIUM)
endif()

enable_testing()

# Build order (pkg -> internal -> cmd)
add_subdirectory(pkg/crypto)
add_subdirectory(pkg/bridge)
add_subdirectory(pkg/memory)
add_subdirectory(pkg/identity)
add_subdirectory(pkg/inference)
add_subdirectory(pkg/a2a)
add_subdirectory(pkg/security)
add_subdirectory(internal/platform)
add_subdirectory(pkg/runtime)
add_subdirectory(pkg/metrics)
add_subdirectory(pkg/network)
add_subdirectory(pkg/core)
add_subdirectory(pkg/publisher)
add_subdirectory(pkg/adapters)
add_subdirectory(cmd/gateway)
add_subdirectory(cmd/cli)
add_subdirectory(cmd/publisher)
add_subdirectory(pkg/bench)
add_subdirectory(cmd/etx)
