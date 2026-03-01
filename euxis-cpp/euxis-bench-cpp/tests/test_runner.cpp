#include <gtest/gtest.h>
#include <sodium.h>

#include <algorithm>
#include <chrono>
#include <string>

#include "euxis/bench/runner.hpp"
#include "euxis/bench/reporter.hpp"

namespace euxis::bench {
namespace {

// ---------------------------------------------------------------------------
// Fixture: initialise libsodium once
// ---------------------------------------------------------------------------
class BenchRunnerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_GE(sodium_init(), 0) << "sodium_init() failed";
    }

    // Helper: create a simple passing benchmark function.
    static auto make_passing_bench(const std::string& name,
                                    const std::string& suite) -> BenchmarkFn {
        return [name, suite]() -> BenchmarkResult {
            return BenchmarkResult{
                .name = name,
                .suite = suite,
                .passed = true,
                .value = 99.9,
                .unit = "%",
                .target = 95.0,
                .duration = std::chrono::microseconds{42},
                .message = "pass",
            };
        };
    }

    // Helper: create a simple failing benchmark function.
    static auto make_failing_bench(const std::string& name,
                                    const std::string& suite) -> BenchmarkFn {
        return [name, suite]() -> BenchmarkResult {
            return BenchmarkResult{
                .name = name,
                .suite = suite,
                .passed = false,
                .value = 10.0,
                .unit = "%",
                .target = 95.0,
                .duration = std::chrono::microseconds{100},
                .message = "fail",
            };
        };
    }
};

// ---------------------------------------------------------------------------
// register_benchmark adds entries
// ---------------------------------------------------------------------------
TEST_F(BenchRunnerTest, RegisterBenchmarkAddsEntries) {
    BenchmarkRunner runner;
    EXPECT_TRUE(runner.suites().empty());

    runner.register_benchmark("alpha", "bench1",
                              make_passing_bench("bench1", "alpha"));
    runner.register_benchmark("alpha", "bench2",
                              make_passing_bench("bench2", "alpha"));
    runner.register_benchmark("beta", "bench3",
                              make_passing_bench("bench3", "beta"));

    EXPECT_EQ(runner.suites().size(), 2u);
}

// ---------------------------------------------------------------------------
// run_suite runs only matching suite
// ---------------------------------------------------------------------------
TEST_F(BenchRunnerTest, RunSuiteFiltersCorrectly) {
    BenchmarkRunner runner;
    runner.register_benchmark("alpha", "a1",
                              make_passing_bench("a1", "alpha"));
    runner.register_benchmark("alpha", "a2",
                              make_passing_bench("a2", "alpha"));
    runner.register_benchmark("beta", "b1",
                              make_passing_bench("b1", "beta"));

    auto report = runner.run_suite("alpha");

    EXPECT_EQ(report.suite_name, "alpha");
    EXPECT_EQ(report.results.size(), 2u);
    EXPECT_EQ(report.passed, 2u);
    EXPECT_EQ(report.failed, 0u);

    // Verify all results belong to the alpha suite.
    for (const auto& r : report.results) {
        EXPECT_EQ(r.suite, "alpha");
    }
}

// ---------------------------------------------------------------------------
// run_suite with no matching entries returns empty report
// ---------------------------------------------------------------------------
TEST_F(BenchRunnerTest, RunSuiteNoMatch) {
    BenchmarkRunner runner;
    runner.register_benchmark("alpha", "a1",
                              make_passing_bench("a1", "alpha"));

    auto report = runner.run_suite("nonexistent");

    EXPECT_EQ(report.suite_name, "nonexistent");
    EXPECT_TRUE(report.results.empty());
    EXPECT_EQ(report.passed, 0u);
    EXPECT_EQ(report.failed, 0u);
}

