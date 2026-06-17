# libs/scan — OpenGrep rule-pack ingester + rule engine.
#
# Week 11 lands the YAML loader + literal-pattern execution against
# a CPG. Future batches add metavariable binding, pattern-not /
# pattern-inside, and AST-based pattern matching.
#
# yaml-cpp is fetched at the root CMakeLists.txt; the unprefixed
# target name is reliable across yaml-cpp 0.7 and 0.8.

euxis_add_library(euxis-scan-cpp
    SOURCES
        src/rule.cpp
        src/rule_loader.cpp
        src/rule_engine.cpp
    DEPS
        euxis-cpg-cpp
        euxis-security-cpp
        euxis-parse-cpp
        yaml-cpp
    TEST_SOURCES
        tests/test_rule.cpp
        tests/test_rule_loader.cpp
        tests/test_rule_engine.cpp
)
