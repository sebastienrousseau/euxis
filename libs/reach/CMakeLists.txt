# libs/reach — call graph + reachability pruner.
#
# Week 16 lands intra-file call-graph construction (callgraph.cpp)
# and forward BFS reachability + Finding annotation (reachability.
# cpp). Cross-file resolution lands when the per-project scanner
# lifts findings out of the per-file loop.

euxis_add_library(euxis-reach-cpp
    SOURCES
        src/callgraph.cpp
        src/reachability.cpp
    DEPS
        euxis-cpg-cpp
        euxis-parse-cpp
        euxis-security-cpp
    TEST_SOURCES
        tests/test_callgraph.cpp
        tests/test_reachability.cpp
)
