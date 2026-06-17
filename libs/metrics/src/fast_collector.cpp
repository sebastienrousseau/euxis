#include "euxis/metrics/fast_collector.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>

namespace euxis::metrics {
namespace {

auto now_epoch() -> double {
    return std::chrono::duration<double>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

auto default_metrics_dir() -> std::filesystem::path {
    const char* home = std::getenv("EUXIS_HOME");
    if (home) return std::filesystem::path(home) / "data/runtime" / "metrics";
    const char* user_home = std::getenv("HOME");
    return std::filesystem::path(user_home ? user_home : "/tmp") /
           ".euxis" / "data/runtime" / "metrics";
}

} // namespace

FastMetricsBuffer::FastMetricsBuffer(size_t max_size) : max_size_(max_size) {}

void FastMetricsBuffer::append(MetricEvent event) {
    std::lock_guard lock(mutex_);
    if (buffer_.size() >= max_size_) buffer_.pop_front();
    buffer_.push_back(std::move(event));
}

auto FastMetricsBuffer::drain() -> std::vector<MetricEvent> {
    std::lock_guard lock(mutex_);
    std::vector<MetricEvent> events(buffer_.begin(), buffer_.end());
    buffer_.clear();
    return events;
}

auto FastMetricsBuffer::size() const -> size_t {
    std::lock_guard lock(mutex_);
    return buffer_.size();
}

FastMetricsCollector::FastMetricsCollector(
    const std::filesystem::path& metrics_dir) {
    auto dir = metrics_dir.empty() ? default_metrics_dir() : metrics_dir;
    std::filesystem::create_directories(dir);
    events_file_ = dir / "events.jsonl";
}

FastMetricsCollector::~FastMetricsCollector() { shutdown(); }

void FastMetricsCollector::record(const std::string& event_type,
                                  const nlohmann::json& properties) {
    buffer_.append({event_type, now_epoch(), properties});
}

void FastMetricsCollector::record_metric(const std::string& name, double value,
                                         const nlohmann::json& tags) {
    record("metric", {{"name", name}, {"value", value}, {"tags", tags}});
}

void FastMetricsCollector::record_timing(const std::string& name,
                                         double duration_ms,
                                         const nlohmann::json& tags) {
    record("timing",
           {{"name", name}, {"duration_ms", duration_ms}, {"tags", tags}});
}

void FastMetricsCollector::record_counter(const std::string& name, int delta,
                                          const nlohmann::json& tags) {
    record("counter", {{"name", name}, {"delta", delta}, {"tags", tags}});
}

auto FastMetricsCollector::flush() -> int {
    auto events = buffer_.drain();
    if (events.empty()) return 0;
    write_jsonl(events);
    return static_cast<int>(events.size());
}

void FastMetricsCollector::start_background_flush() {
    if (flush_thread_.joinable()) return;
    flush_thread_ = std::jthread([this](std::stop_token stop) {
        background_flush_loop(stop);
    });
}

void FastMetricsCollector::shutdown() {
    shutdown_.store(true);
    if (flush_thread_.joinable()) {
        flush_thread_.request_stop();
        flush_thread_.join();
    }
    flush(); // Final drain
}

void FastMetricsCollector::background_flush_loop(std::stop_token stop) {
    while (!stop.stop_requested() && !shutdown_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        try {
            flush();
        } catch (const std::exception&) {
            // Don't let flush errors break the loop
            (void)0;  // swallowed: best-effort
        }
    }
}

void FastMetricsCollector::write_jsonl(
    const std::vector<MetricEvent>& events) {
    std::ofstream f(events_file_, std::ios::app);
    for (const auto& e : events) {
        nlohmann::json j = {
            {"event_type", e.event_type},
            {"timestamp", e.timestamp},
            {"properties", e.properties},
        };
        f << j.dump() << '\n';
    }
}

} // namespace euxis::metrics
