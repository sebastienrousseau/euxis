find_package(Qt6 COMPONENTS Widgets Network SvgWidgets Concurrent LinguistTools QUIET)

if(Qt6_FOUND)
  set(CMAKE_AUTOMOC ON)

  # Translation source files for 15 languages
  set(ETX_TS_FILES
    translations/euxis_etx_en.ts
    translations/euxis_etx_fr.ts
    translations/euxis_etx_es.ts
    translations/euxis_etx_de.ts
    translations/euxis_etx_pt.ts
    translations/euxis_etx_zh.ts
    translations/euxis_etx_ja.ts
    translations/euxis_etx_ko.ts
    translations/euxis_etx_ru.ts
    translations/euxis_etx_it.ts
    translations/euxis_etx_ar.ts
    translations/euxis_etx_he.ts
    translations/euxis_etx_hi.ts
    translations/euxis_etx_nl.ts
    translations/euxis_etx_id.ts
  )

  qt_add_executable(euxis-etx
    include/euxis/etx/app.hpp
    include/euxis/etx/theme.hpp
    include/euxis/etx/registry.hpp
    include/euxis/etx/chat_engine.hpp
    include/euxis/etx/conversation_widget.hpp
    include/euxis/etx/chat_input_widget.hpp
    include/euxis/etx/ghost_text.hpp
    include/euxis/etx/breadcrumb_widget.hpp
    include/euxis/etx/bento_layout.hpp
    include/euxis/etx/collapsible_section.hpp
    include/euxis/etx/semantic_colors.hpp
    include/euxis/etx/oauth_flow.hpp
    include/euxis/etx/accessibility.hpp
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
    src/accessibility.cpp
    src/chat_engine.cpp
    src/oauth_flow.cpp
    src/widgets/conversation.cpp
    src/widgets/chat_input.cpp
    src/ghost_text.cpp
    src/widgets/breadcrumb.cpp
    src/widgets/bento_layout.cpp
    src/widgets/collapsible_section.cpp
    src/semantic_colors.cpp
  )
  target_include_directories(euxis-etx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_link_libraries(euxis-etx PRIVATE
    Qt6::Widgets Qt6::Network Qt6::SvgWidgets Qt6::Concurrent
    nlohmann_json::nlohmann_json
    euxis-crypto-cpp euxis-bridge-cpp euxis-memory-cpp
    euxis-identity-cpp euxis-inference-cpp euxis-a2a-cpp
    euxis-cli-lib
  )
  qt_add_resources(euxis-etx "resources" PREFIX "/" FILES
    resources/styles/liquid-glass.qss
    resources/styles/calm.qss
    resources/styles/focused.qss
    resources/icons/euxis-logo.svg
  )

  # --- Translations (Qt Linguist) ---
  if(Qt6LinguistTools_FOUND)
    qt_add_translations(euxis-etx
      TS_FILES ${ETX_TS_FILES}
      RESOURCE_PREFIX "/translations"
    )
  else()
    message(STATUS "Qt6::LinguistTools not found, skipping translations")
  endif()

  # --- Tests (require Qt6::Test) ---
  find_package(Qt6 COMPONENTS Test QUIET)
  if(Qt6Test_FOUND)
    set(ETX_TESTABLE_SOURCES
      # Headers (for AUTOMOC)
      include/euxis/etx/app.hpp
      include/euxis/etx/theme.hpp
      include/euxis/etx/registry.hpp
      include/euxis/etx/chat_engine.hpp
      include/euxis/etx/conversation_widget.hpp
      include/euxis/etx/chat_input_widget.hpp
      include/euxis/etx/accessibility.hpp
      include/euxis/etx/ghost_text.hpp
      include/euxis/etx/breadcrumb_widget.hpp
      include/euxis/etx/bento_layout.hpp
      include/euxis/etx/collapsible_section.hpp
      include/euxis/etx/semantic_colors.hpp
      include/euxis/etx/oauth_flow.hpp
      # Core sources
      src/config.cpp
      src/registry.cpp
      src/theme.cpp
      src/accessibility.cpp
      src/ghost_text.cpp
      src/semantic_colors.cpp
      src/app.cpp
      src/chat_engine.cpp
      src/oauth_flow.cpp
      # Screen sources
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
      # Widget sources
      src/widgets/breadcrumb.cpp
      src/widgets/bento_layout.cpp
      src/widgets/collapsible_section.cpp
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
      src/widgets/conversation.cpp
      src/widgets/chat_input.cpp
    )

    add_executable(euxis-etx-tests
      ${ETX_TESTABLE_SOURCES}
      tests/etx_test_main.cpp
      tests/test_config.cpp
      tests/test_registry.cpp
      tests/test_theme.cpp
      tests/test_accessibility.cpp
      tests/test_ghost_text.cpp
      tests/test_breadcrumb.cpp
      tests/test_bento_layout.cpp
      tests/test_collapsible_section.cpp
      tests/test_semantic_colors.cpp
      tests/test_screens.cpp
      tests/test_widgets.cpp
      tests/test_chat_engine.cpp
      tests/test_oauth_flow.cpp
      tests/test_app.cpp
    )
    set_target_properties(euxis-etx-tests PROPERTIES AUTOMOC ON)
    target_include_directories(euxis-etx-tests PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(euxis-etx-tests PRIVATE
      Qt6::Widgets Qt6::Network Qt6::SvgWidgets Qt6::Concurrent Qt6::Test
      nlohmann_json::nlohmann_json
      GTest::gtest
      euxis-cli-lib
      euxis-crypto-cpp euxis-bridge-cpp euxis-memory-cpp
      euxis-identity-cpp euxis-inference-cpp euxis-a2a-cpp
    )
    qt_add_resources(euxis-etx-tests "test_resources" PREFIX "/" FILES
      resources/styles/liquid-glass.qss
      resources/styles/calm.qss
      resources/styles/focused.qss
      resources/icons/euxis-logo.svg
    )
    add_test(NAME euxis-etx-tests COMMAND euxis-etx-tests)
    # QT_QPA_PLATFORM=offscreen — Qt's default platform plugin tries to talk
    # to a real display, which (a) doesn't exist on Linux CI runners and
    # (b) flakes the ScreenTest cases under ASan on macOS by leaving Qt
    # widgets in a half-rendered state when the test tears them down.
    # The offscreen plugin renders into a pixmap; everything else is
    # identical behaviour.
    set_tests_properties(euxis-etx-tests PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/tests/lsan_suppressions.txt"
    )
  else()
    message(STATUS "Qt6::Test not found, skipping euxis-etx tests")
  endif()
else()
  message(STATUS "Qt6 not found, skipping euxis-etx GUI build")
endif()
