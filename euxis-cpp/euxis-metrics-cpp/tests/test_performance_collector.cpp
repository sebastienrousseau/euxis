#include <gtest/gtest.h>
#include <filesystem>

#include "euxis/metrics/performance_collector.hpp"

namespace euxis::metrics {
namespace {

class PerfCollectorTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_;
    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() / "euxis_perf_test";
        std::filesystem::remove_all(tmp_);
    }
    void TearDown() override { std::filesystem::remove_all(tmp_); }
};

TEST_F(PerfCollectorTest, TaskStartedReturnsCorrelationId) {
    PerformanceMetricsCollector c(tmp_);
    auto cid = c.task_started("agent-1", "session-1");
    EXPECT_FALSE(cid.empty());
    EXPECT_TRUE(cid.starts_with("metrics-"));
}

TEST_F(PerfCollectorTest, TaskCompletedWritesSession) {
    PerformanceMetricsCollector c(tmp_);
    c.task_started("agent-1", "session-1");
    c.task_completed("session-1");
    EXPECT_TRUE(std::filesystem::exists(tmp_ / "sessions.jsonl"));
}

TEST_F(PerfCollectorTest, TaskFailedEmitsEvent) {
    PerformanceMetricsCollector c(tmp_);
    c.task_started("agent-1", "session-1");
    c.task_failed("session-1", {.failure_reason = "timeout"});
    EXPECT_TRUE(std::filesystem::exists(tmp_ / "events.jsonl"));
}

TEST_F(PerfCollectorTest, UnknownSessionIgnored) {
    PerformanceMetricsCollector c(tmp_);
    c.task_completed("nonexistent");
    // Should not throw
}

TEST_F(PerfCollectorTest, DelegationRoundtrip) {
    PerformanceMetricsCollector c(tmp_);
    auto cid = c.delegation_started("agent-1", "agent-2");
    c.delegation_completed(cid, "agent-1",
                           DelegationContext{.target_agent = "agent-2",
                                             .total_duration_ms = 100,
                                             .handoff_successful = true,
                                             .quality_score = std::nullopt});
    EXPECT_TRUE(std::filesystem::exists(tmp_ / "events.jsonl"));
}

TEST_F(PerfCollectorTest, ToolExecution) {
    PerformanceMetricsCollector c(tmp_);
    c.tool_execution("agent-1", "grep",
                     ToolExecutionContext{.execution_duration_ms = 50,
                                          .success = true,
                                          .retries = 0,
                                          .correlation_id = std::nullopt});
    EXPECT_TRUE(std::filesystem::exists(tmp_ / "events.jsonl"));
}

} // namespace
} // namespace euxis::metrics
