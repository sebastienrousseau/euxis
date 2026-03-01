find_package(Qt6 COMPONENTS Widgets Network QUIET)

if(Qt6_FOUND)
  set(CMAKE_AUTOMOC ON)

  qt_add_executable(euxis-etx
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
    Qt6::Widgets Qt6::Network
    euxis-crypto-cpp euxis-bridge-cpp euxis-memory-cpp
    euxis-identity-cpp euxis-inference-cpp euxis-a2a-cpp
  )
  qt_add_resources(euxis-etx "resources" PREFIX "/" FILES
    resources/styles/liquid-glass.qss
    resources/styles/calm.qss
    resources/styles/focused.qss
    resources/icons/euxis-logo.svg
  )
else()
  message(STATUS "Qt6 not found, skipping euxis-etx GUI build")
endif()