// ---------------------------------------------------------------------------
// run_all runs everything
// ---------------------------------------------------------------------------
TEST_F(BenchRunnerTest, RunAllRunsAllSuites) {
    BenchmarkRunner runner;
    runner.register_benchmark("alpha", "a1",
                              make_passing_bench("a1", "alpha"));
    runner.register_benchmark("beta", "b1",
                              make_passing_bench("b1", "beta"));
    runner.register_benchmark("gamma", "g1",
                              make_failing_bench("g1", "gamma"));

    auto reports = runner.run_all();

    EXPECT_EQ(reports.size(), 3u);

    // Check that all suites are represented.
    std::vector<std::string> suite_names;
    for (const auto& r : reports) {
        suite_names.push_back(r.suite_name);
    }
    EXPECT_NE(std::find(suite_names.begin(), suite_names.end(), "alpha"),
              suite_names.end());
    EXPECT_NE(std::find(suite_names.begin(), suite_names.end(), "beta"),
              suite_names.end());
    EXPECT_NE(std::find(suite_names.begin(), suite_names.end(), "gamma"),
              suite_names.end());
}

// ---------------------------------------------------------------------------
// suites() returns unique suite names in insertion order
// ---------------------------------------------------------------------------
TEST_F(BenchRunnerTest, SuitesReturnsUniqueNames) {
    BenchmarkRunner runner;
    runner.register_benchmark("alpha", "a1",
                              make_passing_bench("a1", "alpha"));
    runner.register_benchmark("beta", "b1",
                              make_passing_bench("b1", "beta"));
    runner.register_benchmark("alpha", "a2",
                              make_passing_bench("a2", "alpha"));
    runner.register_benchmark("gamma", "g1",
                              make_passing_bench("g1", "gamma"));
    runner.register_benchmark("beta", "b2",
                              make_passing_bench("b2", "beta"));

    auto names = runner.suites();

    EXPECT_EQ(names.size(), 3u);
    // Insertion order: alpha, beta, gamma
    EXPECT_EQ(names[0], "alpha");
    EXPECT_EQ(names[1], "beta");
    EXPECT_EQ(names[2], "gamma");
}

// ---------------------------------------------------------------------------
// BenchmarkResult serializes to JSON
// ---------------------------------------------------------------------------
TEST_F(BenchRunnerTest, BenchmarkResultToJson) {
    BenchmarkResult result{
        .name = "test_bench",
        .suite = "test_suite",
        .passed = true,
        .value = 99.5,
        .unit = "%",
        .target = 95.0,
        .duration = std::chrono::microseconds{1234},
        .message = "All good",
    };

    const auto j = result.to_json();

    EXPECT_EQ(j.at("name").get<std::string>(), "test_bench");
    EXPECT_EQ(j.at("suite").get<std::string>(), "test_suite");
    EXPECT_EQ(j.at("passed").get<bool>(), true);
    EXPECT_DOUBLE_EQ(j.at("value").get<double>(), 99.5);
    EXPECT_EQ(j.at("unit").get<std::string>(), "%");
    EXPECT_DOUBLE_EQ(j.at("target").get<double>(), 95.0);
    EXPECT_EQ(j.at("duration_us").get<int64_t>(), 1234);
    EXPECT_EQ(j.at("message").get<std::string>(), "All good");
}

// ---------------------------------------------------------------------------
// SuiteReport contains correct pass/fail counts
// ---------------------------------------------------------------------------
TEST_F(BenchRunnerTest, SuiteReportPassFailCounts) {
    BenchmarkRunner runner;
    runner.register_benchmark("mixed", "pass1",
                              make_passing_bench("pass1", "mixed"));
    runner.register_benchmark("mixed", "pass2",
                              make_passing_bench("pass2", "mixed"));
    runner.register_benchmark("mixed", "fail1",
                              make_failing_bench("fail1", "mixed"));

    auto report = runner.run_suite("mixed");

    EXPECT_EQ(report.passed, 2u);
    EXPECT_EQ(report.failed, 1u);
    EXPECT_EQ(report.results.size(), 3u);
}

