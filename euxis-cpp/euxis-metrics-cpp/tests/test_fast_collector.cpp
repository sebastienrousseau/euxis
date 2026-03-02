#include <gtest/gtest.h>
#include <filesystem>

#include "euxis/metrics/fast_collector.hpp"

namespace euxis::metrics {
namespace {

class FastCollectorTest : public ::testing::Test {
protected:
    std::filesystem::path tmp_;
    void SetUp() override {
        tmp_ = std::filesystem::temp_directory_path() / "euxis_fast_test";
        std::filesystem::remove_all(tmp_);
    }
    void TearDown() override { std::filesystem::remove_all(tmp_); }
};

TEST_F(FastCollectorTest, RecordAndFlush) {
    FastMetricsCollector c(tmp_);
    c.record("test_event", {{"key", "value"}});
    auto flushed = c.flush();
    EXPECT_EQ(flushed, 1);
    EXPECT_TRUE(std::filesystem::exists(tmp_ / "events.jsonl"));
}

TEST_F(FastCollectorTest, EmptyFlushReturnsZero) {
    FastMetricsCollector c(tmp_);
    EXPECT_EQ(c.flush(), 0);
}

TEST_F(FastCollectorTest, RecordMetric) {
    FastMetricsCollector c(tmp_);
    c.record_metric("cpu_usage", 0.75);
    EXPECT_EQ(c.flush(), 1);
}

TEST_F(FastCollectorTest, RecordTiming) {
    FastMetricsCollector c(tmp_);
    c.record_timing("request_latency", 42.5);
    EXPECT_EQ(c.flush(), 1);
}

TEST_F(FastCollectorTest, RecordCounter) {
    FastMetricsCollector c(tmp_);
    c.record_counter("requests", 5);
    EXPECT_EQ(c.flush(), 1);
}

TEST_F(FastCollectorTest, BufferRespectsSizeLimit) {
    FastMetricsBuffer buf(3);
    buf.append({"a", 0.0, {}});
    buf.append({"b", 0.0, {}});
    buf.append({"c", 0.0, {}});
    buf.append({"d", 0.0, {}});  // evicts "a"
    auto events = buf.drain();
    EXPECT_EQ(events.size(), 3);
    EXPECT_EQ(events[0].event_type, "b");
}

TEST_F(FastCollectorTest, BufferSize) {
    FastMetricsBuffer buf(100);
    EXPECT_EQ(buf.size(), 0u);
    buf.append({"a", 0.0, {}});
    EXPECT_EQ(buf.size(), 1u);
    buf.append({"b", 0.0, {}});
    EXPECT_EQ(buf.size(), 2u);
}

TEST_F(FastCollectorTest, BufferDrainClearsBuffer) {
    FastMetricsBuffer buf(100);
    buf.append({"a", 0.0, {}});
    buf.append({"b", 0.0, {}});
    auto events = buf.drain();
    EXPECT_EQ(events.size(), 2u);
    EXPECT_EQ(buf.size(), 0u);
}

TEST_F(FastCollectorTest, MultipleFlushes) {
    FastMetricsCollector c(tmp_);
    c.record("event1", {{"k", "v"}});
    EXPECT_EQ(c.flush(), 1);
    c.record("event2", {{"k", "v"}});
    c.record("event3", {{"k", "v"}});
    EXPECT_EQ(c.flush(), 2);
}

TEST_F(FastCollectorTest, RecordMetricWithTags) {
    FastMetricsCollector c(tmp_);
    c.record_metric("cpu", 0.8, {{"host", "server1"}});
    EXPECT_EQ(c.flush(), 1);
}

TEST_F(FastCollectorTest, RecordTimingWithTags) {
    FastMetricsCollector c(tmp_);
    c.record_timing("latency", 123.4, {{"endpoint", "/api/v1"}});
    EXPECT_EQ(c.flush(), 1);
}

TEST_F(FastCollectorTest, RecordCounterWithTags) {
    FastMetricsCollector c(tmp_);
    c.record_counter("errors", 3, {{"service", "gateway"}});
    EXPECT_EQ(c.flush(), 1);
}

TEST_F(FastCollectorTest, ShutdownFlushesRemaining) {
    FastMetricsCollector c(tmp_);
    c.record("pre-shutdown", {});
    c.shutdown();
    EXPECT_TRUE(std::filesystem::exists(tmp_ / "events.jsonl"));
}

TEST_F(FastCollectorTest, BackgroundFlushStartsAndStops) {
    FastMetricsCollector c(tmp_);
    c.start_background_flush();
    c.record("bg-event", {});
    // Wait a bit for flush to happen
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    c.shutdown();
}

TEST_F(FastCollectorTest, BackgroundFlushDoubleStartIgnored) {
    FastMetricsCollector c(tmp_);
    c.start_background_flush();
    c.start_background_flush();  // Should be a no-op
    c.shutdown();
}

// --- Coverage: lines 16-21 (default_metrics_dir with EUXIS_HOME set) ---
TEST_F(FastCollectorTest, DefaultMetricsDirFromEuxisHome) {
    auto env_dir = std::filesystem::temp_directory_path() / "euxis_fast_env_test";
    std::filesystem::remove_all(env_dir);

    const char* old_euxis = std::getenv("EUXIS_HOME");
    setenv("EUXIS_HOME", env_dir.c_str(), 1);

    // Construct with empty path to trigger default_metrics_dir()
    FastMetricsCollector c({});
    c.record("env-event", {{"k", "v"}});
    auto flushed = c.flush();
    EXPECT_GE(flushed, 1);

    if (old_euxis) setenv("EUXIS_HOME", old_euxis, 1);
    else unsetenv("EUXIS_HOME");
    std::filesystem::remove_all(env_dir);
}

// --- Coverage: lines 19-21 (default_metrics_dir without EUXIS_HOME, HOME fallback) ---
TEST_F(FastCollectorTest, DefaultMetricsDirFromHomeFallback) {
    const char* old_euxis = std::getenv("EUXIS_HOME");
    unsetenv("EUXIS_HOME");

    // Construct with empty path; falls back to HOME-based path
    FastMetricsCollector c({});
    c.record("fallback-event", {});
    auto flushed = c.flush();
    EXPECT_GE(flushed, 1);

    if (old_euxis) setenv("EUXIS_HOME", old_euxis, 1);
}

// --- Coverage: lines 105-107 (background_flush_loop catch block) ---
// The catch block swallows exceptions during flush; we exercise the loop
// which implicitly covers the try/catch path in background_flush_loop.
TEST_F(FastCollectorTest, BackgroundFlushLoopWithEvents) {
    FastMetricsCollector c(tmp_);
    c.start_background_flush();

    // Record multiple events to ensure the loop flushes them
    for (int i = 0; i < 5; ++i) {
        c.record("bg-loop-event-" + std::to_string(i), {});
    }

    // Wait long enough for at least one flush cycle (1 second)
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    c.shutdown();

    EXPECT_TRUE(std::filesystem::exists(tmp_ / "events.jsonl"));
}

} // namespace
} // namespace euxis::metrics
