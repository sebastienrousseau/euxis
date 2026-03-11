cmake_minimum_required(VERSION 3.28)
project(euxis VERSION 0.0.3 LANGUAGES CXX)

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

# --- Performance Optimizations (Moved to root build logic) ---
if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Release")
    check_ipo_supported(RESULT lto_supported OUTPUT error)
    if(lto_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options(-march=native -O3 -flto)
    endif()
endif()

# --- Dependency resolution ---
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)

find_package(unofficial-sodium CONFIG QUIET)
if(NOT unofficial-sodium_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
    add_library(unofficial-sodium::sodium ALIAS PkgConfig::SODIUM)
endif()

find_package(httplib CONFIG QUIET)
if(NOT httplib_FOUND)
    FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG        v0.18.7
        GIT_SHALLOW    TRUE
    )
    set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(httplib)
endif()

find_package(ixwebsocket CONFIG QUIET)
if(NOT ixwebsocket_FOUND)
    FetchContent_Declare(
        ixwebsocket
        GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
        GIT_TAG        v9.9.0
        GIT_SHALLOW    TRUE
    )
    set(USE_TLS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(ixwebsocket)
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
add_subdirectory(pkg/adapters)
add_subdirectory(cmd/gateway)
add_subdirectory(cmd/cli)
add_subdirectory(pkg/bench)
add_subdirectory(cmd/etx)
