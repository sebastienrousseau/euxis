# euxis-cli-cpp is an executable, not a library
add_executable(euxis-cli
  src/main.cpp
  src/engine.cpp
  src/omnigraph.cpp
)
target_include_directories(euxis-cli PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(euxis-cli PRIVATE
  euxis-core-cpp
  euxis-gateway-cpp
  euxis-metrics-cpp
  euxis-security-cpp
  nlohmann_json::nlohmann_json
  spdlog::spdlog
)

# Tests for the CLI library components
add_executable(euxis-cli-cpp_tests
  tests/test_engine.cpp
  tests/test_omnigraph.cpp
  src/engine.cpp
  src/omnigraph.cpp
)
target_include_directories(euxis-cli-cpp_tests PRIVATE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(euxis-cli-cpp_tests PRIVATE
  euxis-core-cpp
  euxis-gateway-cpp
  euxis-metrics-cpp
  euxis-security-cpp
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  GTest::gtest_main
)
add_test(NAME euxis-cli-cpp_tests COMMAND euxis-cli-cpp_tests)
