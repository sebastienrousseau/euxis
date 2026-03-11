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

TEST_F(PerfCollectorTest, MemoryOperation) {
    PerformanceMetricsCollector c(tmp_);
    c.memory_operation("agent-1", "store", "cortex", 50);
    EXPECT_TRUE(std::filesystem::exists(tmp_ / "events.jsonl"));
}

TEST_F(PerfCollectorTest, MemoryOperationWithCorrelationId) {
    PerformanceMetricsCollector c(tmp_);
    c.memory_operation("agent-1", "retrieve", "long_term", 25, "custom-cid-123");
    EXPECT_TRUE(std::filesystem::exists(tmp_ / "events.jsonl"));
}

TEST_F(PerfCollectorTest, ReflexionGenerated) {
    PerformanceMetricsCollector c(tmp_);
    c.reflexion_generated("agent-1", "timeout_error", true);
    EXPECT_TRUE(std::filesystem::exists(tmp_ / "events.jsonl"));
}

TEST_F(PerfCollectorTest, ReflexionGeneratedWithCorrelationId) {
    PerformanceMetricsCollector c(tmp_);
    c.reflexion_generated("agent-1", "logic_error", false, "custom-cid-456");
    EXPECT_TRUE(std::filesystem::exists(tmp_ / "events.jsonl"));
}

TEST_F(PerfCollectorTest, CleanupStaleSessions) {
    PerformanceMetricsCollector c(tmp_);
    // Start a session and immediately try cleanup with 0 timeout
    c.task_started("agent-1", "stale-session-1");
    // Use a very short timeout (0 seconds) so the session is considered stale
    auto cleaned = c.cleanup_stale_sessions(0);
    EXPECT_GE(cleaned, 1);
}

TEST_F(PerfCollectorTest, CleanupNoStaleSessions) {
    PerformanceMetricsCollector c(tmp_);
    // No sessions -> cleanup returns 0
    auto cleaned = c.cleanup_stale_sessions(3600);
    EXPECT_EQ(cleaned, 0);
}

TEST_F(PerfCollectorTest, TaskStartedWithContext) {
    PerformanceMetricsCollector c(tmp_);
    TaskStartContext ctx{
        .task_type = "code_review",
        .priority = "P1",
        .task_complexity = "high",
        .parent_session = "parent-session-1",
    };
    auto cid = c.task_started("agent-1", "session-ctx", ctx);
    EXPECT_FALSE(cid.empty());
    EXPECT_TRUE(cid.starts_with("metrics-"));
}

TEST_F(PerfCollectorTest, TaskFailedWithContext) {
    PerformanceMetricsCollector c(tmp_);
    c.task_started("agent-1", "fail-session");
    TaskFailureContext ctx{
        .failure_reason = "external_service_error",
        .error_category = "external",
        .partial_completion = true,
        .reflexion_generated = true,
    };
    c.task_failed("fail-session", ctx);
    EXPECT_TRUE(std::filesystem::exists(tmp_ / "events.jsonl"));
}

TEST_F(PerfCollectorTest, TaskFailedUnknownSessionIgnored) {
    PerformanceMetricsCollector c(tmp_);
    c.task_failed("nonexistent-session", {.failure_reason = "test"});
    // Should not crash
}

TEST_F(PerfCollectorTest, DelegationWithQualityScore) {
    PerformanceMetricsCollector c(tmp_);
    auto cid = c.delegation_started("agent-1", "agent-2", "expertise", "P1");
    c.delegation_completed(cid, "agent-1",
                           DelegationContext{.target_agent = "agent-2",
                                             .total_duration_ms = 200,
                                             .handoff_successful = true,
                                             .quality_score = 0.95});
    EXPECT_TRUE(std::filesystem::exists(tmp_ / "events.jsonl"));
}

TEST_F(PerfCollectorTest, ToolExecutionWithRetries) {
    PerformanceMetricsCollector c(tmp_);
    c.tool_execution("agent-1", "file_search",
                     ToolExecutionContext{.execution_duration_ms = 150,
                                          .success = false,
                                          .retries = 3,
                                          .correlation_id = "tool-cid-789"});
    EXPECT_TRUE(std::filesystem::exists(tmp_ / "events.jsonl"));
}

TEST_F(PerfCollectorTest, TaskCompletedWithFullContext) {
    PerformanceMetricsCollector c(tmp_);
    c.task_started("agent-1", "complete-session");
    TaskCompletionContext ctx{
        .status = "SUCCESS",
        .artifacts_created = 3,
        .cortex_operations = 5,
        .tool_calls_count = 10,
        .handoff_required = true,
        .delegation_count = 2,
    };
    c.task_completed("complete-session", ctx);
    EXPECT_TRUE(std::filesystem::exists(tmp_ / "sessions.jsonl"));
}

TEST_F(PerfCollectorTest, MultipleSessionsLifecycle) {
    PerformanceMetricsCollector c(tmp_);
    auto cid1 = c.task_started("agent-1", "session-1");
    auto cid2 = c.task_started("agent-2", "session-2");
    EXPECT_NE(cid1, cid2);

    c.task_completed("session-1");
    c.task_failed("session-2", {.failure_reason = "test"});

    EXPECT_TRUE(std::filesystem::exists(tmp_ / "events.jsonl"));
    EXPECT_TRUE(std::filesystem::exists(tmp_ / "sessions.jsonl"));
}

// --- Coverage: lines 29-34 (default_metrics_dir with EUXIS_HOME set) ---
TEST_F(PerfCollectorTest, DefaultMetricsDirFromEuxisHome) {
    auto env_dir = std::filesystem::temp_directory_path() / "euxis_perf_env_test";
    std::filesystem::remove_all(env_dir);

    const char* old_euxis = std::getenv("EUXIS_HOME");
    setenv("EUXIS_HOME", env_dir.c_str(), 1);

    // Construct with empty path to trigger default_metrics_dir()
    PerformanceMetricsCollector c({});
    auto cid = c.task_started("agent-env", "session-env");
    EXPECT_FALSE(cid.empty());

    if (old_euxis) setenv("EUXIS_HOME", old_euxis, 1);
    else unsetenv("EUXIS_HOME");
    std::filesystem::remove_all(env_dir);
}

// --- Coverage: lines 32-34 (default_metrics_dir without EUXIS_HOME) ---
TEST_F(PerfCollectorTest, DefaultMetricsDirFromHomeFallback) {
    const char* old_euxis = std::getenv("EUXIS_HOME");
    unsetenv("EUXIS_HOME");

    PerformanceMetricsCollector c({});
    auto cid = c.task_started("agent-fb", "session-fb");
    EXPECT_FALSE(cid.empty());

    if (old_euxis) setenv("EUXIS_HOME", old_euxis, 1);
}

// --- Coverage: line 255 (cleanup_stale_sessions calls task_failed on stale sessions) ---
TEST_F(PerfCollectorTest, CleanupStaleSessionsCallsTaskFailed) {
    PerformanceMetricsCollector c(tmp_);
    c.task_started("agent-1", "stale-1");
    c.task_started("agent-2", "stale-2");

    // Use timeout of 0 so all sessions are immediately stale
    auto cleaned = c.cleanup_stale_sessions(0);
    EXPECT_EQ(cleaned, 2);

    // Events file should contain TaskFailed events from the cleanup
    EXPECT_TRUE(std::filesystem::exists(tmp_ / "events.jsonl"));
}

} // namespace
} // namespace euxis::metrics
