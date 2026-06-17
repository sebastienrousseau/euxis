euxis_add_library(euxis-bench-cpp
  SOURCES
    src/runner.cpp
    src/reporter.cpp
    src/security_bench.cpp
    src/autonomy_bench.cpp
    src/performance_bench.cpp
    src/portability_bench.cpp
    src/interop_bench.cpp
    src/result_matrix.cpp
  DEPS
    euxis-crypto-cpp
    euxis-bridge-cpp
    euxis-memory-cpp
    euxis-identity-cpp
    euxis-inference-cpp
    euxis-a2a-cpp
    nlohmann_json::nlohmann_json
    spdlog::spdlog
  TEST_SOURCES
    tests/test_runner.cpp
    tests/test_result_matrix.cpp
    tests/test_bench_suites.cpp
)

# Optional: Google Benchmark harness for the performance suite.
# Enabled by -DEUXIS_BUILD_GBENCH=ON at the root.
if(EUXIS_BUILD_GBENCH)
    add_executable(euxis_perf_gbench src/performance_bench_gbench.cpp)
    target_link_libraries(euxis_perf_gbench
        PRIVATE
            euxis-crypto-cpp
            euxis-a2a-cpp
            benchmark::benchmark
            benchmark::benchmark_main
    )

    # Scan + evidence pipeline benchmarks. Distinct binary so the
    # crypto micro-benchmarks (euxis_perf_gbench) stay focused on
    # primitives while euxis_scan_gbench reports the user-visible
    # SBOM / sign / parse / cache numbers.
    add_executable(euxis_scan_gbench src/scan_bench.cpp)
    target_link_libraries(euxis_scan_gbench
        PRIVATE
            euxis-attest-cpp
            euxis-cache-cpp
            euxis-crypto-cpp
            euxis-sbom-cpp
            euxis-sca-cpp
            euxis-slopsquatting-cpp
            benchmark::benchmark
            benchmark::benchmark_main
    )

    # Agentic-core microbenchmarks: context engine, iteration budget,
    # approval classifier, tool dispatch. Separate binary so the
    # crypto and scan suites stay focused on their own slices.
    add_executable(euxis_agent_gbench src/agent_bench.cpp)
    target_link_libraries(euxis_agent_gbench
        PRIVATE
            euxis-runtime-cpp
            nlohmann_json::nlohmann_json
            benchmark::benchmark
            benchmark::benchmark_main
    )
endif()
