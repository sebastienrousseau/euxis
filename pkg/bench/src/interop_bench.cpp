#include "euxis/bench/runner.hpp"

#include <chrono>
#include <format>
#include <string>

#include "euxis/bridge/skill.hpp"
#include "euxis/a2a/agent_card.hpp"

#include <spdlog/spdlog.h>

namespace euxis::bench {

// ---------------------------------------------------------------------------
// Benchmark: skill_import_throughput
// ---------------------------------------------------------------------------
static auto bench_skill_import_throughput() -> BenchmarkResult {
    BenchmarkResult result;
    result.name = "skill_import_throughput";
    result.suite = "interop";
    result.unit = "ops/sec";
    result.target = 10'000.0;

    constexpr size_t num_skills = 100;

    // Build a template JSON for BridgedSkill::from_json.
    nlohmann::json skill_json;
    skill_json["name"] = "bench-skill";
    skill_json["slug"] = "bench-skill";
    skill_json["source_dir"] = "/tmp/skills/bench";
    skill_json["description"] = "A benchmark skill for throughput testing";
    skill_json["runtime"] = "python";
    skill_json["entrypoint"] = "main.py";
    skill_json["tags"] = nlohmann::json::array({"benchmark", "test", "perf"});
    skill_json["metadata"] = {{"version", "1.0.0"}, {"author", "euxis-bench"}};

    const auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < num_skills; ++i) {
        // Vary the name slightly to avoid any caching.
        skill_json["name"] = std::format("bench-skill-{}", i);
        skill_json["slug"] = std::format("bench-skill-{}", i);

        [[maybe_unused]] auto skill = bridge::BridgedSkill::from_json(skill_json);
    }

    const auto end = std::chrono::steady_clock::now();
    const auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    const double elapsed_sec =
        static_cast<double>(elapsed_us.count()) / 1'000'000.0;

    result.duration = elapsed_us;
    result.value = static_cast<double>(num_skills) / elapsed_sec;
    result.passed = result.value >= result.target;
    result.message = std::format(
        "{} skill imports in {:.3f}s = {:.0f} ops/sec",
        num_skills, elapsed_sec, result.value);

    spdlog::info("interop/skill_import_throughput: {}", result.message);
    return result;
}

// ---------------------------------------------------------------------------
// Benchmark: a2a_card_generation
// ---------------------------------------------------------------------------
static auto bench_a2a_card_generation() -> BenchmarkResult {
    BenchmarkResult result;
    result.name = "a2a_card_generation";
    result.suite = "interop";
    result.unit = "ms/op";
    result.target = 0.01;

    constexpr size_t num_cards = 100'000;

    const auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < num_cards; ++i) {
        a2a::AgentCard card;
        card.name = "bench-agent";
        card.description = "Benchmark agent for card generation testing";
        card.url = "https://bench.euxis.dev/a2a";
        card.version = "1.0.0";
        card.capabilities.push_back(a2a::Capability{
            .name = "generate",
            .description = "Generates benchmark data",
            .input_schema = std::nullopt,
            .output_schema = std::nullopt,
        });

        // Serialize to JSON to materialize the card fully.
        [[maybe_unused]] auto j = card.to_json();
    }

    const auto end = std::chrono::steady_clock::now();
    const auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    const double elapsed_ms =
        static_cast<double>(elapsed_us.count()) / 1'000.0;

    result.duration = elapsed_us;
    const double avg_ms = elapsed_ms / static_cast<double>(num_cards);
    result.value = avg_ms;
    result.passed = result.value <= result.target;
    result.message = std::format(
        "{} agent cards generated in {:.1f}ms, avg {:.6f}ms/op",
        num_cards, elapsed_ms, avg_ms);

    spdlog::info("interop/a2a_card_generation: {}", result.message);
    return result;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
void register_interop_benchmarks(BenchmarkRunner& runner) {
    runner.register_benchmark("interop", "skill_import_throughput",
                              bench_skill_import_throughput);
    runner.register_benchmark("interop", "a2a_card_generation",
                              bench_a2a_card_generation);
}

} // namespace euxis::bench
