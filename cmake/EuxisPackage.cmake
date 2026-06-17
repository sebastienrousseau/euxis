macro(euxis_add_library NAME)
  cmake_parse_arguments(ARG "" "" "SOURCES;DEPS;TEST_SOURCES" ${ARGN})
  add_library(${NAME} STATIC ${ARG_SOURCES})
  target_include_directories(${NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
  target_link_libraries(${NAME} PUBLIC ${ARG_DEPS})
  # Tests
  if(ARG_TEST_SOURCES)
    add_executable(${NAME}_tests ${ARG_TEST_SOURCES})
    target_link_libraries(${NAME}_tests PRIVATE ${NAME} GTest::gtest_main)
    add_test(NAME ${NAME}_tests COMMAND ${NAME}_tests)
    if(EUXIS_LSAN_NEEDS_DISABLE)
      set_tests_properties(${NAME}_tests PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
    endif()
  endif()
endmacro()
