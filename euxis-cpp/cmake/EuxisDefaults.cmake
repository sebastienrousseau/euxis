add_compile_options(-Wall -Wextra -Wpedantic -Werror)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "15")
  # GCC 15 false-positive in <regex> and <functional> internals
  add_compile_options(-Wno-maybe-uninitialized)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-fsanitize=address,undefined)
  add_link_options(-fsanitize=address,undefined)
endif()
