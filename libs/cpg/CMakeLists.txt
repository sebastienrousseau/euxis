# libs/cpg — Code Property Graph foundation.
#
# AST projection only in this batch (Week 8a). CFG (statement
# successor) and DDG (def-use) land in 8b / 8c without changing
# the public type surface.
#
# Depends on libs/parse for the source Ast. Pulls fixture paths
# from libs/parse via the same compile definition so the two test
# suites stay in sync.

euxis_add_library(euxis-cpg-cpp
    SOURCES
        src/types.cpp
        src/graph.cpp
        src/builder.cpp
    DEPS
        euxis-parse-cpp
    TEST_SOURCES
        tests/test_types.cpp
        tests/test_graph.cpp
        tests/test_builder.cpp
)

# The test suite reuses libs/parse's fixtures rather than forking
# the corpus. The compile definition mirrors what libs/parse sets on
# its own test target so the two paths stay in sync.
target_compile_definitions(euxis-cpg-cpp_tests PRIVATE
    EUXIS_PARSE_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/libs/parse/tests/fixtures"
)
