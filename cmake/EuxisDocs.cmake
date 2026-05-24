# EuxisDocs.cmake — optional Doxygen API reference target.
#
# Registers `docs` when doxygen is present on PATH. Prints a clear status
# message and does nothing otherwise. CI environments without doxygen are
# unaffected.

if(NOT EUXIS_BUILD_DOCS)
  return()
endif()

find_package(Doxygen QUIET OPTIONAL_COMPONENTS dot)

if(NOT DOXYGEN_FOUND)
  message(STATUS "Euxis: doxygen not found — skipping docs target "
                 "(install: pacman -S doxygen graphviz | apt install doxygen graphviz)")
  return()
endif()

# Validate that any discovered `dot` is actually graphviz, not a homonym
# (mise dotfiles tool, cargo dot, etc.). graphviz `dot -V` writes
# "dot - graphviz version X.Y.Z" to stderr.
set(DOXYGEN_HAVE_DOT "NO")
if(DOXYGEN_DOT_FOUND)
  execute_process(
    COMMAND ${DOXYGEN_DOT_EXECUTABLE} -V
    OUTPUT_VARIABLE _dot_stdout
    ERROR_VARIABLE  _dot_stderr
    RESULT_VARIABLE _dot_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
  )
  if(_dot_rc EQUAL 0 AND "${_dot_stdout}${_dot_stderr}" MATCHES "graphviz")
    set(DOXYGEN_HAVE_DOT "YES")
  endif()
endif()

if(DOXYGEN_HAVE_DOT STREQUAL "YES")
  message(STATUS "Euxis: doxygen ${DOXYGEN_VERSION} + graphviz found — docs target enabled")
else()
  message(STATUS "Euxis: doxygen ${DOXYGEN_VERSION} found, graphviz unavailable — docs target enabled (no diagrams)")
endif()

# Fetch doxygen-awesome-css (modern theme) — pin to a stable release.
# https://github.com/jothepro/doxygen-awesome-css
include(FetchContent)
FetchContent_Declare(
  doxygen_awesome
  GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css.git
  GIT_TAG        v2.3.4
)
FetchContent_MakeAvailable(doxygen_awesome)
set(DOXYGEN_AWESOME_DIR "${doxygen_awesome_SOURCE_DIR}")
message(STATUS "Euxis: doxygen-awesome-css fetched at ${DOXYGEN_AWESOME_DIR}")

configure_file(
  ${CMAKE_SOURCE_DIR}/Doxyfile.in
  ${CMAKE_BINARY_DIR}/Doxyfile
  @ONLY
)

add_custom_target(docs
  COMMAND Doxygen::doxygen ${CMAKE_BINARY_DIR}/Doxyfile
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Generating Euxis API documentation with Doxygen"
  VERBATIM
)
