# libs/parse — tree-sitter-backed syntax parsing for C and C++23.
#
# We pull tree-sitter and the two grammars in via FetchContent so
# every contributor builds against the same pinned versions without
# needing system packages. The grammar repositories ship only a
# `src/parser.c` (+ optional `src/scanner.c`) — no CMakeLists.txt —
# so we compile each grammar into a small static library here.

# Root project is declared LANGUAGES CXX only. The grammar
# parser.c sources need a working C toolchain; enable it here so
# every grammar library (euxis_ts_grammar_*) below can be built.
enable_language(C)

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
# SOURCE_SUBDIR pointing at a non-existent path tells FetchContent to
# populate the repo (so we can reach parser.c) but skip the implicit
# add_subdirectory() of the grammar's own CMakeLists.txt. Necessary
# because every grammar repo defines an `ts-test` target with the
# same name, which collides at root scope under CMP0002. We only
# need the generated C sources, not the grammar's build system.
FetchContent_Declare(
    tree-sitter-c
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-c.git
    GIT_TAG        v0.23.4
    SOURCE_SUBDIR  _euxis_skip_subdir_
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
    INTERPROCEDURAL_OPTIMIZATION OFF
)
target_compile_options(euxis_ts_grammar_c PRIVATE
    -w
)

# ---- tree-sitter-cpp grammar -----------------------------------------------
FetchContent_Declare(
    tree-sitter-cpp
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-cpp.git
    GIT_TAG        v0.23.4
    SOURCE_SUBDIR  _euxis_skip_subdir_
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
    INTERPROCEDURAL_OPTIMIZATION OFF
)
target_compile_options(euxis_ts_grammar_cpp PRIVATE
    -w
)

# ---- additional grammars --------------------------------------------------
# Helper macro: clone a tree-sitter-<lang> repo, compile its
# `src/parser.c` (plus `src/scanner.c` when present) into a small
# static library, and silence the inevitable generator-produced
# warnings. The macro intentionally avoids the FetchContent_Declare
# pattern of the core dep so version pins stay co-located with the
# library definition.
macro(euxis_add_grammar NAME REPO TAG)
    FetchContent_Declare(
        ${NAME}
        GIT_REPOSITORY ${REPO}
        GIT_TAG        ${TAG}
        SOURCE_SUBDIR  _euxis_skip_subdir_
    )
    FetchContent_GetProperties(${NAME})
    if(NOT ${NAME}_POPULATED)
        FetchContent_MakeAvailable(${NAME})
    endif()
    set(_ts_sources ${${NAME}_SOURCE_DIR}/src/parser.c)
    if(EXISTS ${${NAME}_SOURCE_DIR}/src/scanner.c)
        list(APPEND _ts_sources ${${NAME}_SOURCE_DIR}/src/scanner.c)
    endif()
    add_library(euxis_${NAME} STATIC ${_ts_sources})
    target_include_directories(euxis_${NAME} PRIVATE
        ${${NAME}_SOURCE_DIR}/src
    )
    target_link_libraries(euxis_${NAME} PUBLIC tree-sitter)
    set_target_properties(euxis_${NAME} PROPERTIES
        COMPILE_WARNING_AS_ERROR OFF
        POSITION_INDEPENDENT_CODE ON
        INTERPROCEDURAL_OPTIMIZATION OFF
    )
    target_compile_options(euxis_${NAME} PRIVATE -w)
endmacro()

euxis_add_grammar(ts_grammar_rust       https://github.com/tree-sitter/tree-sitter-rust.git       v0.23.2)
euxis_add_grammar(ts_grammar_go         https://github.com/tree-sitter/tree-sitter-go.git         v0.23.4)
euxis_add_grammar(ts_grammar_python     https://github.com/tree-sitter/tree-sitter-python.git     v0.23.6)
euxis_add_grammar(ts_grammar_javascript https://github.com/tree-sitter/tree-sitter-javascript.git v0.23.1)
euxis_add_grammar(ts_grammar_java       https://github.com/tree-sitter/tree-sitter-java.git       v0.23.5)

# tree-sitter-typescript is special — the repo ships two grammars
# (typescript + tsx) under subdirectories. We wire just the
# typescript subgrammar here; tsx lands when libs/parse adds a
# Language::Tsx enumerator in a follow-up batch.
FetchContent_Declare(
    ts_grammar_typescript
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-typescript.git
    GIT_TAG        v0.23.2
    SOURCE_SUBDIR  _euxis_skip_subdir_
)
FetchContent_GetProperties(ts_grammar_typescript)
if(NOT ts_grammar_typescript_POPULATED)
    FetchContent_MakeAvailable(ts_grammar_typescript)
endif()
set(_ts_typescript_sources
    ${ts_grammar_typescript_SOURCE_DIR}/typescript/src/parser.c
)
# Recent tree-sitter-typescript releases moved the scanner to
# `common/scanner.c`; older releases kept a per-grammar copy. Probe
# both so the build survives either layout.
if(EXISTS ${ts_grammar_typescript_SOURCE_DIR}/common/scanner.c)
    list(APPEND _ts_typescript_sources
        ${ts_grammar_typescript_SOURCE_DIR}/common/scanner.c)
elseif(EXISTS ${ts_grammar_typescript_SOURCE_DIR}/typescript/src/scanner.c)
    list(APPEND _ts_typescript_sources
        ${ts_grammar_typescript_SOURCE_DIR}/typescript/src/scanner.c)
endif()
add_library(euxis_ts_grammar_typescript STATIC ${_ts_typescript_sources})
target_include_directories(euxis_ts_grammar_typescript PRIVATE
    ${ts_grammar_typescript_SOURCE_DIR}/typescript/src
    ${ts_grammar_typescript_SOURCE_DIR}/common
)
target_link_libraries(euxis_ts_grammar_typescript PUBLIC tree-sitter)
set_target_properties(euxis_ts_grammar_typescript PROPERTIES
    COMPILE_WARNING_AS_ERROR OFF
    POSITION_INDEPENDENT_CODE ON
    INTERPROCEDURAL_OPTIMIZATION OFF
)
target_compile_options(euxis_ts_grammar_typescript PRIVATE -w)

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
        euxis_ts_grammar_rust
        euxis_ts_grammar_go
        euxis_ts_grammar_python
        euxis_ts_grammar_javascript
        euxis_ts_grammar_typescript
        euxis_ts_grammar_java
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
