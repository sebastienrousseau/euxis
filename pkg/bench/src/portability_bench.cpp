#include "euxis/bench/runner.hpp"

#include <chrono>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include "euxis/bridge/platform.hpp"

#include <spdlog/spdlog.h>

namespace euxis::bench {

// ---------------------------------------------------------------------------
// Benchmark: platform_detection
// ---------------------------------------------------------------------------
static auto bench_platform_detection() -> BenchmarkResult {
    BenchmarkResult result;
    result.name = "platform_detection";
    result.suite = "portability";
    result.unit = "%";
    result.target = 100.0;

    const auto start = std::chrono::steady_clock::now();

    auto info = bridge::detect_platform();

    // Count how many fields are non-empty / populated.
    // PlatformInfo has 4 fields: os_name, arch, has_nsjail, has_sandbox_exec.
    // The boolean fields are always "populated", so we check the strings.
    constexpr size_t total_fields = 4;
    size_t populated = 0;

    if (!info.os_name.empty()) {
        ++populated;
    }
    if (!info.arch.empty()) {
        ++populated;
    }
    // Boolean fields are always populated (they have a definite value).
    populated += 2; // has_nsjail, has_sandbox_exec

    const auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start);

    result.value = (static_cast<double>(populated) / static_cast<double>(total_fields)) * 100.0;
    result.passed = result.value >= result.target;
    result.message = std::format(
        "Platform: os={}, arch={}, {}/{} fields populated ({:.0f}%)",
        info.os_name, info.arch, populated, total_fields, result.value);

    spdlog::info("portability/platform_detection: {}", result.message);
    return result;
}

// ---------------------------------------------------------------------------
// Benchmark: path_normalization
// ---------------------------------------------------------------------------
static auto bench_path_normalization() -> BenchmarkResult {
    BenchmarkResult result;
    result.name = "path_normalization";
    result.suite = "portability";
    result.unit = "ops/sec";
    result.target = 1'000'000.0;

    // Generate a set of paths to normalize.
    const std::vector<std::string> raw_paths = {
        "/home/user/../user/./Documents/file.txt",
        "/tmp/./cache/../cache/data.bin",
        "relative/path/./to/../to/file.hpp",
        "/usr/local/./bin/../lib/libfoo.so",
        "/opt/euxis/./skills/../skills/hello/main.py",
        "../../parent/child/./grandchild",
        "/var/log/./syslog",
        "./current/dir/file.rs",
        "/home/user/projects/./euxis/../../euxis/src/main.cpp",
        "/etc/./config/../config/app.toml",
    };

    constexpr size_t total_ops = 10'000;
    const auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < total_ops; ++i) {
        const auto& raw = raw_paths[i % raw_paths.size()];
        std::filesystem::path p(raw);
        [[maybe_unused]] auto normalized = p.lexically_normal();
    }

    const auto end = std::chrono::steady_clock::now();
    const auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    const double elapsed_sec =
        static_cast<double>(elapsed_us.count()) / 1'000'000.0;

    result.duration = elapsed_us;
    result.value = static_cast<double>(total_ops) / elapsed_sec;
    result.passed = result.value >= result.target;
    result.message = std::format(
        "{} path normalizations in {:.3f}s = {:.0f} ops/sec",
        total_ops, elapsed_sec, result.value);

    spdlog::info("portability/path_normalization: {}", result.message);
    return result;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
void register_portability_benchmarks(BenchmarkRunner& runner) {
    runner.register_benchmark("portability", "platform_detection",
                              bench_platform_detection);
    runner.register_benchmark("portability", "path_normalization",
                              bench_path_normalization);
}

} // namespace euxis::bench
