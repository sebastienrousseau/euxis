euxis_add_library(euxis-platform-cpp
  SOURCES
    src/platform_factory.cpp
    src/sys/linux/platform.cpp
    src/sys/linux/sandbox.cpp
    src/sys/macos/platform.cpp
    src/sys/wsl/platform.cpp
  DEPS
    euxis-runtime-cpp
  TEST_SOURCES
    tests/test_linux_hardening.cpp
    tests/test_sandbox.cpp
)

target_include_directories(euxis-platform-cpp PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
