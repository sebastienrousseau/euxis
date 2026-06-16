# Google Benchmark suite for euxis::cache::JsonCache.
# Builds only when the `benchmark` target from the root
# FetchContent block is available (i.e. EUXIS_BENCHMARKS=ON,
# which is the default).

if(NOT TARGET benchmark::benchmark)
    return()
endif()

add_executable(euxis-cache-bench bench_json_cache.cpp)
target_link_libraries(euxis-cache-bench PRIVATE
    euxis-cache-cpp
    benchmark::benchmark
    nlohmann_json::nlohmann_json
)
target_compile_features(euxis-cache-bench PRIVATE cxx_std_23)

# Third-party benchmark warnings shouldn't fail our -Werror gate.
if(COMMAND euxis_relax_thirdparty_warnings)
    euxis_relax_thirdparty_warnings(benchmark benchmark_main)
endif()
