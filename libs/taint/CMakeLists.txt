# libs/taint — forward-DDG taint analyzer.
#
# Consumes the AST + CPG + DDG from libs/parse + libs/cpg, classifies
# nodes against source / sink / sanitizer specs, and emits canonical
# Findings. Built-in spec library covers 8 languages with the
# highest-frequency taint patterns; richer rule packs land in
# follow-up batches (OpenGrep import, IRIS / AdaTaint mined corpora).

euxis_add_library(euxis-taint-cpp
    SOURCES
        src/spec.cpp
        src/builtin_specs.cpp
        src/analyzer.cpp
    DEPS
        euxis-cpg-cpp
        euxis-parse-cpp
        euxis-security-cpp
    TEST_SOURCES
        tests/test_spec.cpp
        tests/test_analyzer.cpp
)
