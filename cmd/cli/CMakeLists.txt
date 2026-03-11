# Static library with all sources except main.cpp (for test linking)
add_library(euxis-cli-lib STATIC
  src/engine.cpp
  src/omnigraph.cpp
  src/terminal.cpp
  src/term_caps.cpp
  src/box.cpp
  src/config_loader.cpp
  src/pii_filter.cpp
  src/process.cpp
  src/session.cpp
  src/registry_client.cpp
  src/provider_router.cpp
  src/provider_executor.cpp
  src/auth_profile_store.cpp
  src/cli_provider.cpp
  src/cmd/system.cpp
  src/cmd/fleet.cpp
  src/cmd/knowledge.cpp
  src/cmd/infra.cpp
  src/cmd/dev.cpp
  src/cmd/specialized.cpp
  src/tui/color_system.cpp
  src/tui/event_loop.cpp
  src/tui/layout.cpp
  src/tui/focus_manager.cpp
  src/tui/keybindings.cpp
  src/tui/palette.cpp
  src/tui/syntax.cpp
  src/tui/collapsible.cpp
  src/tui/diff_view.cpp
  src/tui/workspace.cpp
  src/tui/chat_widget.cpp
  src/tui/context_sidebar.cpp
  src/tui/swarm_view.cpp
  src/tui/text_utils.cpp
)
target_include_directories(euxis-cli-lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(euxis-cli-lib PUBLIC
  euxis-core-cpp
  euxis-platform-cpp
  euxis-runtime-cpp
  euxis-gateway-cpp
  euxis-metrics-cpp
  euxis-security-cpp
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  SQLite::SQLite3
)

# Main executable
add_executable(euxis-cli src/main.cpp)
target_link_libraries(euxis-cli PRIVATE euxis-cli-lib)

# Tests
add_executable(euxis-cli-cpp_tests
  tests/test_engine.cpp
  tests/test_omnigraph.cpp
  tests/test_terminal.cpp
  tests/test_config_loader.cpp
  tests/test_registry_client.cpp
  tests/test_provider_router.cpp
  tests/test_session.cpp
  tests/test_pii_filter.cpp
  tests/test_cmd_system.cpp
  tests/test_cmd_fleet.cpp
  tests/test_cmd_knowledge.cpp
  tests/test_cmd_infra.cpp
  tests/test_cmd_dev.cpp
  tests/test_cmd_specialized.cpp
  tests/test_auth_profile_store.cpp
  tests/test_process.cpp
  tests/test_provider_executor.cpp
  tests/test_term_caps.cpp
  tests/test_box.cpp
  tests/test_cli_provider.cpp
  tests/test_i18n.cpp
  tests/test_color_system.cpp
  tests/test_event_loop.cpp
  tests/test_platform.cpp
  tests/test_layout.cpp
  tests/test_keybindings.cpp
  tests/test_palette.cpp
  tests/test_syntax.cpp
  tests/test_collapsible.cpp
  tests/test_diff_view.cpp
  tests/test_workspace.cpp
  tests/test_context_sidebar.cpp
  tests/test_swarm_view.cpp
  tests/test_text_utils.cpp
)
target_link_libraries(euxis-cli-cpp_tests PRIVATE
  euxis-cli-lib
  GTest::gtest_main
)
add_test(NAME euxis-cli-cpp_tests COMMAND euxis-cli-cpp_tests)
