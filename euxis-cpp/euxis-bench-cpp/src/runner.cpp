#include "euxis/bench/runner.hpp"

#include <algorithm>
#include <set>

#include <spdlog/spdlog.h>

// Forward declarations for built-in benchmark registration functions.
namespace euxis::bench {
void register_security_benchmarks(BenchmarkRunner& runner);
void register_autonomy_benchmarks(BenchmarkRunner& runner);
void register_performance_benchmarks(BenchmarkRunner& runner);
void register_portability_benchmarks(BenchmarkRunner& runner);
void register_interop_benchmarks(BenchmarkRunner& runner);
} // namespace euxis::bench

namespace euxis::bench {

// ---------------------------------------------------------------------------
// BenchmarkResult::to_json
// ---------------------------------------------------------------------------
nlohmann::json BenchmarkResult::to_json() const {
    nlohmann::json j;
    j["name"] = name;
    j["suite"] = suite;
    j["passed"] = passed;
    j["value"] = value;
    j["unit"] = unit;
    j["target"] = target;
    j["duration_us"] = duration.count();
    j["message"] = message;
    return j;
}

// ---------------------------------------------------------------------------
// SuiteReport::to_json
// ---------------------------------------------------------------------------
nlohmann::json SuiteReport::to_json() const {
    nlohmann::json j;
    j["suite_name"] = suite_name;
    j["passed"] = passed;
    j["failed"] = failed;
    j["total_duration_us"] = total_duration.count();

    auto arr = nlohmann::json::array();
    for (const auto& r : results) {
        arr.push_back(r.to_json());
    }
    j["results"] = std::move(arr);
    return j;
}

// ---------------------------------------------------------------------------
// BenchmarkRunner::register_benchmark
// ---------------------------------------------------------------------------
void BenchmarkRunner::register_benchmark(std::string suite, std::string name,
                                          BenchmarkFn fn) {
    spdlog::debug("Registering benchmark: suite={}, name={}", suite, name);
    benchmarks_.push_back(Entry{
        .suite = std::move(suite),
        .name = std::move(name),
        .fn = std::move(fn),
    });
}

// ---------------------------------------------------------------------------
// BenchmarkRunner::run_suite
// ---------------------------------------------------------------------------
auto BenchmarkRunner::run_suite(const std::string& suite) -> SuiteReport {
    SuiteReport report;
    report.suite_name = suite;
    report.passed = 0;
    report.failed = 0;
    report.total_duration = std::chrono::microseconds{0};

    for (const auto& entry : benchmarks_) {
        if (entry.suite != suite) {
            continue;
        }

        spdlog::info("Running benchmark: {} / {}", suite, entry.name);
        const auto start = std::chrono::steady_clock::now();
        auto result = entry.fn();
        const auto end = std::chrono::steady_clock::now();

        // If the benchmark function did not set duration, use wall-clock time
        if (result.duration == std::chrono::microseconds{0}) {
            result.duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end - start);
        }

        if (result.passed) {
            ++report.passed;
        } else {
            ++report.failed;
        }

        report.total_duration += result.duration;
        report.results.push_back(std::move(result));
    }

    spdlog::info("Suite '{}' complete: {}/{} passed in {}us",
                 suite, report.passed, report.passed + report.failed,
                 report.total_duration.count());
    return report;
}

// ---------------------------------------------------------------------------
// BenchmarkRunner::run_all
// ---------------------------------------------------------------------------
auto BenchmarkRunner::run_all() -> std::vector<SuiteReport> {
    std::vector<SuiteReport> reports;
    const auto suite_names = suites();
    reports.reserve(suite_names.size());

    for (const auto& s : suite_names) {
        reports.push_back(run_suite(s));
    }

    return reports;
}

// ---------------------------------------------------------------------------
// BenchmarkRunner::suites
// ---------------------------------------------------------------------------
auto BenchmarkRunner::suites() const -> std::vector<std::string> {
    // Preserve insertion order while deduplicating.
    std::set<std::string> seen;
    std::vector<std::string> result;
    for (const auto& entry : benchmarks_) {
        if (seen.insert(entry.suite).second) {
            result.push_back(entry.suite);
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// BenchmarkRunner::register_defaults
// ---------------------------------------------------------------------------
void BenchmarkRunner::register_defaults() {
    register_security_benchmarks(*this);
    register_autonomy_benchmarks(*this);
    register_performance_benchmarks(*this);
    register_portability_benchmarks(*this);
    register_interop_benchmarks(*this);
}

} // namespace euxis::bench
