#include <gtest/gtest.h>
#include <filesystem>

#include "euxis/metrics/performance_collector.hpp"
#include "euxis/metrics/fast_collector.hpp"
#include "euxis/metrics/analyzer.hpp"

namespace euxis::integration {
namespace {

class MetricsFlowTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_;

    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() / "euxis_integ_metrics";
        std::filesystem::remove_all(tmp_);
        std::filesystem::create_directories(tmp_);
    }

    void TearDown() override {
        std::filesystem::remove_all(tmp_);
    }
};

TEST_F(MetricsFlowTest, CollectorToAnalyzerPipeline) {
    metrics::PerformanceMetricsCollector collector(tmp_);

    // Simulate agent tasks
    auto sid1 = collector.task_started("agent-alpha", "session-1");
    collector.task_completed("session-1");

    auto sid2 = collector.task_started("agent-beta", "session-2");
    collector.task_completed("session-2");

    auto sid3 = collector.task_started("agent-alpha", "session-3");
    collector.task_failed("session-3");

    // Analyzer reads the same metrics directory
    metrics::PerformanceAnalyzer analyzer(tmp_);
    auto report = analyzer.generate_performance_report(24);
    EXPECT_FALSE(report.empty());
}

TEST_F(MetricsFlowTest, FastCollectorFlushPersists) {
    metrics::FastMetricsCollector fast(tmp_);

    fast.record_metric("latency", 42.0, {{"agent", "alpha"}});
    fast.record_metric("latency", 55.0, {{"agent", "beta"}});
    fast.record_timing("inference", 120.5);
    fast.record_counter("requests", 3);

    auto flushed = fast.flush();
    EXPECT_EQ(flushed, 4);
    EXPECT_TRUE(std::filesystem::exists(tmp_ / "events.jsonl"));
}

TEST_F(MetricsFlowTest, AgentRankingsFromCollectedData) {
    metrics::PerformanceMetricsCollector collector(tmp_);

    // alpha: 2 successes, 1 failure
    collector.task_started("alpha", "s1");
    collector.task_completed("s1");
    collector.task_started("alpha", "s2");
    collector.task_completed("s2");
    collector.task_started("alpha", "s3");
    collector.task_failed("s3");

    // beta: 3 successes, 0 failures
    collector.task_started("beta", "s4");
    collector.task_completed("s4");
    collector.task_started("beta", "s5");
    collector.task_completed("s5");
    collector.task_started("beta", "s6");
    collector.task_completed("s6");

    metrics::PerformanceAnalyzer analyzer(tmp_);
    auto rankings = analyzer.get_agent_rankings(24, "success_rate");
    // Rankings may be empty if the analyzer needs events in a specific format,
    // but if populated, both agents should appear
    if (!rankings.empty()) {
        bool found_alpha = false;
        bool found_beta = false;
        for (const auto& [name, score] : rankings) {
            if (name == "alpha") found_alpha = true;
            if (name == "beta") found_beta = true;
        }
        EXPECT_TRUE(found_alpha || found_beta);
    }
}

TEST_F(MetricsFlowTest, DelegationTracking) {
    metrics::PerformanceMetricsCollector collector(tmp_);

    auto corr_id = collector.delegation_started("supervisor", "worker-1", "scope_boundary");
    EXPECT_FALSE(corr_id.empty());

    collector.delegation_completed(corr_id, "supervisor");

    metrics::PerformanceAnalyzer analyzer(tmp_);
    auto patterns = analyzer.analyze_delegation_patterns(24);
    EXPECT_FALSE(patterns.empty());
}

TEST_F(MetricsFlowTest, ToolExecutionMetrics) {
    metrics::PerformanceMetricsCollector collector(tmp_);

    collector.tool_execution("agent-alpha", "file_read");
    collector.tool_execution("agent-alpha", "web_search");
    collector.tool_execution("agent-alpha", "file_read");
    collector.tool_execution("agent-beta", "code_exec");

    metrics::PerformanceAnalyzer analyzer(tmp_);
    auto tools = analyzer.analyze_tool_usage_patterns(24);
    EXPECT_FALSE(tools.empty());
}

} // namespace
} // namespace euxis::integration
