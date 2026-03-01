find_package(Qt6 COMPONENTS Widgets Network SvgWidgets QUIET)

if(Qt6_FOUND)
  set(CMAKE_AUTOMOC ON)

  qt_add_executable(euxis-etx
    include/euxis/etx/app.hpp
    include/euxis/etx/theme.hpp
    include/euxis/etx/registry.hpp
    src/main.cpp
    src/app.cpp
    src/theme.cpp
    src/config.cpp
    src/registry.cpp
    src/screens/welcome.cpp
    src/screens/dashboard.cpp
    src/screens/agent.cpp
    src/screens/about.cpp
    src/screens/fleet_monitor.cpp
    src/screens/settings.cpp
    src/screens/help.cpp
    src/screens/logs.cpp
    src/screens/playbooks.cpp
    src/screens/approvals.cpp
    src/screens/cortex.cpp
    src/screens/error_details.cpp
    src/screens/metrics.cpp
    src/screens/omnigraph.cpp
    src/screens/squad_detail.cpp
    src/screens/time_travel.cpp
    src/screens/tool_runner.cpp
    src/widgets/header.cpp
    src/widgets/shortcut_bar.cpp
    src/widgets/fleet_grid.cpp
    src/widgets/agent_card.cpp
    src/widgets/search_bar.cpp
    src/widgets/tip_bar.cpp
    src/widgets/output_panel.cpp
    src/widgets/provider_select.cpp
    src/widgets/resource_monitor.cpp
    src/widgets/scroll_minimap.cpp
    src/widgets/sparkline.cpp
  )
  target_include_directories(euxis-etx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_link_libraries(euxis-etx PRIVATE
    Qt6::Widgets Qt6::Network Qt6::SvgWidgets
    euxis-crypto-cpp euxis-bridge-cpp euxis-memory-cpp
    euxis-identity-cpp euxis-inference-cpp euxis-a2a-cpp
  )
  qt_add_resources(euxis-etx "resources" PREFIX "/" FILES
    resources/styles/liquid-glass.qss
    resources/styles/calm.qss
    resources/styles/focused.qss
    resources/icons/euxis-logo.svg
  )
  # --- Tests (require Qt6::Test) ---
  find_package(Qt6 COMPONENTS Test QUIET)
  if(Qt6Test_FOUND)
    set(ETX_TESTABLE_SOURCES
      include/euxis/etx/theme.hpp
      include/euxis/etx/registry.hpp
      src/config.cpp
      src/registry.cpp
      src/theme.cpp
    )

    add_executable(euxis-etx-tests
      ${ETX_TESTABLE_SOURCES}
      tests/etx_test_main.cpp
      tests/test_config.cpp
      tests/test_registry.cpp
      tests/test_theme.cpp
    )
    set_target_properties(euxis-etx-tests PROPERTIES AUTOMOC ON)
    target_include_directories(euxis-etx-tests PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(euxis-etx-tests PRIVATE
      Qt6::Widgets Qt6::Test
      GTest::gtest
    )
    qt_add_resources(euxis-etx-tests "test_resources" PREFIX "/" FILES
      resources/styles/liquid-glass.qss
      resources/styles/calm.qss
      resources/styles/focused.qss
    )
    add_test(NAME euxis-etx-tests COMMAND euxis-etx-tests)
    set_tests_properties(euxis-etx-tests PROPERTIES
      ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/tests/lsan_suppressions.txt"
    )
  else()
    message(STATUS "Qt6::Test not found, skipping euxis-etx tests")
  endif()
else()
  message(STATUS "Qt6 not found, skipping euxis-etx GUI build")
endif()
