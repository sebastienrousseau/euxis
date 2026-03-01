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

} // namespace
} // namespace euxis::metrics
