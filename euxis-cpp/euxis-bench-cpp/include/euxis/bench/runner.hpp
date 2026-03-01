#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::bench {

struct BenchmarkResult {
    std::string name;
    std::string suite;
    bool passed;
    double value;
    std::string unit;
    double target;
    std::chrono::microseconds duration;
    std::string message;

    [[nodiscard]] nlohmann::json to_json() const;
};

struct SuiteReport {
    std::string suite_name;
    std::vector<BenchmarkResult> results;
    size_t passed;
    size_t failed;
    std::chrono::microseconds total_duration;

    [[nodiscard]] nlohmann::json to_json() const;
};

using BenchmarkFn = std::function<BenchmarkResult()>;

class BenchmarkRunner {
public:
    void register_benchmark(std::string suite, std::string name, BenchmarkFn fn);
    [[nodiscard]] auto run_suite(const std::string& suite) -> SuiteReport;
    [[nodiscard]] auto run_all() -> std::vector<SuiteReport>;
    [[nodiscard]] auto suites() const -> std::vector<std::string>;

    /// Register all built-in benchmark suites
    void register_defaults();

private:
    struct Entry {
        std::string suite;
        std::string name;
        BenchmarkFn fn;
    };
    std::vector<Entry> benchmarks_;
};

} // namespace euxis::bench
