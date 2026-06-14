# libs/parse — tree-sitter-backed syntax parsing for C and C++23.
#
# We pull tree-sitter and the two grammars in via FetchContent so
# every contributor builds against the same pinned versions without
# needing system packages. The grammar repositories ship only a
# `src/parser.c` (+ optional `src/scanner.c`) — no CMakeLists.txt —
# so we compile each grammar into a small static library here.

include(FetchContent)

# ---- tree-sitter core ------------------------------------------------------
# v0.24+ ships a CMakeLists.txt that builds a `tree-sitter` target.
FetchContent_Declare(
    tree-sitter
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
    GIT_TAG        v0.24.6
    SYSTEM
)
FetchContent_MakeAvailable(tree-sitter)

# Some tree-sitter releases name the target `tree-sitter`, others
# `tree_sitter`. Provide a consistent alias for downstream linking.
if(TARGET tree-sitter AND NOT TARGET tree_sitter)
    add_library(tree_sitter ALIAS tree-sitter)
endif()

# ---- tree-sitter-c grammar -------------------------------------------------
FetchContent_Declare(
    tree-sitter-c
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-c.git
    GIT_TAG        v0.23.4
)
FetchContent_GetProperties(tree-sitter-c)
if(NOT tree-sitter-c_POPULATED)
    FetchContent_MakeAvailable(tree-sitter-c)
endif()

add_library(euxis_ts_grammar_c STATIC
    ${tree-sitter-c_SOURCE_DIR}/src/parser.c
)
target_include_directories(euxis_ts_grammar_c PRIVATE
    ${tree-sitter-c_SOURCE_DIR}/src
)
target_link_libraries(euxis_ts_grammar_c PUBLIC tree-sitter)
# Grammar parsers are generated C; silence the warnings the generator
# is known to produce so the project's warnings-as-errors gate does
# not fail on third-party code.
set_target_properties(euxis_ts_grammar_c PROPERTIES
    COMPILE_WARNING_AS_ERROR OFF
    POSITION_INDEPENDENT_CODE ON
)
target_compile_options(euxis_ts_grammar_c PRIVATE
    -w
)

# ---- tree-sitter-cpp grammar -----------------------------------------------
FetchContent_Declare(
    tree-sitter-cpp
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-cpp.git
    GIT_TAG        v0.23.4
)
FetchContent_GetProperties(tree-sitter-cpp)
if(NOT tree-sitter-cpp_POPULATED)
    FetchContent_MakeAvailable(tree-sitter-cpp)
endif()

# tree-sitter-cpp ships an external scanner for raw-string-literal
# tracking; it must be linked alongside the main parser. We probe
# for the scanner file because some grammar releases drop it.
set(_ts_cpp_sources ${tree-sitter-cpp_SOURCE_DIR}/src/parser.c)
if(EXISTS ${tree-sitter-cpp_SOURCE_DIR}/src/scanner.c)
    list(APPEND _ts_cpp_sources ${tree-sitter-cpp_SOURCE_DIR}/src/scanner.c)
endif()

add_library(euxis_ts_grammar_cpp STATIC ${_ts_cpp_sources})
target_include_directories(euxis_ts_grammar_cpp PRIVATE
    ${tree-sitter-cpp_SOURCE_DIR}/src
)
target_link_libraries(euxis_ts_grammar_cpp PUBLIC tree-sitter)
set_target_properties(euxis_ts_grammar_cpp PROPERTIES
    COMPILE_WARNING_AS_ERROR OFF
    POSITION_INDEPENDENT_CODE ON
)
target_compile_options(euxis_ts_grammar_cpp PRIVATE
    -w
)

# ---- libs/parse main library ----------------------------------------------
euxis_add_library(euxis-parse-cpp
    SOURCES
        src/ast.cpp
        src/parser.cpp
        src/language.cpp
    DEPS
        tree-sitter
        euxis_ts_grammar_c
        euxis_ts_grammar_cpp
    TEST_SOURCES
        tests/test_language.cpp
        tests/test_parser.cpp
)

# Surface the tree-sitter headers ONLY to libs/parse internals.
# The public include path of euxis-parse-cpp does NOT carry
# tree-sitter — that is the PIMPL boundary.
target_include_directories(euxis-parse-cpp PRIVATE
    ${tree-sitter_SOURCE_DIR}/lib/include
)

# Make sure the fixtures directory is reachable from the test
# binary's working directory. CMake copies the path into a compile
# definition rather than relying on `getcwd()` so the test can run
# from anywhere.
target_compile_definitions(euxis-parse-cpp_tests PRIVATE
    EUXIS_PARSE_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
)
