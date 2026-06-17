# libs/ensemble — multi-verifier quorum runner.
#
# Week 15 ships the runner + deterministic mock verifier. Real
# LLM-provider verifiers (Claude, OpenAI, Gemini) land as concrete
# Verifier subclasses in follow-up batches without touching the
# public surface here.

euxis_add_library(euxis-ensemble-cpp
    SOURCES
        src/deterministic.cpp
        src/runner.cpp
        src/prompt.cpp
        src/response.cpp
        src/providers/http_verifier.cpp
        src/providers/claude.cpp
        src/providers/openai.cpp
        src/providers/gemini.cpp
    DEPS
        euxis-security-cpp
        nlohmann_json::nlohmann_json
        httplib::httplib
    TEST_SOURCES
        tests/test_deterministic.cpp
        tests/test_runner.cpp
        tests/test_prompt.cpp
        tests/test_response.cpp
        tests/test_providers.cpp
)
