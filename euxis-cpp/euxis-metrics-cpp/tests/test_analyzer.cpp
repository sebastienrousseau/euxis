#include <gtest/gtest.h>
#include <filesystem>

#include "euxis/metrics/analyzer.hpp"

namespace euxis::metrics {
namespace {

TEST(AnalyzerTest, PercentileCalculation) {
    std::vector<double> values{10, 20, 30, 40, 50};
    EXPECT_DOUBLE_EQ(PerformanceAnalyzer::calculate_percentile(values, 50), 30.0);
    EXPECT_NEAR(PerformanceAnalyzer::calculate_percentile(values, 95), 48.0, 0.1);
}

TEST(AnalyzerTest, EmptyPercentile) {
    EXPECT_DOUBLE_EQ(PerformanceAnalyzer::calculate_percentile({}, 50), 0.0);
}

TEST(AnalyzerTest, SingleValuePercentile) {
    EXPECT_DOUBLE_EQ(PerformanceAnalyzer::calculate_percentile({42.0}, 99), 42.0);
}

TEST(AnalyzerTest, EmptyDirReturnsEmptyReport) {
    auto tmp = std::filesystem::temp_directory_path() / "euxis_analyzer_test";
    std::filesystem::remove_all(tmp);
    PerformanceAnalyzer a(tmp);
    auto report = a.generate_performance_report();
    EXPECT_TRUE(report["agent_performance"].empty());
    std::filesystem::remove_all(tmp);
}

} // namespace
} // namespace euxis::metrics
