# libs/cpg — Code Property Graph.
#
# Week 8 landed AST projection (graph.cpp + builder.cpp). Week 9
# added CFG construction (cfg.cpp). Week 10 closes the triad with
# DDG construction (ddg.cpp).
#
# The Week 9 commit shipped cfg.cpp + test_cfg.cpp but the CMake
# wiring didn't make it in — so the library built without them
# until Week 10. This file is the consolidated wiring for both
# weeks plus Week 10's ddg.cpp.
#
# Depends on libs/parse for the source Ast. Pulls fixture paths
# from libs/parse via the same compile definition so the two test
# suites stay in sync.

euxis_add_library(euxis-cpg-cpp
    SOURCES
        src/types.cpp
        src/graph.cpp
        src/builder.cpp
        src/cfg.cpp
        src/ddg.cpp
    DEPS
        euxis-parse-cpp
    TEST_SOURCES
        tests/test_types.cpp
        tests/test_graph.cpp
        tests/test_builder.cpp
        tests/test_cfg.cpp
        tests/test_ddg.cpp
)

# The test suite reuses libs/parse's fixtures rather than forking
# the corpus. The compile definition mirrors what libs/parse sets on
# its own test target so the two paths stay in sync.
target_compile_definitions(euxis-cpg-cpp_tests PRIVATE
    EUXIS_PARSE_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/libs/parse/tests/fixtures"
)
