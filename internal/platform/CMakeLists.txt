euxis_add_library(euxis-platform-cpp
  SOURCES
    # We will conditionally add sources based on the OS
  DEPS
    euxis-runtime-cpp
)

if(APPLE)
  target_sources(euxis-platform-cpp PRIVATE src/sys/macos/platform.cpp)
elseif(WIN32)
  target_sources(euxis-platform-cpp PRIVATE src/sys/windows/platform.cpp)
else()
  # Basic WSL detection for CMake
  if(EXISTS "/proc/sys/kernel/osrelease")
    file(READ "/proc/sys/kernel/osrelease" OS_RELEASE)
    string(TOLOWER "${OS_RELEASE}" OS_RELEASE_LOWER)
    if(OS_RELEASE_LOWER MATCHES "microsoft|wsl")
      target_sources(euxis-platform-cpp PRIVATE src/sys/wsl/platform.cpp)
    else()
      target_sources(euxis-platform-cpp PRIVATE src/sys/linux/platform.cpp)
    endif()
  else()
    target_sources(euxis-platform-cpp PRIVATE src/sys/linux/platform.cpp)
  endif()
endif()
