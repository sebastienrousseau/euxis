#include "euxis/bench/reporter.hpp"

#include <cstdio>
#include <format>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <spdlog/spdlog.h>

namespace euxis::bench {

// ---------------------------------------------------------------------------
// BenchmarkReporter::to_json
// ---------------------------------------------------------------------------
auto BenchmarkReporter::to_json(const std::vector<SuiteReport>& reports)
    -> nlohmann::json {
    nlohmann::json root;

    auto suites = nlohmann::json::array();
    for (const auto& report : reports) {
        suites.push_back(report.to_json());
    }
    root["suites"] = std::move(suites);

    // Compute aggregate statistics.
    size_t total_passed = 0;
    size_t total_failed = 0;
    int64_t total_duration_us = 0;
    for (const auto& report : reports) {
        total_passed += report.passed;
        total_failed += report.failed;
        total_duration_us += report.total_duration.count();
    }
    root["total_passed"] = total_passed;
    root["total_failed"] = total_failed;
    root["total_duration_us"] = total_duration_us;

    return root;
}

// ---------------------------------------------------------------------------
// BenchmarkReporter::to_markdown
// ---------------------------------------------------------------------------
auto BenchmarkReporter::to_markdown(const std::vector<SuiteReport>& reports)
    -> std::string {
    std::ostringstream os;

    os << "# Euxis Benchmark Report\n\n";

    // Summary
    size_t total_passed = 0;
    size_t total_failed = 0;
    for (const auto& report : reports) {
        total_passed += report.passed;
        total_failed += report.failed;
    }
    os << std::format("**Total:** {} passed, {} failed\n\n",
                      total_passed, total_failed);

    // Table header
    os << "| Suite | Benchmark | Value | Target | Unit | Status | Duration |\n";
    os << "|-------|-----------|-------|--------|------|--------|----------|\n";

    for (const auto& report : reports) {
        for (const auto& r : report.results) {
            os << std::format("| {} | {} | {:.4f} | {:.4f} | {} | {} | {}us |\n",
                              r.suite,
                              r.name,
                              r.value,
                              r.target,
                              r.unit,
                              r.passed ? "PASS" : "FAIL",
                              r.duration.count());
        }
    }

    os << "\n";
    return os.str();
}

// ---------------------------------------------------------------------------
// BenchmarkReporter::write_json
// ---------------------------------------------------------------------------
void BenchmarkReporter::write_json(const std::vector<SuiteReport>& reports,
                                    const std::filesystem::path& path) {
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error(
            std::format("Failed to open file for writing: {}", path.string()));
    }
    out << to_json(reports).dump(2) << "\n";
    spdlog::info("Benchmark report written to {}", path.string());
}

// ---------------------------------------------------------------------------
// BenchmarkReporter::write_markdown
// ---------------------------------------------------------------------------
void BenchmarkReporter::write_markdown(const std::vector<SuiteReport>& reports,
                                        const std::filesystem::path& path) {
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error(
            std::format("Failed to open file for writing: {}", path.string()));
    }
    out << to_markdown(reports);
    spdlog::info("Benchmark Markdown report written to {}", path.string());
}

} // namespace euxis::bench
