/// @file
/// @brief Google Benchmark suite for euxis::cache::JsonCache.
///
/// Issue: `libs/cache/include/euxis/cache/json_cache.hpp:21-27`
/// claimed the warm-cache path was `< 5%` of cold-cache wall
/// time on the 53-file registry fixture. This file is the
/// actual measurement.

#include "euxis/cache/json_cache.hpp"

#include <benchmark/benchmark.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace euxis::cache::bench {

namespace fs = std::filesystem;

/// Build a deterministic 53-file fixture: 41 agent JSON docs
/// (~1 KiB each), 6 squad docs (~600 B), and 6 playbook docs
/// (~4 KiB). Matches the shape of the real
/// `agents/registry.json` + `squads.json` +
/// `config/playbooks/*.json` corpus the RegistryClient walks on
/// every CLI invocation.
auto build_fixture(const fs::path& dir) -> std::vector<fs::path> {
    fs::create_directories(dir);
    std::vector<fs::path> paths;
    paths.reserve(53);
    auto emit = [&](const fs::path& p, std::size_t pad_bytes) {
        nlohmann::json j;
        j["id"]      = p.stem().string();
        j["version"] = "0.0.3";
        j["tags"]    = {"benchmark", "fixture"};
        j["payload"] = std::string(pad_bytes, 'x');
        std::ofstream(p) << j.dump();
        paths.push_back(p);
    };
    for (int i = 0; i < 41; ++i)
        emit(dir / ("agent_"    + std::to_string(i) + ".json"),  900);
    for (int i = 0; i <  6; ++i)
        emit(dir / ("squad_"    + std::to_string(i) + ".json"),  500);
    for (int i = 0; i <  6; ++i)
        emit(dir / ("playbook_" + std::to_string(i) + ".json"), 3800);
    return paths;
}

/// Cold-cache scenario: a fresh JsonCache database opened per
/// iteration, every fixture file is `get_or_load`ed. Measures
/// the dominant cost on a clean machine — full parse + msgpack
/// store.
void BM_ColdGetOrLoad(benchmark::State& state) {
    auto fixture_dir = fs::temp_directory_path() / "euxis_bench_json_fixture";
    auto cache_dir   = fs::temp_directory_path() / "euxis_bench_json_cold";
    auto paths       = build_fixture(fixture_dir);
    for (auto _ : state) {
        state.PauseTiming();
        fs::remove_all(cache_dir);
        auto cache = JsonCache::open(cache_dir / "cache.sqlite");
        state.ResumeTiming();
        if (!cache) continue;
        for (const auto& p : paths) {
            auto j = cache->get_or_load(p);
            benchmark::DoNotOptimize(j);
        }
    }
    state.SetItemsProcessed(state.iterations() *
                            static_cast<int64_t>(paths.size()));
    fs::remove_all(fixture_dir);
    fs::remove_all(cache_dir);
}
BENCHMARK(BM_ColdGetOrLoad)->Unit(benchmark::kMillisecond);

/// Warm-cache scenario: cache is pre-populated once, every
/// iteration reads the same 53 files back through the
/// hash-validated cache hit path. Measures msgpack deserialise +
/// hash comparison only — the path euxis pays on every
/// short-lived CLI invocation after the first.
void BM_WarmGetOrLoad(benchmark::State& state) {
    auto fixture_dir = fs::temp_directory_path() / "euxis_bench_json_fixture";
    auto cache_dir   = fs::temp_directory_path() / "euxis_bench_json_warm";
    fs::remove_all(cache_dir);
    auto paths = build_fixture(fixture_dir);
    auto cache = JsonCache::open(cache_dir / "cache.sqlite");
    if (cache) {
        for (const auto& p : paths) {
            benchmark::DoNotOptimize(cache->get_or_load(p));
        }
    }
    for (auto _ : state) {
        if (!cache) break;
        for (const auto& p : paths) {
            auto j = cache->get_or_load(p);
            benchmark::DoNotOptimize(j);
        }
    }
    state.SetItemsProcessed(state.iterations() *
                            static_cast<int64_t>(paths.size()));
    fs::remove_all(fixture_dir);
    fs::remove_all(cache_dir);
}
BENCHMARK(BM_WarmGetOrLoad)->Unit(benchmark::kMillisecond);

/// Direct-parse baseline (no cache at all). Establishes what
/// JsonCache competes against — `nlohmann::json::parse` on every
/// invocation.
void BM_DirectParse(benchmark::State& state) {
    auto fixture_dir = fs::temp_directory_path() / "euxis_bench_json_fixture";
    auto paths       = build_fixture(fixture_dir);
    for (auto _ : state) {
        for (const auto& p : paths) {
            std::ifstream f(p);
            nlohmann::json j;
            f >> j;
            benchmark::DoNotOptimize(j);
        }
    }
    state.SetItemsProcessed(state.iterations() *
                            static_cast<int64_t>(paths.size()));
    fs::remove_all(fixture_dir);
}
BENCHMARK(BM_DirectParse)->Unit(benchmark::kMillisecond);

} // namespace euxis::cache::bench

BENCHMARK_MAIN();