// ---------------------------------------------------------------------------
// SuiteReport serializes to JSON with correct structure
// ---------------------------------------------------------------------------
TEST_F(BenchRunnerTest, SuiteReportToJson) {
    BenchmarkRunner runner;
    runner.register_benchmark("json_test", "b1",
                              make_passing_bench("b1", "json_test"));
    runner.register_benchmark("json_test", "b2",
                              make_failing_bench("b2", "json_test"));

    auto report = runner.run_suite("json_test");
    const auto j = report.to_json();

    EXPECT_EQ(j.at("suite_name").get<std::string>(), "json_test");
    EXPECT_EQ(j.at("passed").get<size_t>(), 1u);
    EXPECT_EQ(j.at("failed").get<size_t>(), 1u);
    EXPECT_TRUE(j.contains("total_duration_us"));
    EXPECT_TRUE(j.contains("results"));
    EXPECT_EQ(j.at("results").size(), 2u);
}

// ---------------------------------------------------------------------------
// BenchmarkReporter generates valid JSON
// ---------------------------------------------------------------------------
TEST_F(BenchRunnerTest, ReporterToJson) {
    BenchmarkRunner runner;
    runner.register_benchmark("suite_a", "a1",
                              make_passing_bench("a1", "suite_a"));
    runner.register_benchmark("suite_b", "b1",
                              make_failing_bench("b1", "suite_b"));

    auto reports = runner.run_all();
    auto j = BenchmarkReporter::to_json(reports);

    EXPECT_TRUE(j.contains("suites"));
    EXPECT_EQ(j.at("suites").size(), 2u);
    EXPECT_EQ(j.at("total_passed").get<size_t>(), 1u);
    EXPECT_EQ(j.at("total_failed").get<size_t>(), 1u);
    EXPECT_TRUE(j.contains("total_duration_us"));
}

// ---------------------------------------------------------------------------
// BenchmarkReporter generates valid Markdown
// ---------------------------------------------------------------------------
TEST_F(BenchRunnerTest, ReporterToMarkdown) {
    BenchmarkRunner runner;
    runner.register_benchmark("md_suite", "m1",
                              make_passing_bench("m1", "md_suite"));
    runner.register_benchmark("md_suite", "m2",
                              make_failing_bench("m2", "md_suite"));

    auto reports = runner.run_all();
    auto md = BenchmarkReporter::to_markdown(reports);

    // Check that the markdown contains expected content.
    EXPECT_NE(md.find("# Euxis Benchmark Report"), std::string::npos);
    EXPECT_NE(md.find("| Suite |"), std::string::npos);
    EXPECT_NE(md.find("md_suite"), std::string::npos);
    EXPECT_NE(md.find("m1"), std::string::npos);
    EXPECT_NE(md.find("m2"), std::string::npos);
    EXPECT_NE(md.find("PASS"), std::string::npos);
    EXPECT_NE(md.find("FAIL"), std::string::npos);
    EXPECT_NE(md.find("1 passed"), std::string::npos);
    EXPECT_NE(md.find("1 failed"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Total duration is accumulated across benchmarks
// ---------------------------------------------------------------------------
TEST_F(BenchRunnerTest, TotalDurationAccumulated) {
    BenchmarkRunner runner;
    runner.register_benchmark("dur", "d1",
                              make_passing_bench("d1", "dur"));
    runner.register_benchmark("dur", "d2",
                              make_passing_bench("d2", "dur"));

    auto report = runner.run_suite("dur");

    // Each mock benchmark returns 42us, so total should be >= 84us.
    EXPECT_GE(report.total_duration.count(), 84);
}

// ---------------------------------------------------------------------------
// Empty runner produces empty results
// ---------------------------------------------------------------------------
TEST_F(BenchRunnerTest, EmptyRunnerProducesNothing) {
    BenchmarkRunner runner;

    EXPECT_TRUE(runner.suites().empty());

    auto reports = runner.run_all();
    EXPECT_TRUE(reports.empty());
}

} // namespace
} // namespace euxis::bench
