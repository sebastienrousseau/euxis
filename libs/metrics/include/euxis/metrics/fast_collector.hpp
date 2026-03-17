/// @file
/// @brief High-performance non-blocking metrics collection.
#pragma once

#include <atomic>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

namespace euxis::metrics {

/// @brief A single data point in the metrics stream.
struct MetricEvent {
    std::string event_type;
    double timestamp;
    nlohmann::json properties;
};

/// @brief Thread-safe ring buffer for staging metric events.
class FastMetricsBuffer {
public:
    /// @brief Construct buffer with a maximum event capacity.
    explicit FastMetricsBuffer(size_t max_size = 1000);
    
    /// @brief Append an event to the buffer.
    void append(MetricEvent event);
    
    /// @brief remove and return all events from the buffer.
    auto drain() -> std::vector<MetricEvent>;
    
    /// @brief current number of events in buffer.
    auto size() const -> size_t;

private:
    size_t max_size_;
    std::deque<MetricEvent> buffer_;
    mutable std::mutex mutex_;
};

/// @brief collector that uses a background thread to persist staged metrics.
class FastMetricsCollector {
public:
    /// @brief Construct collector targeting a specific directory.
    explicit FastMetricsCollector(
        const std::filesystem::path& metrics_dir = {});
    ~FastMetricsCollector();

    /// @brief Record a raw event.
    void record(const std::string& event_type, const nlohmann::json& properties);
    
    /// @brief Record a gauge metric.
    void record_metric(const std::string& name, double value,
                       const nlohmann::json& tags = {});
    
    /// @brief Record a duration/timing metric.
    void record_timing(const std::string& name, double duration_ms,
                       const nlohmann::json& tags = {});
    
    /// @brief Record an incrementing counter.
    void record_counter(const std::string& name, int delta = 1,
                        const nlohmann::json& tags = {});

    /// @brief Synchronously flush all buffered metrics to disk.
    auto flush() -> int;
    
    /// @brief Spawn background thread for periodic flushing.
    void start_background_flush();
    
    /// @brief stop background threads and flush remaining data.
    void shutdown();

private:
    FastMetricsBuffer buffer_;
    std::filesystem::path events_file_;
    std::jthread flush_thread_;
    std::atomic<bool> shutdown_{false};

    void background_flush_loop(std::stop_token stop);
    void write_jsonl(const std::vector<MetricEvent>& events);
};

} // namespace euxis::metrics
