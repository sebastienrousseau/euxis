cmake_minimum_required(VERSION 3.28)
project(euxis VERSION 0.0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/EuxisDefaults.cmake)
include(cmake/EuxisPackage.cmake)
include(FetchContent)

# --- Dependency resolution (vcpkg or system) ---

# nlohmann_json, spdlog, GTest: same target names in vcpkg and system packages
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

# libsodium: vcpkg uses unofficial-sodium::sodium, system uses pkg-config
find_package(unofficial-sodium CONFIG QUIET)
if(NOT unofficial-sodium_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
    add_library(unofficial-sodium::sodium ALIAS PkgConfig::SODIUM)
endif()

# cpp-httplib: vcpkg uses httplib::httplib, system may not have it
find_package(httplib CONFIG QUIET)
if(NOT httplib_FOUND)
    FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG        v0.18.7
        GIT_SHALLOW    TRUE
    )
    set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
    set(HTTPLIB_REQUIRE_BROTLI OFF CACHE BOOL "" FORCE)
    set(HTTPLIB_COMPILE OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(httplib)
endif()

# IXWebSocket: vcpkg uses ixwebsocket::ixwebsocket, FetchContent fallback
find_package(ixwebsocket CONFIG QUIET)
if(NOT ixwebsocket_FOUND)
    FetchContent_Declare(
        ixwebsocket
        GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
        GIT_TAG        v9.9.0
        GIT_SHALLOW    TRUE
    )
    set(USE_TLS OFF CACHE BOOL "" FORCE)
    set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(ixwebsocket)
    # IXWebSocket v9.x lacks <cstdint> includes needed by GCC 15+
    if(TARGET ixwebsocket)
        target_compile_options(ixwebsocket PRIVATE -include cstdint -Wno-error)
        if(NOT TARGET ixwebsocket::ixwebsocket)
            add_library(ixwebsocket::ixwebsocket ALIAS ixwebsocket)
        endif()
    endif()
endif()

enable_testing()

# Build order matches dependency graph
add_subdirectory(euxis-crypto-cpp)    # No deps
add_subdirectory(euxis-bridge-cpp)    # Depends on crypto
add_subdirectory(euxis-memory-cpp)    # Depends on crypto
add_subdirectory(euxis-identity-cpp)  # No deps
add_subdirectory(euxis-inference-cpp) # No deps
add_subdirectory(euxis-a2a-cpp)       # Depends on identity
add_subdirectory(euxis-security-cpp)  # No deps
add_subdirectory(euxis-scripts-cpp)   # No deps
add_subdirectory(euxis-runtime-cpp)   # nlohmann_json
add_subdirectory(euxis-metrics-cpp)   # nlohmann_json, spdlog
add_subdirectory(euxis-core-cpp)      # crypto-cpp, metrics-cpp
add_subdirectory(euxis-adapters-cpp)  # httplib, json, spdlog
add_subdirectory(euxis-gateway-cpp)   # all above + httplib, sodium
add_subdirectory(euxis-cli-cpp)       # executable, links core/gateway/metrics/security
add_subdirectory(euxis-bench-cpp)     # Depends on all above
add_subdirectory(euxis-etx)           # Qt6 GUI, depends on all above
