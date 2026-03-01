cmake_minimum_required(VERSION 3.28)
project(euxis VERSION 0.0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/EuxisDefaults.cmake)
include(cmake/EuxisPackage.cmake)

find_package(unofficial-sodium CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

enable_testing()

# Build order matches dependency graph
add_subdirectory(euxis-crypto-cpp)    # No deps
add_subdirectory(euxis-bridge-cpp)    # Depends on crypto
add_subdirectory(euxis-memory-cpp)    # Depends on crypto
add_subdirectory(euxis-identity-cpp)  # No deps
add_subdirectory(euxis-inference-cpp) # No deps
add_subdirectory(euxis-a2a-cpp)       # Depends on identity
add_subdirectory(euxis-bench-cpp)     # Depends on all above
add_subdirectory(euxis-etx)           # Qt6 GUI, depends on all above
