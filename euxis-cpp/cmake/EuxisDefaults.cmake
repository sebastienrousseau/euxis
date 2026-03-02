add_compile_options(-Wall -Wextra -Wpedantic -Werror)

# Security hardening flags
add_compile_options(-fstack-protector-strong)
add_compile_options(-Wformat-security)
if(NOT WIN32)
  add_compile_options(-fPIE)
  add_link_options(-Wl,-z,relro -Wl,-z,now)
  add_link_options(-pie)
  # _FORTIFY_SOURCE requires optimization; only set for Release/RelWithDebInfo
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT CMAKE_BUILD_TYPE STREQUAL "")
    add_compile_definitions(_FORTIFY_SOURCE=2)
  endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "15")
  # GCC 15 false-positive in <regex> and <functional> internals
  add_compile_options(-Wno-maybe-uninitialized)
endif()

# Sanitizers (debug/dev builds)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  if(NOT EUXIS_DISABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
  endif()
endif()

# Coverage support (opt-in via -DEUXIS_COVERAGE=ON)
if(EUXIS_COVERAGE)
  add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
  add_link_options(--coverage)
endif()
