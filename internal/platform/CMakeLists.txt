euxis_add_library(euxis-platform-cpp
  SOURCES
    src/platform_factory.cpp
    src/sys/linux/platform.cpp
    src/sys/macos/platform.cpp
    src/sys/wsl/platform.cpp
  DEPS
    euxis-runtime-cpp
)

target_include_directories(euxis-platform-cpp PRIVATE
  ${CMAKE_SOURCE_DIR}/cmd/cli/include
)
