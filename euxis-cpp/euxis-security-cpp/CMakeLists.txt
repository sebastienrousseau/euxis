euxis_add_library(euxis-security-cpp
  SOURCES
    src/policy.cpp
  DEPS
    nlohmann_json::nlohmann_json
  TEST_SOURCES
    tests/test_policy.cpp
)
