add_executable(euxis-integration-tests
    test_crypto_memory_roundtrip.cpp
    test_bridge_admission_pipeline.cpp
    test_metrics_collection_analysis.cpp
    test_cli_dispatch_pipeline.cpp
    test_core_resilience_swarm.cpp
)
target_link_libraries(euxis-integration-tests PRIVATE
    euxis-crypto-cpp euxis-memory-cpp euxis-bridge-cpp
    euxis-core-cpp euxis-metrics-cpp euxis-cli-lib
    euxis-security-cpp GTest::gtest_main
)
add_test(NAME euxis-integration-tests COMMAND euxis-integration-tests)
