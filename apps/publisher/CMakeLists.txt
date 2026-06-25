add_library(euxis-publisher-app-lib STATIC
  src/publisher_main.cpp
)
target_include_directories(euxis-publisher-app-lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(euxis-publisher-app-lib PUBLIC
  euxis-publisher-cpp
  euxis-runtime-cpp
  nlohmann_json::nlohmann_json
  spdlog::spdlog
)

add_executable(euxis-publisher
  src/main.cpp
)

target_link_libraries(euxis-publisher PRIVATE
  euxis-publisher-app-lib
)

install(TARGETS euxis-publisher DESTINATION bin)

add_executable(euxis-publisher-app-cpp_tests
  tests/test_main_entry.cpp
)
target_link_libraries(euxis-publisher-app-cpp_tests PRIVATE
  euxis-publisher-app-lib
  GTest::gtest_main
)
add_test(NAME euxis-publisher-app-cpp_tests COMMAND euxis-publisher-app-cpp_tests)
if(EUXIS_LSAN_NEEDS_DISABLE)
  set_tests_properties(euxis-publisher-app-cpp_tests PROPERTIES
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()
