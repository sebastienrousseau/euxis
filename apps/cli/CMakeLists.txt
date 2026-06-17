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
  src/mcp_provider.cpp
  src/cmd/system.cpp
  src/cmd/fleet.cpp
  src/cmd/knowledge.cpp
  src/cmd/infra.cpp
  src/cmd/dev.cpp
  src/cmd/sdk.cpp
  src/cmd/specialized.cpp
  src/cmd/surface.cpp
  src/cmd/lifecycle.cpp
  src/cmd/certify.cpp
  src/cmd/system_card.cpp
  src/cmd/sbom.cpp
  src/cmd/slopsquatting.cpp
  src/cmd/cache.cpp
  src/cmd/attest.cpp
  src/cmd/scan.cpp
  src/agent_mcp_context.cpp
  src/certification.cpp
  src/playbook_schema.cpp
  src/sarif.cpp
  src/autofix.cpp
  src/lsp_bridge.cpp
  src/lang_detect.cpp
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
# Defensive re-find: root-scope find_package(SQLite3 REQUIRED) creates
# the SQLite3::SQLite3 IMPORTED target which CMake propagates to
# subdirectories. Some CI CMake versions / configs surface this target
# only in the directory it was found in, leaving apps/cli/CMakeLists
# unable to link it. Re-finding here is cheap (find_package short-
# circuits when a result is already cached) and guarantees the target
# resolves at this scope.
find_package(SQLite3 REQUIRED)
target_link_libraries(euxis-cli-lib PUBLIC
  euxis-core-cpp
  euxis-bridge-cpp
  euxis-network-cpp
  euxis-platform-cpp
  euxis-runtime-cpp
  euxis-gateway-cpp
  euxis-metrics-cpp
  euxis-security-cpp
  euxis-sbom-cpp
  euxis-sca-cpp
  euxis-slopsquatting-cpp
  euxis-cache-cpp
  euxis-attest-cpp
  euxis-parse-cpp
  euxis-cpg-cpp
  euxis-scan-cpp
  euxis-taint-cpp
  euxis-reach-cpp
  euxis-ensemble-cpp
  unofficial-sodium::sodium
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  SQLite3::SQLite3
)

# Main executable
add_executable(euxis-cli src/main.cpp)
target_link_libraries(euxis-cli PRIVATE euxis-cli-lib)
install(TARGETS euxis-cli RUNTIME DESTINATION bin)

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
  tests/test_cmd_surface.cpp
  tests/test_cmd_lifecycle.cpp
  tests/test_cmd_certify.cpp
  tests/test_cmd_system_card.cpp
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
  tests/test_agent_mcp_context.cpp
  tests/test_mcp_provider.cpp
  tests/test_verdict.cpp
  tests/test_playbook_schema.cpp
  tests/test_certification.cpp
  tests/test_hardening.cpp
  tests/test_sarif.cpp
)
target_link_libraries(euxis-cli-cpp_tests PRIVATE
  euxis-cli-lib
  GTest::gtest_main
)
add_test(NAME euxis-cli-cpp_tests COMMAND euxis-cli-cpp_tests)
