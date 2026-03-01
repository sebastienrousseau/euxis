#pragma once

#include <atomic>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "types.hpp"

namespace euxis::metrics {

class FastMetricsBuffer {
public:
    explicit FastMetricsBuffer(size_t max_size = 10000);
    void append(MetricEvent event);
    auto drain() -> std::vector<MetricEvent>;
    [[nodiscard]] auto size() const -> size_t;

private:
    std::deque<MetricEvent> buffer_;
    size_t max_size_;
    mutable std::mutex mutex_;
};

class FastMetricsCollector {
public:
    explicit FastMetricsCollector(const std::filesystem::path& metrics_dir = {});
    ~FastMetricsCollector();

    FastMetricsCollector(const FastMetricsCollector&) = delete;
    FastMetricsCollector& operator=(const FastMetricsCollector&) = delete;

    void record(const std::string& event_type,
                const nlohmann::json& properties = {});
    void record_metric(const std::string& name, double value,
                       const nlohmann::json& tags = {});
    void record_timing(const std::string& name, double duration_ms,
                       const nlohmann::json& tags = {});
    void record_counter(const std::string& name, int delta = 1,
                        const nlohmann::json& tags = {});

    auto flush() -> int;
    void start_background_flush();
    void shutdown();

private:
    std::filesystem::path events_file_;
    FastMetricsBuffer buffer_;
    std::jthread flush_thread_;
    std::atomic<bool> shutdown_{false};

    void background_flush_loop(std::stop_token stop);
    void write_jsonl(const std::vector<MetricEvent>& events);
};

} // namespace euxis::metrics
